/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
/**
 * @file SeqLAsiaLineBMSSCharDecoder.cpp
 * @author hanjunyu(com@baidu.com)
 * @date 2015/03/22 16:06:01
 * revise xieshufu
 * @data 2016/9/8
 * @brief 
 *  
 **/

#include "seql_asia_line_bmss_word_decoder.h"
#include "dll_main.h"
#include "common_func.h"

extern char g_pimage_title[256];
extern char g_psave_dir[256];

CSeqLAsiaLineBMSSWordDecoder::CSeqLAsiaLineBMSSWordDecoder(){
    _m_remove_head_end_noise = false;
    _m_save_low_prob_flag = false;
    _m_resnet_strategy_flag = false;
}

void CSeqLAsiaLineBMSSWordDecoder::set_remove_head_end_noise(bool flag) {
    _m_remove_head_end_noise = flag;
}

void CSeqLAsiaLineBMSSWordDecoder::set_resnet_strategy_flag(bool flag) {
    _m_resnet_strategy_flag = flag;
}
// function
// decode one line
SSeqLEngLineInfor CSeqLAsiaLineBMSSWordDecoder::decode_line(\
        void *recognizer, SSeqLEngLineInfor &inputLine) {

    _m_input_line = inputLine;
    line_word_decode();

    return _m_output_line;
}
    
int CSeqLAsiaLineBMSSWordDecoder::set_save_low_prob_flag(bool b_save_low_prob_flag) {
    _m_save_low_prob_flag = b_save_low_prob_flag;
    return 0;
}

int compute_word_ratio(SSeqLEngLineInfor *pOutputLine, double &dValidCharRatio)  {
    int iret = 0;
    int ivalid_char_num = 0;
    int iword_vec_len = pOutputLine->wordVec_.size();

    for (int idx = 0; idx < iword_vec_len; idx++)  {
        SSeqLEngWordInfor *pword = &(pOutputLine->wordVec_[idx]);
        if (pword->reg_prob >= 0.65) {
            ivalid_char_num++;
        }
    }
    if (iword_vec_len == 1) {
        SSeqLEngWordInfor *pword = &(pOutputLine->wordVec_[0]);
        if (pword->reg_prob < 0.7) {
            ivalid_char_num = 0; 
        }
    } 
    dValidCharRatio = (double)ivalid_char_num / iword_vec_len;

    return iret;
}

// reject the word with low confidence value
int  CSeqLAsiaLineBMSSWordDecoder::refine_line_word(std::vector<SSeqLEngWordInfor> &InwordVec_)  {
    int iret = 0;
    int iword_vec_len = InwordVec_.size();
    if (iword_vec_len == 0) {
        return 0;
    }

    // refine the word position
    for (unsigned int i = 0; i < InwordVec_.size(); i++) {
        SSeqLEngWordInfor *pword = &(InwordVec_[i]);
        if (pword->iLcType != 0) {
            double daspect_ratio = (double)pword->wid_ / pword->hei_;
            if (daspect_ratio < 0.5) {
                int icenter = pword->left_ + pword->wid_ / 2;
                pword->left_ = std::max(icenter - pword->hei_ / 4, 0);
                pword->wid_ = pword->hei_ / 2;
            }
            // refine the candidate character position
            for (int j = 0; j < pword->cand_word_vec.size(); j++) {
                pword->cand_word_vec[j].left_ = pword->left_;
                pword->cand_word_vec[j].top_ = pword->top_;
                pword->cand_word_vec[j].wid_ = pword->wid_;
                pword->cand_word_vec[j].hei_ = pword->hei_;
            }
        }
    }

    return iret;
}

//remove the character which are overlapped together
int CSeqLAsiaLineBMSSWordDecoder::remove_overlap_word(
    std::vector<SSeqLEngWordInfor> &in_word_vec)  {
    int ret_val = 0;

    int word_num = in_word_vec.size();
    if (word_num <= 1) {
        return ret_val;
    }
   
    std::vector<int> word_flag_vec;
    for (int i = 0; i < word_num; i++) {
        word_flag_vec.push_back(1);
    }
    for (int i = 0; i < word_num; i++) {
        if (word_flag_vec[i] == 0) {
            continue;
        }

        SSeqLEngWordInfor& word_a = in_word_vec[i];
        int word_a_x0 = word_a.left_;
        int word_a_x1 = word_a_x0 + word_a.wid_ - 1;
        for (int j = i + 1; j < word_num; j++) {
            SSeqLEngWordInfor& word_b = in_word_vec[j];
            int word_b_x0 = word_b.left_;
            int word_b_x1 = word_b_x0 + word_b.wid_ - 1;
            if (word_b_x0 > word_a_x1 || word_b_x1 < word_a_x0) {
                continue;
            }
            int x_len = std::max(word_a_x1, word_b_x1) 
                - std::min(word_a_x0, word_b_x0) + 1;
            int x_overlap = word_a.wid_ + word_b.wid_ - x_len;
            float mean_wid_t = (word_a.wid_ + word_b.wid_) / 4.0;
            if (word_a.iLcType == 3 && word_b.iLcType == 3 \
                && word_a.wordStr_ == word_b.wordStr_) {
                mean_wid_t = 1;
            } 
            if (x_overlap > mean_wid_t) {
                // preserve the word with high confidence  
                if (word_a.regLogProb_ >= word_b.regLogProb_) {
                    word_flag_vec[j] = 0;
                } else {
                    word_flag_vec[i] = 0;
                }
            }
        }
    }

    std::vector<SSeqLEngWordInfor> temp_word_vec = in_word_vec;
    in_word_vec.clear();
    for (int i = 0; i < word_num; i++) {
        if (word_flag_vec[i] == 0) {
            continue ;
        }
        SSeqLEngWordInfor word = temp_word_vec[i];
        in_word_vec.push_back(word);
    }
    return ret_val;
}

int  CSeqLAsiaLineBMSSWordDecoder::check_valid_line(SSeqLEngLineInfor &inputLine,\
        int &iValid)  {
    int iret = 0;
    double dvalid_char_ratio = 1.0;

    iValid = 1;
    compute_word_ratio(&inputLine, dvalid_char_ratio);
    
    const double valid_thresh = 0.3; // default=0.3
    if (dvalid_char_ratio < valid_thresh) {
        iValid = 0;
    }

    return iret;
}

// fill the char information for each word
void CSeqLAsiaLineBMSSWordDecoder::fill_char_infor(std::vector<SSeqLEngWordInfor> &InwordVec_) {
    int iword_vec_len = InwordVec_.size();
    if (iword_vec_len == 0) {
        return ;
    }

    for (int idx = 0; idx < iword_vec_len; idx++) {
        SSeqLEngWordInfor *pword = &(InwordVec_[idx]);
        SSeqLEngCharInfor char_infor;
        char_infor.left_ = pword->left_;
        char_infor.top_ = pword->top_;
        char_infor.wid_ = pword->wid_;
        char_infor.hei_ = pword->hei_;
        snprintf(char_infor.charStr_, 4, "%s", pword->wordStr_.data());
        pword->charVec_.push_back(char_infor);
    }

    return ;
}
        
// new decode function 
// only the recognition confidence of characters are used
int CSeqLAsiaLineBMSSWordDecoder::line_word_decode() {
    int ret_val = 0;

    _m_output_line.left_ = _m_input_line.left_;
    _m_output_line.top_ = _m_input_line.top_;
    _m_output_line.wid_ = _m_input_line.wid_;
    _m_output_line.hei_ = _m_input_line.hei_;
    _m_output_line.imgWidth_ = _m_input_line.imgWidth_;
    _m_output_line.imgHeight_ = _m_input_line.imgHeight_;
    _m_output_line.isBlackWdFlag_ = _m_input_line.isBlackWdFlag_;

    // get the word information from the recognition lines
    std::vector<SSeqLEngWordInfor> & in_word_vec = _m_input_line.wordVec_;
    int word_num = in_word_vec.size();
    std::vector<int> word_flag_vec;

    for (int i = 0; i < word_num; i++) {
        word_flag_vec.push_back(1);
    }

    for (int i = 0; i < word_num; i++) {
        if (word_flag_vec[i] == 0) {
            continue;
        }

        SSeqLEngWordInfor& word_a = in_word_vec[i];
        int word_a_x0 = word_a.left_;
        int word_a_x1 = word_a_x0 + word_a.wid_ - 1;
        for (int j = i + 1; j < word_num; j++) {
            SSeqLEngWordInfor& word_b = in_word_vec[j];
            int word_b_x0 = word_b.left_;
            int word_b_x1 = word_b_x0 + word_b.wid_ - 1;
            if (word_b_x0 > word_a_x1 || word_b_x1 < word_a_x0) {
                continue;
            }
            int x_len = std::max(word_a_x1, word_b_x1) 
                - std::min(word_a_x0, word_b_x0) + 1;
            int x_overlap = word_a.wid_ + word_b.wid_ - x_len;
            float mean_wid_t = (word_a.wid_ + word_b.wid_) / 4.0;
            if (word_a.iLcType == 3 && word_b.iLcType == 3) {
                mean_wid_t = 1;
            }
            if (!_m_resnet_strategy_flag) {
                if (x_overlap > mean_wid_t) {
                    // preserve the word with high confidence  
                    if (word_a.regLogProb_ >= word_b.regLogProb_) {
                        word_flag_vec[j] = 0;
                    } else {
                        word_flag_vec[i] = 0;
                    }
                }
            }
        }
    }
    // 
    const double min_char_thresh = 0.35; //-1.0;
    const double first_end_thresh = 0.9; //-0.1;
    int i_st = 0;
    int i_end = word_num - 1;

    if (_m_remove_head_end_noise) {
        // lx added, to remove the noise in the head and end
        const double first_end_noise_thresh = -0.1;
        for (int i = 0; i < word_num; i++) {
            bool is_chn = false;
            check_chn(in_word_vec[i].wordStr_, is_chn);
            if (is_chn || (in_word_vec[i].regLogProb_ > first_end_noise_thresh)) {
                i_st = i;
                /*std::cout << in_word_vec[i].iLcType << " , " <<
                    in_word_vec[i].regLogProb_ << " , " <<
                    in_word_vec[i].wordStr_ << std::endl;*/
                break;
            }
        }
        for (int i = word_num - 1; i >= i_st; i--) {
            bool is_chn = false;
            check_chn(in_word_vec[i].wordStr_, is_chn);
            if (is_chn || (in_word_vec[i].regLogProb_ > first_end_noise_thresh)) {
                /*std::cout << in_word_vec[i].iLcType << " , " <<
                    in_word_vec[i].regLogProb_ << " , " <<
                    in_word_vec[i].wordStr_ << std::endl;*/
                i_end = i;
                break;
            }
        }
    }
    
    if (_m_remove_head_end_noise && i_st > i_end) {
        return -1;
    }

    for (int i = i_st; i < i_end + 1; i++) {
        if (word_flag_vec[i] == 0) {
            continue ;
        }
        
        //_m_save_low_prob_flag=true: preserve the char with low probability
        //_m_save_low_prob_flag=false: do not preserve the char with low probability
        if (!_m_save_low_prob_flag) {
            if (in_word_vec[i].reg_prob < min_char_thresh) {
                continue ;
            }
            // check the first or the last character in this line
            // if the confidence is not high, omit this character
            if ((i == 0 || i == word_num - 1) &&
                in_word_vec[i].iLcType == 3 && !_m_resnet_strategy_flag &&
                (in_word_vec[i].reg_prob < first_end_thresh)) {
                continue ;
            }
            else if ((i == 0 || i == word_num - 1) &&
                    in_word_vec[i].isPunc_ == true && _m_resnet_strategy_flag &&
                    (in_word_vec[i].reg_prob < first_end_thresh)) {
                    continue ;
            }
        }
        
        SSeqLEngWordInfor word = in_word_vec[i];
        int high_low_type_ =  word.highLowType_;          
        if (high_low_type_ == 1) {
            string strVec = word.wordStr_;
            transform(strVec.begin(), strVec.end(), strVec.begin(), ::toupper);
            word.wordStr_ = strVec;
        }
        _m_output_line.wordVec_.push_back(word);
    }
    
    // check the width of Chinese character
    for (int i = 0; i < word_num; i++) {
        if (word_flag_vec[i] == 0 || in_word_vec[i].iLcType != 0) {
            continue ;
        }
        float aspect_ratio = (float)in_word_vec[i].wid_ / in_word_vec[i].hei_;
        if (aspect_ratio <= 0.2) {
            word_flag_vec[i] = 0;
        }
    }
        
    int ivalid = 0;
    check_valid_line(_m_output_line, ivalid);

    if (ivalid == 0) {
        _m_output_line.wordVec_.clear();
        ret_val = -1;
    } else {
        refine_line_word(_m_output_line.wordVec_);
        if (!_m_resnet_strategy_flag) {
            remove_overlap_word(_m_output_line.wordVec_);
        }
        // fill the character information for each chinese word
        fill_char_infor(_m_output_line.wordVec_);
    }

    return ret_val;
}
/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */

