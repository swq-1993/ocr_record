/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
/**
 * @file SeqLAsiaWordRecog.cpp
 * @author hanjunyu(com@baidu.com)
 * @date 2015/03/22 14:24:54
 * @brief 
 *  
 **/
#include "seql_asia_word_recog.h"
#include "common_func.h"
#include "dll_main.h"
#include "predictor/seql_predictor.h"

int g_irct_index = 0;
extern char g_psave_dir[256];
extern char g_pimage_title[256];
extern int g_iline_index;
static pthread_rwlock_t s_rwlock;

void get_duplicate_word_flag(std::vector<SSeqLEngWordInfor> &outVec, int *piFlag) {
    int iword_num = outVec.size();
    int *pidup_index = new int[iword_num];
    int idup_num = 0;
    for (int i = 0; i < iword_num; i++) {
        if (piFlag[i] == 1) {
            continue ;
        }
                
        idup_num = 1;
        pidup_index[0] = i;

        SSeqLEngWordInfor *pa = &(outVec[i]);
        int ia_width = pa->wid_;
        for (int j = i + 1; j < iword_num; j++) {
            SSeqLEngWordInfor *pb = &(outVec[j]);

            if (piFlag[j] == 1 || pa->wordStr_ != pb->wordStr_) {
                continue ;
            }
            int ix0 = std::min(pa->left_, pb->left_);
            int ix1 = std::max(pa->left_ + pa->wid_, pb->left_+pb->wid_);
            int ib_width = pb->wid_;
            int ix_len = ix1 - ix0 + 1;
            int ioverlap = ia_width + ib_width - ix_len;
            int imin_ab_width = std::min(ia_width, ib_width);
            double dratio = (double)ioverlap / imin_ab_width;
            
            // check the overlapping region and the same word
            if (pa->iLcType != 0 && dratio >= 0.0) {
                piFlag[j] = 1;
                pa->bRegFlag = true;
                pidup_index[idup_num] = j;
                idup_num++;
            }
            else if ((pa->iLcType == 0 && dratio >= 0.6)) { 
                piFlag[j] = 1;
                pidup_index[idup_num] = j;
                idup_num++;
            } 
        }
        if (idup_num == 1) {
            continue ;
        }
        double dcenter = 0;
        double dwidth = 0;
        for (int j = 0; j < idup_num; j++)  {
            int index = pidup_index[j];
            dcenter += outVec[index].left_ + outVec[index].wid_ / 2.0;
            dwidth += outVec[index].wid_;
            pa->regLogProb_ = std::max(pa->regLogProb_, outVec[index].regLogProb_) / idup_num; 
        }
        dcenter /= idup_num;
        dwidth /= idup_num;
        pa->left_ = (int)(dcenter - dwidth / 2);
        pa->wid_ = (int)dwidth;
        pa->duplicateNum_ = idup_num;
    }
    if (pidup_index) {
        delete []pidup_index;
        pidup_index = NULL;
    }

    return ;
}

void remove_duplicate_word(std::vector<SSeqLEngWordInfor> &outVec)  {
    int iword_num = outVec.size();
    if (iword_num <= 1) {
        return ;
    }
    
    std::vector<SSeqLEngWordInfor> in_vec = outVec;
    int *piflag = new int[iword_num];
    memset(piflag, 0, sizeof(int) * iword_num);

    get_duplicate_word_flag(in_vec, piflag);

    outVec.clear();
    for (int i = 0; i < iword_num; i++)  {
        if (piflag[i] == 0)  {
            outVec.push_back(in_vec[i]);
        }
    }
    if (piflag)  {
        delete []piflag;
        piflag = NULL;
    }

    return ;
}

CSeqLAsiaWordReg::CSeqLAsiaWordReg() {
    seqLCdnnModel_ = NULL;
    seqLCdnnTable_ = NULL;
    _use_paddle = false;
    _use_gpu = false;
    _batch_size = 1;
    _m_recog_thresh = 0;
    _m_openmp_thread_num = 0;
    _m_recog_model_type = 0;
    
    _m_cand_num = -1;
    _m_enhance_model = NULL;
    _m_recog_thresh = 0.0;
    _g_split_length = 3; //number_recog
    _m_resnet_strategy_flag = false;
}

CSeqLAsiaWordReg::~CSeqLAsiaWordReg() {
    _m_cand_num = -1; 
    _m_enhance_model = NULL;
}
CSeqLAsiaWordReg::CSeqLAsiaWordReg(const CSeqLEngWordRegModel &inRegModel) {
    
    seqLCdnnModel_ = inRegModel.seqLCdnnModel_;
    seqLCdnnTable_ = inRegModel.seqLCdnnTable_;
    seqLCdnnMean_  = inRegModel.seqLCdnnMean_;
    seqLNormheight_ = inRegModel.seqLNormheight_;

    tabelStrVec_.clear();
    for (int i = 0; i < inRegModel.tabelStrVec_.size(); i++) {
        tabelStrVec_.push_back(inRegModel.tabelStrVec_[i]);
    }
    tabelLowStrVec_.clear();
    for (int i = 0; i < inRegModel.tabelLowStrVec_.size(); i++) {
        tabelLowStrVec_.push_back(inRegModel.tabelLowStrVec_[i]);
    }
    dataDim_ = inRegModel.dataDim_;
    labelsDim_ = inRegModel.labelsDim_;
    blank_ = inRegModel.blank_;
    _m_cand_num = -1;
    _use_paddle = inRegModel._use_paddle;
    _use_gpu = inRegModel._use_gpu;
    _batch_size = inRegModel._batch_size;
    _m_recog_thresh = 0;
    _m_openmp_thread_num = 0;
    _m_recog_model_type = 0;
    _g_split_length = 3; //number_recog
    return ;
}

void  CSeqLAsiaWordReg::draw_time_line_data(SSeqLEngWordInfor &tempWord,\
        float fExtendRatio, IplImage *pTmpImage)  {

    int word_time_num = tempWord.proMat_.size() / labelsDim_; 
    int ileft = tempWord.left_;
    int itop = tempWord.top_;
    int iwidth = tempWord.wid_;
    int iheight = tempWord.hei_;

    double dnorm_height_ratio = (double)tempWord.iNormHeight_ / 3;
    double dest_rect_w = (double)(tempWord.iNormWidth_ * tempWord.hei_) \
                  / tempWord.iNormHeight_;
    double dresize_ratio = dest_rect_w / tempWord.wid_;
    double dwidth_for_recog_ratio = (double)(word_time_num * dnorm_height_ratio) \
                             / (tempWord.iNormWidth_);
    double drect_width_for_recog = tempWord.wid_ * dwidth_for_recog_ratio \
                            * dresize_ratio;
    double davg_recog_char_width = drect_width_for_recog / \
                            (word_time_num * fExtendRatio); 

    float itime_line_w = davg_recog_char_width;
    for (int j = 0; j < word_time_num; j++) {

        CvPoint point1 = cvPoint(0, 0);
        CvPoint point2 = cvPoint(0, 0);
        CvScalar color = cvScalar(0, 0, 0);
        point1 = cvPoint((int)(ileft + j * itime_line_w), itop);
        if (j < word_time_num - 1) {
            point2 = cvPoint((int)(ileft + (j + 1) * itime_line_w), itop + iheight);
        }
        else {
            point2 = cvPoint(ileft + iwidth, itop + iheight);
        }
        if (j % 2 == 0) {
            color = cvScalar(255, 0, 0);
        }
        else {
            color = cvScalar(0, 255, 0);
        }
        cvRectangle(pTmpImage, point1, point2, color);
    }
    return ;
}

void CSeqLAsiaWordReg::add_word_candidate(std::vector<SSeqLEngWordInfor> &outVec) {
    // we try to get the confidence probability of this character
    int word_candidate_num = 1;
    if (_m_cand_num != -1) {
        word_candidate_num = _m_cand_num;
    } 
    if (word_candidate_num <= 1)  {
        return ;
    }
    
    std::vector<SSeqLEngWordInfor> in_vec = outVec;
    int in_vec_size = in_vec.size();
    for (int i = 0; i < in_vec_size; i++) {
        SSeqLEngWordInfor & new_word = in_vec[i];
        if (new_word.bRegFlag && _m_cand_num == -1) {
            continue ;
        }
        std::vector<float> & per_mat = new_word.proMat_;
        float *pconf_row = &(per_mat[0]);
        float fmax_conf = 0;
        int s = new_word.timeStart_;
        int e = new_word.timeEnd_;
        int icheck_num = e - s + 1;

        int *pisort_idx = new int[word_candidate_num];
        large_value_quick_index(pconf_row, labelsDim_ * icheck_num,\
                word_candidate_num, pisort_idx);
        for (int idx = 0; idx < word_candidate_num; idx++)  {
            pisort_idx[idx] = pisort_idx[idx] % labelsDim_;
        }
        fmax_conf = pconf_row[pisort_idx[0]];
        float fmax_conf2 = pconf_row[pisort_idx[1]];
        double dratio = fmax_conf / (fmax_conf2 + 0.0001);
        
        SSeqLEngWordInfor first_word = new_word;
        // first_word --> converting to the upper case 
        if (first_word.iLcType == 2 && first_word.highLowType_ == 1) {
            std::string strvec = first_word.wordStr_;
            transform(strvec.begin(), strvec.end(), strvec.begin(), ::toupper);
            first_word.wordStr_ = strvec;
        }
        if (_m_cand_num != -1) {
            outVec[i].cand_word_vec.push_back(first_word);
        }

        // the confidence value is reliable
        // default = 5.0
        if (dratio >= 5.0 && _m_cand_num == -1) {
            delete []pisort_idx;
            pisort_idx = NULL;
            continue;
        }
            
        for (int icand_idx = 1; icand_idx < word_candidate_num; icand_idx++) {
            if (pisort_idx[icand_idx] == labelsDim_-1 || \
                    pisort_idx[icand_idx] == pisort_idx[0]) {
                continue ;
            }

            SSeqLEngWordInfor cand_word = new_word;
            std::vector<int> path;
            path.clear();
            path.push_back(pisort_idx[icand_idx]);
            cand_word.pathVec_ = path;
            cand_word.maxVec_ = path;
            
            float fconfident = pconf_row[pisort_idx[icand_idx]];
            cand_word.reg_prob = fconfident; 
            cand_word.regLogProb_ = safeLog(fconfident); 
            cand_word.wordStr_ = path_to_accuratestring(path); 
            //cand_word.wordStr_ = pathToString(path); 
            cand_word.bRegFlag = false;
            int iclass_index = cand_word.maxVec_[0];
            std::string str_label = tabelStrVec_[iclass_index];
            unsigned short icode = seqLCdnnTable_[iclass_index];
            
            bool bnum = false;
            bool benglish = false;
            bool bhigh_low_type = false;
            bool bchn = false;

            check_num_str(str_label, bnum);
            check_eng_str(str_label, benglish, bhigh_low_type);
            check_chn(str_label, bchn);

            cand_word.isPunc_ = check_punc(icode);
            cand_word.iLcType = 0;
            cand_word.highLowType_ = 0;
            if (bnum) {
                cand_word.iLcType = 1;
            } else if (benglish) {
                cand_word.iLcType = 2;
                if (bhigh_low_type) {
                    cand_word.highLowType_ = 1;
                }
            } else if (cand_word.isPunc_ && !_m_resnet_strategy_flag) {
                cand_word.iLcType = 3;
            } else if (!bchn && _m_resnet_strategy_flag) {
                cand_word.iLcType = 3;
            }
            
            cand_word.left_add_space_ = false;
            cand_word.bRegFlag = false;
            cand_word.timeStart_ = s;
            cand_word.timeEnd_ = e;
            cand_word.toBeMaxConf_ = false;
            cand_word.duplicateNum_ = 1;
            outVec[i].cand_word_vec.push_back(cand_word);
        }

        if (pisort_idx) {
            delete []pisort_idx;
            pisort_idx = NULL;
        }
    }
    
    return ;
}

void CSeqLAsiaWordReg::refine_word_position(std::vector<SSeqLEngWordInfor> &outVec,\
        int iStart, int iWordNum, int iLineLeft, int iLineRight)  {
    
    int iend = iStart + iWordNum;

    for (int i = iStart; i < iend; i++) {
        SSeqLEngWordInfor *pword = &(outVec[i]);
        double daspect_ratio = (double)pword->wid_ / pword->hei_;
        if (daspect_ratio>0.75) {
            continue;
        }
        int ileft = 0;
        int iright = 0;
        int icenter = 0;
        int iexd_size = 0;
        int imin_left = 0;
        int imax_right = 0;
        int itime_num = pword->timeEnd_ - pword->timeStart_ + 1;
        
        if (_m_resnet_strategy_flag) {
            if (i < iend - 1) {
                imax_right = outVec[i+1].left_;
            }
            else {
                imax_right = iLineRight;
            }
            if (i == iStart) {
                imin_left = outVec[i].left_;
            }
            else {
                imin_left = std::min(outVec[i].left_, outVec[i-1].left_ + outVec[i-1].wid_);
            }
        } else {
            if (i < iend - 1) {
                imax_right = outVec[i+1].left_-3;
            }
            else {
                imax_right = iLineRight;
            }
            if (i == iStart) {
                imin_left = iLineLeft;
            }
            else {
                imin_left = outVec[i-1].left_ + outVec[i-1].wid_ + 3;
            }
        }
       
        // adjust the word postion according to its type
        // but this strategy is not good and we need to improve it
        // 0: chinese
        // 1: numerical
        // 2: English
        // 3: punctuation 
        
        if (!_m_resnet_strategy_flag) {
            int iword_wid = pword->wid_ / itime_num;
            if (pword->iLcType == 0) {
                icenter = (int)(pword->left_ + iword_wid * 1.4);
                iexd_size = (int)(0.4 * pword->hei_);
            } else if (pword->iLcType == 2) {
                icenter = (int)(pword->left_ + iword_wid * 0.9);
                iexd_size = (int)(pword->hei_ * 0.35);
            } else if (pword->iLcType == 1) {
                icenter = (int)(pword->left_ + iword_wid * 1.0);
                iexd_size = (int)(pword->hei_ * 0.25);
            }  else {
                icenter = (int)(pword->left_ + iword_wid * 1.0);
                iexd_size = (int)(0.6 * pword->wid_);
            }
        } else {
            if (pword->iLcType == 0) {
                icenter = (int)(pword->left_ + pword->wid_);
                iexd_size = (int)(pword->wid_);
            } else {
                continue;
            }
        }

        ileft = std::max(icenter - iexd_size, imin_left);
        iright = std::min(icenter + iexd_size, imax_right);
        pword->left_ = ileft;    
        pword->wid_ = std::max(iright - ileft + 1, 2);
    }

    return;
}

void CSeqLAsiaWordReg::cal_word_infor(SSeqLEngWordInfor &perWord, SSeqLEngRect &newRect,\
                SSeqLEngWordInfor & newWord, int setStartTime, int setEndTime)  {
        
        std::vector<float> & permat = perWord.proMat_;
        int new_start_time = setStartTime;
        int new_end_time = setEndTime;
        int new_time_num = new_end_time - new_start_time + 1 ;

        std::vector<int> max_vec;
        float fconf = 0;
        for (int i = new_start_time; i <= new_end_time; i++)  {
            max_vec.push_back(perWord.maxVec_[i]);

            int index = perWord.maxVec_[i];
            float fout =  permat[i * labelsDim_ + index];
            if (fconf < fout)  {
                fconf = fout;
            }
        }
        newWord.maxVec_ = max_vec;
        newWord.maxStr_ = path_to_string_have_blank(max_vec);
        std::vector<int> path = path_elimination(max_vec, blank_);
        newWord.pathVec_ = path;
        newWord.reg_prob = fconf;
        newWord.regLogProb_ = safeLog(fconf);
        newWord.wordStr_ = path_to_string(path);
        
    
        int show_recog_rst_flag = g_ini_file->get_int_value("SHOW_RECOG_RST",
            IniFile::_s_ini_section_global);
        if (show_recog_rst_flag == 1) {
            std::cout << "[" << newWord.wordStr_ << "," 
                << newWord.regLogProb_ << "," << fconf << "] ";
        }

        int len = labelsDim_ * new_time_num;
        newWord.proMat_.resize(len);
        memcpy(&(newWord.proMat_[0]), \
                &(permat[new_start_time * labelsDim_]), sizeof(float) * len);

        newWord.left_ = newRect.left_;
        newWord.top_  = newRect.top_;
        newWord.wid_  = newRect.wid_;
        newWord.hei_  = newRect.hei_;
        newWord.isPunc_ = false;
        newWord.toBeAdjustCandi_ = true;
        
        return ;
}

void CSeqLAsiaWordReg::split_to_one_word(std::vector<SSeqLEngWordInfor> &inVec,\
        std::vector<SSeqLEngWordInfor> &outVec, float fExtendRatio) {
    
    // xie shufu add
    outVec.clear();
    // refine the information of each character
    int in_vec_size = inVec.size();
    for (int i = 0; i < in_vec_size; i++) {
        SSeqLEngWordInfor & temp_word = inVec[i];
        int word_time_num = temp_word.proMat_.size() / labelsDim_; 

        std::vector<int> start_vec;
        std::vector<int> end_vec;
        std::vector<float> cent_vec;
        estimate_path_pos(temp_word.maxVec_, temp_word.pathVec_,\
                start_vec, end_vec, cent_vec, blank_);
       
        double dnorm_height_ratio = (double)temp_word.iNormHeight_ / _g_split_length;//number_recog
        double dest_rect_w = (double)(temp_word.iNormWidth_ * temp_word.hei_) \
                             / temp_word.iNormHeight_;
        double dresize_ratio = dest_rect_w / temp_word.wid_;
        double dwid_recog_ratio = (double)(word_time_num * dnorm_height_ratio)\
                                   / (temp_word.iNormWidth_);
        double drecog_wid = temp_word.wid_ * dwid_recog_ratio * dresize_ratio;
        double davg_recog_char_w = drecog_wid / (word_time_num * fExtendRatio); 
    
        int iline_left = temp_word.left_;
        int iline_right = temp_word.left_ + temp_word.wid_;
        int istart_vec_size = start_vec.size();

        // compute the position of each character
        for (int j = 0; j < istart_vec_size; j++) {
            int s = start_vec[j];
            int e = end_vec[j];
            int ad_ext_boundary = (int)(-0.0 * temp_word.hei_);

            SSeqLEngRect new_rect;
            new_rect.wid_ = (int)((e - s + 1) * davg_recog_char_w - 2 * ad_ext_boundary);
            new_rect.top_ = temp_word.top_;
            new_rect.hei_ = temp_word.hei_;
            new_rect.left_ = (int)(temp_word.left_ + s * davg_recog_char_w + ad_ext_boundary);

            SSeqLEngWordInfor new_word;
            cal_word_infor(temp_word, new_rect, new_word, s, e);

            // classify the type of the word
            int ivec_idx = start_vec[j];
            int iclass_index = temp_word.maxVec_[ivec_idx];
            std::string str_label = tabelStrVec_[iclass_index];
            unsigned short icode = seqLCdnnTable_[iclass_index];
            
            bool bnum = false;
            bool benglish = false;
            bool bhigh_low_type = false;
            bool bchn = false;

            check_num_str(str_label, bnum);
            check_eng_str(str_label, benglish, bhigh_low_type);
            check_chn(str_label, bchn);
            
            new_word.isPunc_ = check_punc(icode);
            new_word.highLowType_ = 0;
            new_word.iLcType = 0;
            if (bnum) {
                new_word.iLcType = 1;
            } else if (benglish) {
                new_word.iLcType = 2;
                if (bhigh_low_type) {
                    new_word.highLowType_ = 1;
                }
            } else if (new_word.isPunc_ && !_m_resnet_strategy_flag) {
                new_word.iLcType = 3;
            } else if (!bchn && _m_resnet_strategy_flag) {
                new_word.iLcType = 3;
            }

            new_word.bRegFlag = false;
            if (e >= s + 1) {
                new_word.bRegFlag = true;
            }

            new_word.left_add_space_ = false;
            new_word.timeStart_ = s;
            new_word.timeEnd_ = e;
            new_word.maxVec_ = temp_word.maxVec_;
            new_word.toBeMaxConf_ = true;
            new_word.duplicateNum_ = 1;
            outVec.push_back(new_word);            
        }
        refine_word_position(outVec, 0, istart_vec_size, iline_left, iline_right);
    }

    return ;
    //
}

void CSeqLAsiaWordReg::split_to_one_word_beam(std::vector<SSeqLEngWordInfor> &in_vec,\
    std::vector<int> times_vec, std::vector<int> out_len_vec, std::vector<int*> out_label_vec,
    std::vector<SSeqLEngWordInfor> &out_vec, float fExtendRatio) {
    
    // refine the information of each character
    out_vec.clear();
    int in_vec_size = in_vec.size();
    for (int i = 0; i < in_vec_size; i++) {
        SSeqLEngWordInfor & temp_word = in_vec[i];
        int word_time_num = times_vec[i]; 
        int beam_size = out_len_vec[i] / times_vec[i];

        std::vector<int> start_vec;
        std::vector<int> end_vec;
        std::vector<float> cent_vec;
        estimate_path_pos(temp_word.maxVec_, temp_word.pathVec_,\
                start_vec, end_vec, cent_vec, blank_);
       
        double dnorm_height_ratio = (double)temp_word.iNormHeight_ / _g_split_length; //number_recog
        double dest_rect_w = (double)(temp_word.iNormWidth_ * temp_word.hei_) \
                             / temp_word.iNormHeight_;
        double dresize_ratio = dest_rect_w / temp_word.wid_;
        double dwid_recog_ratio = (double)(word_time_num * dnorm_height_ratio)\
                                   / (temp_word.iNormWidth_);
        double drecog_wid = temp_word.wid_ * dwid_recog_ratio * dresize_ratio;
        double davg_recog_char_w = drecog_wid / (word_time_num * fExtendRatio); 
    
        int iline_left = temp_word.left_;
        int iline_right = temp_word.left_ + temp_word.wid_;
        int istart_vec_size = start_vec.size();

        // compute the position of each character
        for (int j = 0; j < istart_vec_size; j++) {
            int s = start_vec[j];
            int e = end_vec[j];

            SSeqLEngRect new_rect;
            new_rect.wid_ = (int)((e - s + 1) * davg_recog_char_w);
            new_rect.top_ = temp_word.top_;
            new_rect.hei_ = temp_word.hei_;
            new_rect.left_ = (int)(temp_word.left_ + s * davg_recog_char_w);

            SSeqLEngWordInfor new_word;
            cal_word_infor_beam(temp_word, new_rect, out_label_vec[i], beam_size, new_word, s, e);

            // classify the type of the word
            int ivec_idx = start_vec[j];
            int iclass_index = temp_word.maxVec_[ivec_idx];
            std::string str_label = tabelStrVec_[iclass_index];
            unsigned short icode = seqLCdnnTable_[iclass_index];
            
            bool bnum = false;
            bool benglish = false;
            bool bhigh_low_type = false;
            bool bchn = false; 

            check_num_str(str_label, bnum);
            check_eng_str(str_label, benglish, bhigh_low_type);
            check_chn(str_label, bchn);
            
            new_word.isPunc_ = check_punc(icode);
            new_word.highLowType_ = 0;
            new_word.iLcType = 0;
            if (bnum) {
                new_word.iLcType = 1;
            } else if (benglish) {
                new_word.iLcType = 2;
                if (bhigh_low_type) {
                    new_word.highLowType_ = 1;
                }
            } else if (new_word.isPunc_ && !_m_resnet_strategy_flag) {
                new_word.iLcType = 3;
            } else if (!bchn && _m_resnet_strategy_flag) {
                new_word.iLcType = 3;
            }

            new_word.bRegFlag = false;
            if (e >= s + 1) {
                new_word.bRegFlag = true;
            }

            new_word.left_add_space_ = false;
            new_word.timeStart_ = s;
            new_word.timeEnd_ = e;
            new_word.maxVec_ = temp_word.maxVec_;
            new_word.toBeMaxConf_ = true;
            new_word.duplicateNum_ = 1;
            out_vec.push_back(new_word);            
        }
        refine_word_position(out_vec, 0, istart_vec_size, iline_left, iline_right);
    }
    int show_recog_rst_flag = g_ini_file->get_int_value("SHOW_RECOG_RST",
        IniFile::_s_ini_section_global);
    if (show_recog_rst_flag == 1) {
        std::cout << std::endl;
    }
    
    return ;
}

// note: the matrix is the sorted probability matrix
void CSeqLAsiaWordReg::cal_word_infor_beam(SSeqLEngWordInfor &perWord, SSeqLEngRect &newRect,\
    int *p_label, int beam_size, SSeqLEngWordInfor &newWord, int setStartTime, int setEndTime) {
    
    std::vector<float> & permat = perWord.proMat_;
    int new_start_time = setStartTime;
    int new_end_time = setEndTime;
    int new_time_num = new_end_time - new_start_time + 1 ;

    std::vector<int> max_vec;
    float fconf = 0;
    for (int i = new_start_time; i <= new_end_time; i++)  {
        max_vec.push_back(perWord.maxVec_[i]);
        float fout =  permat[i * beam_size];
        if (fconf < fout)  {
            fconf = fout;
        }
    }
    newWord.maxVec_ = max_vec;
    newWord.maxStr_ = path_to_string_have_blank(max_vec);
    std::vector<int> path = path_elimination(max_vec, blank_);
    newWord.pathVec_ = path;
    newWord.reg_prob = fconf;
    newWord.regLogProb_ = safeLog(fconf);
    newWord.wordStr_ = path_to_string(path);

    int show_recog_rst_flag = g_ini_file->get_int_value("SHOW_RECOG_RST",
        IniFile::_s_ini_section_global);
    if (show_recog_rst_flag == 1) {
        std::cout << "[" << newWord.wordStr_ << "," 
            << newWord.regLogProb_ << "," << fconf << "] ";
    }

    int len = beam_size * new_time_num;
    newWord.proMat_.resize(len);
    memcpy(&(newWord.proMat_[0]), \
            &(permat[new_start_time * beam_size]), sizeof(float) * len);
    newWord.cand_label_vec.resize(len);
    memcpy(&(newWord.cand_label_vec[0]), \
            (p_label + new_start_time * beam_size), sizeof(float) * len);

    newWord.left_ = newRect.left_;
    newWord.top_  = newRect.top_;
    newWord.wid_  = newRect.wid_;
    newWord.hei_  = newRect.hei_;
    newWord.isPunc_ = false;
    newWord.toBeAdjustCandi_ = true;
    
    return ;
}

void CSeqLAsiaWordReg::add_word_candidate_beam(\
    std::vector<SSeqLEngWordInfor> &outVec, int beam_size) {
    // we try to get the confidence probability of this character
    int word_candidate_num = 1;
    if (_m_cand_num != -1) {
        word_candidate_num = _m_cand_num;
    } 
    if (word_candidate_num <= 1 || beam_size <= 2)  {
        return ;
    }
    word_candidate_num = std::min(word_candidate_num, beam_size);
    
    std::vector<SSeqLEngWordInfor> in_vec = outVec;
    int in_vec_size = in_vec.size();
    for (int i = 0; i < in_vec_size; i++) {
        SSeqLEngWordInfor & new_word = in_vec[i];
        if (new_word.bRegFlag && _m_cand_num == -1) {
            continue ;
        }
        std::vector<float> & per_mat = new_word.proMat_;
        float *pconf_row = &(per_mat[0]);
        float fmax_conf = 0;
        int s = new_word.timeStart_;
        int e = new_word.timeEnd_;
        int icheck_num = e - s + 1;

        int *pisort_idx = new int[word_candidate_num];
        for (int idx = 0; idx < word_candidate_num; idx++) {
            pisort_idx[idx] = new_word.cand_label_vec[i];
        }
        fmax_conf = pconf_row[0];
        float fmax_conf2 = pconf_row[1];
        double dratio = fmax_conf / (fmax_conf2 + 0.0001);
        
        SSeqLEngWordInfor first_word = new_word;
        // first_word --> converting to the upper case 
        if (first_word.iLcType == 2 && first_word.highLowType_ == 1) {
            std::string strvec = first_word.wordStr_;
            transform(strvec.begin(), strvec.end(), strvec.begin(), ::toupper);
            first_word.wordStr_ = strvec;
        }
        if (_m_cand_num != -1) {
            outVec[i].cand_word_vec.push_back(first_word);
        }

        // the confidence value is reliable
        // default = 5.0
        if (dratio >= 5.0 && _m_cand_num == -1) {
            delete []pisort_idx;
            pisort_idx = NULL;
            continue;
        }
            
        for (int icand_idx = 1; icand_idx < word_candidate_num; icand_idx++) {
            if (pisort_idx[icand_idx] == labelsDim_-1 || \
                    pisort_idx[icand_idx] == pisort_idx[0]) {
                continue ;
            }

            SSeqLEngWordInfor cand_word = new_word;
            std::vector<int> path;
            path.clear();
            path.push_back(pisort_idx[icand_idx]);
            cand_word.pathVec_ = path;
            cand_word.maxVec_ = path;
            
            float fconfident = pconf_row[icand_idx];
            cand_word.reg_prob = fconfident; 
            cand_word.regLogProb_ = safeLog(fconfident); 
            cand_word.wordStr_ = path_to_accuratestring(path); 
            //cand_word.wordStr_ = pathToString(path); 
            cand_word.bRegFlag = false;
            int iclass_index = cand_word.maxVec_[0];
            std::string str_label = tabelStrVec_[iclass_index];
            unsigned short icode = seqLCdnnTable_[iclass_index];
            
            bool bnum = false;
            bool benglish = false;
            bool bhigh_low_type = false;
            bool bchn = false;

            check_num_str(str_label, bnum);
            check_eng_str(str_label, benglish, bhigh_low_type);
            check_chn(str_label, bchn);

            cand_word.isPunc_ = check_punc(icode);
            cand_word.iLcType = 0;
            cand_word.highLowType_ = 0;
            if (bnum) {
                cand_word.iLcType = 1;
            } else if (benglish) {
                cand_word.iLcType = 2;
                if (bhigh_low_type) {
                    cand_word.highLowType_ = 1;
                }
            } else if (cand_word.isPunc_ && !_m_resnet_strategy_flag) {
                cand_word.iLcType = 3;
            } else if (!bchn && _m_resnet_strategy_flag) {
                cand_word.iLcType = 3;
            }
            
            cand_word.left_add_space_ = false;
            cand_word.bRegFlag = false;
            cand_word.timeStart_ = s;
            cand_word.timeEnd_ = e;
            cand_word.toBeMaxConf_ = false;
            cand_word.duplicateNum_ = 1;
            outVec[i].cand_word_vec.push_back(cand_word);
        }

        if (pisort_idx) {
            delete []pisort_idx;
            pisort_idx = NULL;
        }
    }
    
    return ;
}

// recognize using deep-cnn lib interface
int CSeqLAsiaWordReg::recognize_word_line_rect(const IplImage *image, SSeqLEngLineRect & lineRect,\
    SSeqLEngLineInfor & outLineInfor) {

    int irect_num = lineRect.rectVec_.size();
    if (image->width < SEQL_MIN_IMAGE_WIDTH \
            || image->height < SEQL_MIN_IMAGE_HEIGHT\
            || image->width > SEQL_MAX_IMAGE_WIDTH \
            || image->height > SEQL_MAX_IMAGE_HEIGHT \
            || irect_num <= 0) {
        return -1;
    }

    char file_path[256];

    // extract the line box 
    outLineInfor.wordVec_.clear();
    outLineInfor.imgWidth_ = image->width;
    outLineInfor.imgHeight_ = image->height;
    outLineInfor.isBlackWdFlag_ = lineRect.isBlackWdFlag_;
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    cal_line_range(lineRect, left, top, right, bottom);
    outLineInfor.left_ = left;
    outLineInfor.top_  = top;
    outLineInfor.wid_ = right - left + 1;
    outLineInfor.hei_ = bottom - top + 1;

    float hor_rsz_ratio = 1.05; //default = 1.05; 
    hor_rsz_ratio = g_ini_file->get_double_value("CHN_RESIZE_W_RATIO",
         IniFile::_s_ini_section_global);

    std::vector<SSeqLEngLineInfor> recg_line_vec;
    recg_line_vec.resize(irect_num);
    for (int i = 0; i < irect_num; i++) {
        SSeqLEngRect & temp_rect = lineRect.rectVec_[i];
        recognize_rect(image, temp_rect,  lineRect.isBlackWdFlag_,
            hor_rsz_ratio, 
            recg_line_vec[i].wordVec_);
    }

    for (int i = 0; i < recg_line_vec.size(); i++) {
        for (int j = 0; j < recg_line_vec[i].wordVec_.size(); j++) {
            outLineInfor.wordVec_.push_back(recg_line_vec[i].wordVec_[j]);
        }
    }

    // add the candidate to the outLineInfor
    remove_duplicate_word(outLineInfor.wordVec_);
    sort(outLineInfor.wordVec_.begin(), outLineInfor.wordVec_.end(), leftSeqLWordRectFirst);
    add_word_candidate(outLineInfor.wordVec_);

    return 0;
}

int CSeqLAsiaWordReg::recognize_rect(const IplImage *image, SSeqLEngRect temp_rect,
    int bg_flag, 
    float hor_rsz_ratio,
    std::vector<SSeqLEngWordInfor> &split_word_vec
    ) {

    SSeqLEngRect ext_temp_rect = temp_rect;
    ext_temp_rect.left_ = std::max(0, temp_rect.left_);
    ext_temp_rect.top_  = std::max(0, temp_rect.top_);
    ext_temp_rect.wid_  = std::min(temp_rect.left_ + temp_rect.wid_ - 1,\
            image->width - 1) - ext_temp_rect.left_ + 1;
    ext_temp_rect.hei_  = std::min(temp_rect.top_ + temp_rect.hei_ - 1,\
            image->height - 1) - ext_temp_rect.top_ + 1;

    SSeqLEngWordInfor temp_word;
    std::vector<SSeqLEngWordInfor> vec_rct_word;

    // 
    float *data = NULL;
    int width = 0;
    int height = 0;
    int channel = 0;
    int beam_size = 1;
    std::vector<float *> data_v;
    std::vector<int> img_width_v;
    std::vector<int> img_height_v;
    std::vector<int> img_channel_v;
    std::vector<float *> vec_output;
    std::vector<int> vec_out_len;
    std::vector<int *> output_labels;
    
    extract_data(image, ext_temp_rect, data, width, height, channel, \
        bg_flag, hor_rsz_ratio);
    data_v.push_back(data);
    img_width_v.push_back(width);
    img_height_v.push_back(height);
    img_channel_v.push_back(channel);

    inference_seq::predict(/* model */seqLCdnnModel_,
                   /* batch_size */ 1,
                   /* inputs */ data_v,
                   /* widths */ img_width_v,
                   /* heights */ img_height_v,
                   /* channels */ img_channel_v,
                   /* beam_size */ beam_size,
                   /* output_probs */ vec_output,
                   /* output_labels */ output_labels,
                   /* output_lengths */ vec_out_len,
                   /* use_paddle */ _use_paddle);
    extract_word_infor(vec_output[0], labelsDim_, vec_out_len[0],\
        temp_word);
    bool recog_flag = true;
    if (_m_enhance_model == NULL) {
        recog_flag = false;
    } else if (_m_recog_model_type == 0 && temp_word.iLcType == 1) {
        recog_flag = false;
    } else if (temp_word.reg_prob >= _m_recog_thresh) {
        recog_flag = false;
    }
    
    // check the probability of the rectangle
    if (recog_flag) {
        for (int j = 0; j < vec_output.size(); j++) {
            if (vec_output[j]) {
                free(vec_output[j]);
                vec_output[j] = NULL;
            }
        }
        for (int j = 0; j < output_labels.size(); j++) {
            if (output_labels[j]) {
                free(output_labels[j]);
                output_labels[j] = NULL;
            }
        }
        inference_seq::predict(/* model */_m_enhance_model->seqLCdnnModel_,
                       /* batch_size */ 1,
                       /* inputs */ data_v,
                       /* widths */ img_width_v,
                       /* heights */ img_height_v,
                       /* channels */ img_channel_v,
                       /* beam_size */ beam_size,
                       /* output_probs */ vec_output,
                       /* output_labels */ output_labels,
                       /* output_lengths */ vec_out_len,
                       /* use_paddle */_m_enhance_model->_use_paddle);
        extract_word_infor(vec_output[0], labelsDim_, vec_out_len[0],\
            temp_word);            
    }
    temp_word.left_ = ext_temp_rect.left_;
    temp_word.top_  = ext_temp_rect.top_;
    temp_word.wid_  = ext_temp_rect.wid_;
    temp_word.hei_  = ext_temp_rect.hei_;
    temp_word.iNormWidth_ = width; 
    temp_word.iNormHeight_ = height; 
    for (int j = 0; j < data_v.size(); j++) {
        if (data_v[j]) {
            free(data_v[j]);
            data_v[j] = NULL;
        }
    }
    for (int j = 0; j < vec_output.size(); j++) {
        if (vec_output[j]) {
            free(vec_output[j]);
            vec_output[j] = NULL;
        }
    }
    for (int j = 0; j < output_labels.size(); j++) {
        if (output_labels[j]) {
            free(output_labels[j]);
            output_labels[j] = NULL;
        }
    }
    vec_rct_word.push_back(temp_word);
    split_to_one_word(vec_rct_word, split_word_vec, hor_rsz_ratio);

    return 0;
}
    
int CSeqLAsiaWordReg::recognize_word_line_rect_paddle(const IplImage *image, 
    SSeqLEngLineRect &lineRect,\
    SSeqLEngLineInfor &outLineInfor) {
    int irect_num = lineRect.rectVec_.size();
    if (image->width < SEQL_MIN_IMAGE_WIDTH \
            || image->height < SEQL_MIN_IMAGE_HEIGHT\
            || image->width > SEQL_MAX_IMAGE_WIDTH \
            || image->height > SEQL_MAX_IMAGE_HEIGHT \
            || irect_num <= 0) {
        return -1;
    }

    // extract the line box 
    outLineInfor.wordVec_.clear();
    outLineInfor.imgWidth_ = image->width;
    outLineInfor.imgHeight_ = image->height;
    outLineInfor.isBlackWdFlag_ = lineRect.isBlackWdFlag_;
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    cal_line_range(lineRect, left, top, right, bottom);
    outLineInfor.left_ = left;
    outLineInfor.top_  = top;
    outLineInfor.wid_ = right - left + 1;
    outLineInfor.hei_ = bottom - top + 1;

    std::vector<int> all_width_v;
    std::vector<int> all_height_v;
    std::vector<int> all_channel_v;
    std::vector<float *> all_output;
    std::vector<int> all_out_len;
    std::vector<int *> all_labels;
    std::vector<int> vec_rect_row_idxs;
    std::vector<SSeqLEngRect> vec_rects;
    int beam_size = 0;
    float hor_rsz_ratio = 1.05; //default = 1.05; 
    hor_rsz_ratio = g_ini_file->get_double_value("CHN_RESIZE_W_RATIO",
         IniFile::_s_ini_section_global);
    for (int i = 0; i < irect_num; i++) {
        SSeqLEngRect & temp_rect = lineRect.rectVec_[i];
        if (temp_rect.bePredictFlag_ == false) {
            continue;
        }
        
        SSeqLEngRect ext_temp_rect = temp_rect;
        ext_temp_rect.left_ = std::max(0, temp_rect.left_);
        ext_temp_rect.top_  = std::max(0, temp_rect.top_);
        ext_temp_rect.wid_  = std::min(temp_rect.left_ + temp_rect.wid_ - 1,\
                image->width - 1) - ext_temp_rect.left_ + 1;
        ext_temp_rect.hei_  = std::min(temp_rect.top_ + temp_rect.hei_ - 1,\
                image->height - 1) - ext_temp_rect.top_ + 1;

        SSeqLEngWordInfor temp_word;
        std::vector<SSeqLEngWordInfor> vec_rct_word;
        std::vector<SSeqLEngWordInfor> split_word_vec;
        
        float *data = NULL;
        int width = 0;
        int height = 0;
        int channel = 0;
        std::vector<float *> data_v;
        std::vector<int> img_width_v;
        std::vector<int> img_height_v;
        std::vector<int> img_channel_v;
        std::vector<float *> vec_output;
        std::vector<int> vec_out_len;
        std::vector<int *> output_labels;
        
        extract_data(image, ext_temp_rect, data, width, height, channel, \
            lineRect.isBlackWdFlag_, hor_rsz_ratio);
        data_v.push_back(data);
        img_width_v.push_back(width);
        img_height_v.push_back(height);
        img_channel_v.push_back(channel);
        vec_rect_row_idxs.push_back(0);
        vec_rects.push_back(ext_temp_rect);
    
        inference_seq::predict(/* model */ seqLCdnnModel_,
                       /* batch_size */ 1,
                       /* inputs */ data_v,
                       /* widths */ img_width_v,
                       /* heights */ img_height_v,
                       /* channels */ img_channel_v,
                       /* beam_size */ beam_size,
                       /* output_probs */ vec_output,
                       /* output_labels */ output_labels,
                       /* output_lengths */ vec_out_len,
                       /* use_paddle */ _use_paddle);
        for (int rct_idx = 0; rct_idx < img_width_v.size(); rct_idx++) {
            all_width_v.push_back(img_width_v[rct_idx]);
            all_height_v.push_back(img_height_v[rct_idx]);
            all_channel_v.push_back(img_channel_v[rct_idx]);
            all_output.push_back(vec_output[rct_idx]);
            all_out_len.push_back(vec_out_len[rct_idx]);
            all_labels.push_back(output_labels[rct_idx]);
        } 

        if (data) {
            free(data);
            data = NULL;
        }
    }

    std::vector<SSeqLEngLineInfor> vec_out_line;
    vec_out_line.resize(1);
    post_proc(all_labels, all_output, all_out_len, all_width_v, all_height_v, 
        beam_size, vec_rect_row_idxs, vec_rects, hor_rsz_ratio, vec_out_line);
    if (vec_out_line.size() > 0) {
        outLineInfor.wordVec_ = vec_out_line[0].wordVec_;
    }
    // free memeory
    for (int i = 0; i < all_output.size(); i++) {
        if (all_output[i]) {
            free(all_output[i]);
            all_output[i] = NULL;
        }
    }
    for (int i = 0; i < all_labels.size(); i++) {
        if (all_labels[i]) {
            free(all_labels[i]);
            all_labels[i] = NULL;
        }
    }

    return 0;
}

int CSeqLAsiaWordReg::cal_line_range(SSeqLEngLineRect &line_rect, 
    int &left, int &top, int &right, int &bottom) {
    int ret = 0;
    
    left = line_rect.rectVec_[0].left_;
    top = line_rect.rectVec_[0].top_;
    right = line_rect.rectVec_[0].left_ + line_rect.rectVec_[0].wid_ - 1;
    bottom = line_rect.rectVec_[0].top_ + line_rect.rectVec_[0].hei_ - 1;
    for (unsigned int i = 1; i < line_rect.rectVec_.size(); i++) {
        SSeqLEngRect & temp_rect = line_rect.rectVec_[i];
        left = std::min(left, temp_rect.left_);
        top = std::min(top, temp_rect.top_);
        right = std::max(right, temp_rect.left_ + temp_rect.wid_ - 1);
        bottom = std::max(bottom, temp_rect.top_ + temp_rect.hei_ - 1);
    }

    return ret; 
}

int CSeqLAsiaWordReg::recognize_line_batch(const std::vector<st_batch_img> vec_img,\
    std::vector<SSeqLEngLineRect> &vec_line_rect,\
    std::vector<SSeqLEngLineInfor> &vec_out_line) {
    
    int vec_img_num = vec_img.size();
    if (vec_img_num <= 0) {
        return -1;
    }
    vec_out_line.clear();
    vec_out_line.resize(vec_img_num);

    const int min_batch_w = 5; 
    const int max_batch_w = 3000;
    const int min_batch_h = 5;
    const int max_batch_h = 3000;
    float resize_ratio = 1.1; //default = 1.1; 
    //resize_ratio = g_ini_file->get_double_value("CHN_RESIZE_W_RATIO",
    //     IniFile::_s_ini_section_global);

    // 1. get the recognize data batch
    std::vector<float *> data_v;
    std::vector<int>  img_width_v, img_height_v, img_channel_v;
    std::vector<int> vec_rect_row_idxs;
    std::vector<SSeqLEngRect> vec_rects;
    for (int i = 0; i < vec_img_num; i++) {
        int rect_num = vec_line_rect[i].rectVec_.size();
        int img_w = vec_img[i].img->width;
        int img_h = vec_img[i].img->height;
        
        if (img_w < min_batch_w \
                || img_h < min_batch_h\
                || img_w > max_batch_w \
                || img_h > max_batch_h \
                || rect_num <= 0) {
            continue ;
        }

        // get the range of the recognition rectangle
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
        cal_line_range(vec_line_rect[i], left, top, right, bottom);
        vec_out_line[i].left_ = left;
        vec_out_line[i].top_  = top;
        vec_out_line[i].wid_ = right - left + 1;
        vec_out_line[i].hei_ = bottom - top + 1;
        
        for (int rct_idx = 0; rct_idx < rect_num; rct_idx++) {
            float *data = NULL;
            int width = 0;
            int height = 0;
            int channel = 0;
            SSeqLEngRect &temp_rect = vec_line_rect[i].rectVec_[rct_idx];
            extract_data(vec_img[i].img, temp_rect, data, width, height, channel, \
                vec_img[i].bg_flag, resize_ratio);
            vec_rect_row_idxs.push_back(i);
            vec_rects.push_back(temp_rect);
            data_v.push_back(data);
            img_width_v.push_back(width);
            img_height_v.push_back(height);
            img_channel_v.push_back(channel);
        }
    }
    
    // 2. recognize using deep-cnn
    // note: the _m_norm_img_w and _m_norm_img_h might be different from img_width_v and img_height_v 
    std::vector<float *> vec_output;
    std::vector<int> vec_out_len;
    int data_num = data_v.size();
   
    int beam_size = 0;
    int gpu_batch_size = _batch_size; 
    std::vector<int *> output_labels;
    inference_seq::predict(/* model */ seqLCdnnModel_,
                       /* batch_size */ gpu_batch_size,
                       /* inputs */ data_v,
                       /* widths */ img_width_v,
                       /* heights */ img_height_v,
                       /* channels */ img_channel_v,
                       /* beam_size */ beam_size,
                       /* output_probs */ vec_output,
                       /* output_labels */ output_labels,
                       /* output_lengths */ vec_out_len,
                       /* use_paddle */ _use_paddle);
    post_proc(output_labels, vec_output, vec_out_len, img_width_v, img_height_v, 
        beam_size, vec_rect_row_idxs, vec_rects, resize_ratio, vec_out_line);

    // free memeory
    for (int i = 0; i < vec_output.size(); i++) {
        if (vec_output[i]) {
            free(vec_output[i]);
            vec_output[i] = NULL;
        }
    }
    for (int i = 0; i < output_labels.size(); i++) {
        if (output_labels[i]) {
            free(output_labels[i]);
            output_labels[i] = NULL;
        }
    }
    for (int i = 0; i < data_v.size(); i++) {
        if (data_v[i]) {
            free(data_v[i]);
            data_v[i] = NULL;
        }
    } 

    return 0;
}

void CSeqLAsiaWordReg::post_proc(const std::vector<int *> &output_labels, 
    const std::vector<float *> &vec_output,
    const std::vector<int> &vec_out_len, 
    const std::vector<int> &img_width_v, 
    const std::vector<int> &img_height_v,
    const int beam_size, 
    const std::vector<int> &vec_rect_row_idxs,
    const std::vector<SSeqLEngRect> &vec_rects,
    float resize_ratio,
    std::vector<SSeqLEngLineInfor> &vec_out_line) {

    int data_num = vec_output.size();
    for (int recg_idx = 0; recg_idx < data_num; recg_idx++) {
        std::vector<SSeqLEngWordInfor> vec_recg_words;
        SSeqLEngWordInfor recg_word;

        int time_num = 1;
        if (beam_size != labelsDim_) {
            extract_word_infor_beam(vec_output[recg_idx], output_labels[recg_idx],\
                beam_size, vec_out_len[recg_idx], recg_word);
        } else {
            extract_word_infor(vec_output[recg_idx], labelsDim_, vec_out_len[recg_idx],\
                recg_word);
        }

        recg_word.left_ = vec_rects[recg_idx].left_;
        recg_word.top_  = vec_rects[recg_idx].top_;
        recg_word.wid_  = vec_rects[recg_idx].wid_;
        recg_word.hei_  = vec_rects[recg_idx].hei_;
        recg_word.iNormWidth_ = img_width_v[recg_idx];
        recg_word.iNormHeight_ = img_height_v[recg_idx];
        vec_recg_words.push_back(recg_word);
    
        std::vector<int> times_vec;
        std::vector<int> out_len_vec;
        std::vector<int*> out_label_vec;

        out_len_vec.push_back(vec_out_len[recg_idx]);
        if (beam_size != labelsDim_) {
            out_label_vec.push_back(output_labels[recg_idx]);
            time_num = vec_out_len[recg_idx] / beam_size;
            times_vec.push_back(time_num);
        } 
        int row_idx = vec_rect_row_idxs[recg_idx]; 
        std::vector<SSeqLEngWordInfor> split_word_vec;
        if (beam_size != labelsDim_) {
            split_to_one_word_beam(vec_recg_words, times_vec, out_len_vec, out_label_vec,
                split_word_vec, resize_ratio);
        } else {
            split_to_one_word(vec_recg_words, split_word_vec, resize_ratio);
        }
        
        for (int j = 0; j < split_word_vec.size(); j++) {
            vec_out_line[row_idx].wordVec_.push_back(split_word_vec[j]);
        }
    }
    
    for (int row_idx = 0; row_idx < vec_out_line.size(); row_idx++) {
        if (!_m_resnet_strategy_flag) {
            remove_duplicate_word(vec_out_line[row_idx].wordVec_);
        }
        sort(vec_out_line[row_idx].wordVec_.begin(),\
            vec_out_line[row_idx].wordVec_.end(), leftSeqLWordRectFirst);
        // add the candidate to the outLineInfor
        if (beam_size != labelsDim_) {
            add_word_candidate_beam(vec_out_line[row_idx].wordVec_, beam_size);
        } else {
            add_word_candidate(vec_out_line[row_idx].wordVec_);
        }
    }

    return ;
}

int CSeqLAsiaWordReg::find_max(float* acts, int numClasses) {
     int  maxIdx = 0;
     float maxVal = acts[maxIdx];
     for (int i = 0; i < numClasses; ++i) {
         if (maxVal < acts[i]) {
             maxVal = acts[i];
             maxIdx = i;
         }
     }
     return maxIdx;
 }

int CSeqLAsiaWordReg::extract_word_infor(float *matrix, int lablesDim, int len,\
    SSeqLEngWordInfor &outWord) {

    int time_num = len / labelsDim_;
    outWord.proMat_.resize(len);
    memcpy(&(outWord.proMat_[0]), matrix, sizeof(float) * len);

    float min_prob = 1.0;
    std::vector<float> max_prob_vec;
    std::vector<int> max_vec;
    max_vec.resize(time_num);
    for (unsigned int i = 0; i < time_num; i++) {
        float *acts = matrix + i * labelsDim_;
        max_vec[i] = find_max(acts, labelsDim_);
        if (max_vec[i] == labelsDim_) {
            continue;
        }
        max_prob_vec.push_back(acts[max_vec[i]]);
        if (min_prob > acts[max_vec[i]]) {
            min_prob = acts[max_vec[i]];
        }
    }

    outWord.maxVec_ = max_vec; 
    outWord.maxStr_ = path_to_string_have_blank(max_vec);
    std::vector<int> path = path_elimination(max_vec, blank_);
    outWord.pathVec_ = path;
    outWord.regLogProb_ = 1.0;
    outWord.reg_prob = min_prob;
    outWord.wordStr_ = path_to_string(path);
    // default 0
    outWord.iLcType = 0;
    int eng_cnt = 0;
    int word_num = path.size();
    for (unsigned int i = 0; i < word_num; i++){
        std::string recog_word = tabelLowStrVec_[path[i]];
        if (recog_word >= "a" && recog_word <= "z") {
            eng_cnt++;
        }
    }
    float ratio = float(eng_cnt) / word_num;
    if (ratio >= 0.9) {
        outWord.iLcType = 1;
    }
    int show_recog_rst_flag = g_ini_file->get_int_value("SHOW_RECOG_RST",
            IniFile::_s_ini_section_global);
    if (show_recog_rst_flag == 1) {
        // xie shufu add   
        std::cout << "maxStr_:" << outWord.maxStr_
            << " wordStr_: " << outWord.wordStr_ \
            << " regLogProb: " << outWord.regLogProb_ 
            << " timeNum: " << time_num << std::endl;
    }

    return 0;
}

int CSeqLAsiaWordReg::extract_word_infor_beam(float *matrix, int *vec_labels,
    int beam_size, int len, SSeqLEngWordInfor &out_word) {
    
    out_word.proMat_.resize(len);

    memcpy(&(out_word.proMat_[0]), matrix, sizeof(float) * len);
    int time_num = len / beam_size;
    std::vector<int> max_vec;
    max_vec.resize(time_num);
    for (unsigned int i = 0; i < time_num; i++) {
        max_vec[i] = vec_labels[i * beam_size];
    }
   
    out_word.maxVec_ = max_vec; 
    out_word.maxStr_ = path_to_string_have_blank(max_vec);
    std::vector<int> path = path_elimination(max_vec, blank_);
    out_word.pathVec_ = path;
    out_word.regLogProb_ = 1.0;
    out_word.wordStr_ = path_to_string(path);

    int show_recog_rst_flag = g_ini_file->get_int_value("SHOW_RECOG_RST",
            IniFile::_s_ini_section_global);
    if (show_recog_rst_flag == 1) {
        // xie shufu add   
        std::cout << "maxStr_:" << out_word.maxStr_
            << " wordStr_: " << out_word.wordStr_ \
            << " regLogProb: " << out_word.regLogProb_ 
            << " timeNum: " << time_num << std::endl;
    }

    return 0;
}
void CSeqLAsiaWordReg::extract_data(const IplImage *image, SSeqLEngRect & tempRect, \
     float *& data, int & width, int & height, int & channel, \
     int isBlackWdFlag, float hor_rsz_rate) {

    IplImage tmp_image = IplImage(*image);
    int rct_w = tempRect.wid_;
    int rct_h = tempRect.hei_;

    IplImage* sub_image = cvCreateImage(cvSize(rct_w, rct_h),\
            image->depth, image->nChannels);
    cvSetImageROI(&tmp_image, cvRect(tempRect.left_, tempRect.top_, rct_w, rct_h));
    cvCopy(&tmp_image, sub_image);
    cvResetImageROI(&tmp_image);
    
    IplImage* gray_image = cvCreateImage(cvSize(sub_image->width, sub_image->height), \
            sub_image->depth, 1);
    if (sub_image->nChannels != 1) {
        cvCvtColor(sub_image, gray_image, CV_BGR2GRAY);
    } else {
        cvCopy(sub_image, gray_image);
    }
    
    resize_image_to_fix_height(gray_image, seqLNormheight_, hor_rsz_rate);  
    if (!_m_resnet_strategy_flag) {
        extend_slim_image(gray_image, 1.0);
    }
    data = (float *)malloc(gray_image->width * gray_image->height \
            * gray_image->nChannels * sizeof(float));
    int index = 0;
    if (isBlackWdFlag == 1) {
        for (int h = 0; h < gray_image->height; h++) {
            for (int w = 0; w < gray_image->width; w++) {
                for (int c = 0; c < gray_image->nChannels; c++) {
                    float value = cvGet2D(gray_image, h, w).val[c];
                    data[index] = value - seqLCdnnMean_;
                    index++;
                }
            }
        }
    } else {
        for (int h = 0; h < gray_image->height; h++) {
            for (int w = 0; w < gray_image->width; w++) {
                for (int c = 0; c < gray_image->nChannels; c++) {
                    float value = cvGet2D(gray_image, h, w).val[c];
                    data[index] = (255 - value) - seqLCdnnMean_;
                    index++;
                }
            }
        }
    }

    width = gray_image->width;
    height = gray_image->height;
    channel = gray_image->nChannels;
    cvReleaseImage(&gray_image);
    cvReleaseImage(&sub_image);

    return ;
}

void CSeqLAsiaWordReg::estimate_path_pos(std::vector<int> maxPath, std::vector<int> path,\
    std::vector<int> &startVec, std::vector<int> &endVec,\
    std::vector<float> &centVec, int blank) {

    std::vector<int> str;
    str.clear();
    startVec.clear();
    endVec.clear();
    centVec.clear();
    int prev_label = -1;
    int index = 0;
    bool flag = false;
    for (std::vector<int>::const_iterator label = maxPath.begin();
        label != maxPath.end(); label++) {
        if (*label != blank && (str.empty() || *label != str.back() || prev_label == blank)) {
            str.push_back(*label);
            startVec.push_back(index);
            if (flag) {
                endVec.push_back(index-1);
            }
            flag = true;
        }

        if (*label == blank && flag) {
            flag = false;
            endVec.push_back(index - 1);
        }
        prev_label = *label;
        index++;
    }
    if (flag == true) {
        endVec.push_back(index-1);
    }
    assert(str.size() == startVec.size());
    assert(str.size() == endVec.size());
    centVec.resize(startVec.size());
    
    for (int i = 0; i < centVec.size(); i++) {
        centVec[i] = (startVec[i] + endVec[i]) / 2.0;
    }

    return ;
}

std::vector<int> CSeqLAsiaWordReg::path_elimination( const std::vector<int> & path, int blank) {
   std::vector<int> str;
   str.clear();
   int prevLabel = -1;
   for (std::vector<int>::const_iterator label = path.begin(); label != path.end(); label++) {
       if (*label != blank && (str.empty() || *label != str.back() || prevLabel == blank)) {
           str.push_back(*label);
       }
       prevLabel = *label;
   }
   return str;
}

std::string CSeqLAsiaWordReg::path_to_string(std::vector<int>& path ){
    std::string str = "";
    for (unsigned int i = 0; i < path.size(); i++){
        str += tabelLowStrVec_[path[i]]; 
    }
    return str;
}

std::string CSeqLAsiaWordReg::path_to_accuratestring(std::vector<int>& path) {
    std::string str = "";
    for (unsigned int i = 0; i < path.size(); i++) {
        str += tabelStrVec_[path[i]]; 
    }
    return str;
}

std::string CSeqLAsiaWordReg::path_to_string_have_blank(std::vector<int>& path){
    std::string str = "";
    for (unsigned int i = 0; i < path.size(); i++){
        if (path[i] != blank_) {
            str += tabelLowStrVec_[path[i]]; 
        }
        else {
            str += "_"; 
        }
    }
    return str;
}
void CSeqLAsiaWordReg::resize_image_to_fix_height(IplImage *&image, 
    const int resizeHeight, float horResizeRate){
    if (resizeHeight > 0) {
        CvSize sz;
        sz.height = resizeHeight;
        sz.width = image->width * resizeHeight * 1.0/ image->height*horResizeRate;
        IplImage *dstImg = cvCreateImage(sz, image->depth, image->nChannels);
        cvResize(image,dstImg, CV_INTER_CUBIC);

        cvReleaseImage(& image);
        image = dstImg;
        dstImg = NULL;
    }
    return ;
}

void CSeqLAsiaWordReg::resize_image_to_fix_size(IplImage *&image, 
    const int resizeHeight, const int fixed_wid, float horResizeRate){
    if (resizeHeight > 0) {
        CvSize sz;
        sz.height = resizeHeight;
        sz.width = int(fixed_wid * resizeHeight * horResizeRate / image->height);
        IplImage *dst_img = cvCreateImage(sz, image->depth, image->nChannels);
        cvResize(image,dst_img, CV_INTER_CUBIC);
        cvReleaseImage(& image);
        image = dst_img;
        dst_img = NULL;
    }
    return ;
}

void CSeqLAsiaWordReg::extend_slim_image(IplImage *&image, float widthRate){
    int goalWidth = widthRate*image->height;
    if (image->width >= goalWidth) {
        return ;
    }
    IplImage *dstImg = cvCreateImage(cvSize(goalWidth, image->height), image->depth, image->nChannels);
    int shiftProject = (dstImg->width - image->width)/2;
    for (int h=0; h<dstImg->height; h++) {
        for (int w=0; w<dstImg->width; w++) {
            for(int c=0; c<dstImg->nChannels; c++) {
                  int projectW = 0;
                  if (w < shiftProject){
                      projectW = 0;
                  } else if (w >= (image->width + shiftProject)) {
                      projectW = image->width - 1;
                  } else {
                      projectW = w-shiftProject;
                  }
                  CV_IMAGE_ELEM(dstImg, uchar, h, dstImg->nChannels * w + c)   = 
                            CV_IMAGE_ELEM(image, uchar, h, image->nChannels*projectW + c);
              }
          }
    }

    cvReleaseImage(& image);
    image = dstImg;
    dstImg = NULL;
    return ;
}

void CSeqLAsiaWordReg::set_enhance_recog_model_row(
    /*enhance_recg_model*/CSeqLEngWordRegModel *p_recog_model,
    /*threshold*/float recog_thresh) {
    _m_enhance_model = p_recog_model;
    _m_recog_thresh = recog_thresh;
    return ;  
}

void CSeqLAsiaWordReg::set_candidate_num(int candidate_num) {
    if (candidate_num > 0 && candidate_num <= 20) {
        _m_cand_num = candidate_num;
    }
    return ;
}

void CSeqLAsiaWordReg::set_openmp_thread_num(
    /*openmp_thread_num*/int thread_num) {
    _m_openmp_thread_num = thread_num;
    return ;
}

void CSeqLAsiaWordReg::set_recog_model_type(
        /*model_type:0,1*/int model_type) {
    _m_recog_model_type = model_type;
    return ; 
}

void CSeqLAsiaWordReg::set_resnet_strategy_flag(bool flag) {
    _m_resnet_strategy_flag = flag;
    return;
}
// recognize the image in batch
// resize the image data to the same size
// do not use any processing
int CSeqLAsiaWordReg::recognize_row_img_batch(const std::vector<st_batch_img> vec_img,\
    std::vector<SSeqLEngLineInfor> &vec_out_line) {
    int vec_img_num = vec_img.size();
    if (vec_img_num <= 0) {
        return -1;
    }
    vec_out_line.clear();
    vec_out_line.resize(vec_img_num);

    const int min_batch_w = 5; 
    const int max_batch_w = 3000;
    const int min_batch_h = 5;
    const int max_batch_h = 3000;
    float resize_ratio = 1.0;  

    float avg_width = 0;
    for (int i = 0; i < vec_img_num; i++) {
        int img_w = vec_img[i].img->width;
        avg_width += img_w;
    }
    avg_width /= vec_img_num;
    int avg_img_width = (int)avg_width;

    // 1. get the recognize data batch
    std::vector<float *> data_v;
    std::vector<int> img_width_v, img_height_v, img_channel_v;
    std::vector<int> vec_rect_row_idxs;
    std::vector<SSeqLEngRect> vec_rects;
    for (int i = 0; i < vec_img_num; i++) {
        int img_w = vec_img[i].img->width;
        int img_h = vec_img[i].img->height;
        
        if (img_w < min_batch_w \
                || img_h < min_batch_h\
                || img_w > max_batch_w \
                || img_h > max_batch_h) {
            continue ;
        }

        // get the range of the recognition rectangle
        int left = 0;
        int top = 0;
        int right = img_w - 1;
        int bottom = img_h - 1;
        vec_out_line[i].left_ = left;
        vec_out_line[i].top_  = top;
        vec_out_line[i].wid_ = right - left + 1;
        vec_out_line[i].hei_ = bottom - top + 1;
        
        float *data = NULL;
        int width = 0;
        int height = 0;
        int channel = 0;
        SSeqLEngRect temp_rect;
        temp_rect.left_ = 0;
        temp_rect.top_ = 0;
        temp_rect.wid_ = img_w;
        temp_rect.hei_ = img_h;
        extract_data_batch(vec_img[i].img, temp_rect, data, width, height, channel, \
            vec_img[i].bg_flag, avg_img_width, resize_ratio);
        vec_rect_row_idxs.push_back(i);
        data_v.push_back(data);
        img_width_v.push_back(width);
        img_height_v.push_back(height);
        img_channel_v.push_back(channel);
    }
    
    // 2. recognize using paddle
    std::vector<float *> vec_output;
    std::vector<int> vec_out_len;
    int data_num = data_v.size();
   
    int beam_size = 0;
    int gpu_batch_size = _batch_size; 
    std::vector<int *> output_labels;
    inference_seq::predict(/* model */ seqLCdnnModel_,
                       /* batch_size */ gpu_batch_size,
                       /* inputs */ data_v,
                       /* widths */ img_width_v,
                       /* heights */ img_height_v,
                       /* channels */ img_channel_v,
                       /* beam_size */ beam_size,
                       /* output_probs */ vec_output,
                       /* output_labels */ output_labels,
                       /* output_lengths */ vec_out_len,
                       /* use_paddle */ _use_paddle);

    post_proc_batch(output_labels, vec_output, vec_out_len, img_width_v, img_height_v, 
        beam_size, vec_rect_row_idxs, vec_out_line);

    // free memeory
    for (int i = 0; i < vec_output.size(); i++) {
        if (vec_output[i]) {
            free(vec_output[i]);
            vec_output[i] = NULL;
        }
    }
    for (int i = 0; i < output_labels.size(); i++) {
        if (output_labels[i]) {
            free(output_labels[i]);
            output_labels[i] = NULL;
        }
    }
    for (int i = 0; i < data_v.size(); i++) {
        if (data_v[i]) {
            free(data_v[i]);
            data_v[i] = NULL;
        }
    } 

    return 0;

}

void CSeqLAsiaWordReg::extract_data_batch(const IplImage *image,
     SSeqLEngRect &tempRect, \
     float *&data, int &width, int &height, int &channel, \
     int isBlackWdFlag, int fixed_wid, float hor_rsz_rate) {

    IplImage tmp_image = IplImage(*image);
    int rct_w = tempRect.wid_;
    int rct_h = tempRect.hei_;

    IplImage* sub_image = cvCreateImage(cvSize(rct_w, rct_h),\
            image->depth, image->nChannels);
    cvSetImageROI(&tmp_image, cvRect(tempRect.left_, tempRect.top_, rct_w, rct_h));
    cvCopy(&tmp_image, sub_image);
    cvResetImageROI(&tmp_image);
    
    IplImage* gray_image = cvCreateImage(cvSize(sub_image->width, sub_image->height), \
            sub_image->depth, 1);
    if (sub_image->nChannels != 1) {
        cvCvtColor(sub_image, gray_image, CV_BGR2GRAY);
    } else {
        cvCopy(sub_image, gray_image);
    }
    
    resize_image_to_fix_size(gray_image, seqLNormheight_, fixed_wid, hor_rsz_rate);  
    //extend_slim_image(gray_image, 1.0);
    
    data = (float *)malloc(gray_image->width * gray_image->height \
            * gray_image->nChannels * sizeof(float));
    int index = 0;
    if (isBlackWdFlag == 1) {
        for (int h = 0; h < gray_image->height; h++) {
            for (int w = 0; w < gray_image->width; w++) {
                for (int c = 0; c < gray_image->nChannels; c++) {
                    float value = cvGet2D(gray_image, h, w).val[c];
                    data[index] = value - seqLCdnnMean_;
                    index++;
                }
            }
        }
    } else {
        for (int h = 0; h < gray_image->height; h++) {
            for (int w = 0; w < gray_image->width; w++) {
                for (int c = 0; c < gray_image->nChannels; c++) {
                    float value = cvGet2D(gray_image, h, w).val[c];
                    data[index] = (255 - value) - seqLCdnnMean_;
                    index++;
                }
            }
        }
    }

    width = gray_image->width;
    height = gray_image->height;
    channel = gray_image->nChannels;
    cvReleaseImage(&gray_image);
    cvReleaseImage(&sub_image);

    return ;
}

void CSeqLAsiaWordReg::post_proc_batch(const std::vector<int *> &output_labels, 
    const std::vector<float *> &vec_output,
    const std::vector<int> &vec_out_len, 
    const std::vector<int> &img_width_v, 
    const std::vector<int> &img_height_v,
    const int beam_size, 
    const std::vector<int> &vec_rect_row_idxs,
    std::vector<SSeqLEngLineInfor> &vec_out_line) {

    int data_num = vec_output.size();
    for (int recg_idx = 0; recg_idx < data_num; recg_idx++) {
        std::vector<SSeqLEngWordInfor> vec_recg_words;
        SSeqLEngWordInfor recg_word;

        int time_num = 1;
        if (beam_size != labelsDim_) {
            extract_word_infor_beam(vec_output[recg_idx], output_labels[recg_idx],\
                beam_size, vec_out_len[recg_idx], recg_word);
        } else {
            extract_word_infor(vec_output[recg_idx], labelsDim_, vec_out_len[recg_idx],\
                recg_word);
        }

        recg_word.left_ = 0;
        recg_word.top_  = 0;
        recg_word.wid_  = img_width_v[recg_idx] - 1;
        recg_word.hei_  = img_height_v[recg_idx] - 1;
        recg_word.iNormWidth_ = img_width_v[recg_idx];
        recg_word.iNormHeight_ = img_height_v[recg_idx];
        int row_idx = vec_rect_row_idxs[recg_idx];
        vec_out_line[row_idx].wordVec_.push_back(recg_word);
    }
    return ;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
