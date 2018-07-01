/***************************************************************************
 * 
 * Copyright (c) 2016 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
/**
 * @file char_recg.cpp
 * @author xieshufu(com@baidu.com)
 * @date 2016/09/20 13:55:51
 * @brief 
 **/  
#include "char_recg.h"
#include "SeqLEngSegmentFun.h"

namespace nsseqocrchn
{

int recognize_single_char(const IplImage *p_image,\
    const std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
    CSeqLEngWordReg *recog_processor,\
    std::vector<SSeqLEngWordInfor> &out_word_vec);

// recognize the row image if the char confidence is low
void recognize_img_char(const IplImage *image,\
    CSeqLEngWordReg *recog_processor,\
    std::vector<SSeqLEngLineInfor> &out_line_vec) {

    if (image == NULL || recog_processor == NULL) {
        return ;
    }

    // high threshold for single char
    const double char_prob_thresh_high = 0.9;
    // low threshold for single char
    const double char_prob_thresh_low = 0.3;

    int img_w = image->width;
    int img_h = image->height;
    int line_num = out_line_vec.size();
    for (int i = 0; i < line_num; i++) {
        SSeqLEngLineInfor& line_infor = out_line_vec[i];
        std::vector<SSeqLEngWordInfor>& word_vec = line_infor.wordVec_;
        int word_num = word_vec.size();
        // omit the first and the last character
        for (int j = 0; j < word_num; j++) {
            if (word_vec[j].cand_word_vec.size() <= 0 || word_vec[j].iLcType != 0) {
                continue ;
            } 
            // recognize the char with new recognition engine
            std::vector<SSeqLESegDecodeSScanWindow> win_vec;
            SSeqLESegDecodeSScanWindow char_win;
            int lnb_right = line_infor.left_;
            int rnb_left = line_infor.left_ + line_infor.wid_ - 1;
            if (j > 0) {
                lnb_right = word_vec[j-1].left_ + word_vec[j-1].wid_ - 1;
            } 
            if (j < word_num-1) {
                rnb_left = word_vec[j+1].left_;
            }

            char_win.left = std::max(lnb_right, 0);
            char_win.right = std::min(rnb_left, img_w - 1);
            char_win.top = line_infor.top_;
            char_win.bottom = std::min(line_infor.top_ + line_infor.hei_ - 1, img_h - 1);
            win_vec.push_back(char_win);
            if (char_win.right <= char_win.left + 2 || char_win.bottom <= char_win.top + 2) {
                continue ;
            }
            std::vector<SSeqLEngWordInfor> out_word_vec;
            recognize_single_char(image, win_vec, recog_processor, out_word_vec);
            if (out_word_vec.size() <= 0) {
                continue ;
            }
            if (out_word_vec[0].wordStr_ == word_vec[j].wordStr_) {
                continue ;
            }
            if (out_word_vec[0].reg_prob <= word_vec[j].reg_prob ||
                out_word_vec[0].reg_prob <= char_prob_thresh_low) {
                continue ;
            }
            if (out_word_vec[0].reg_prob >= char_prob_thresh_high &&
                 out_word_vec[0].reg_prob > word_vec[j].reg_prob) {
                word_vec[j].reg_prob = out_word_vec[0].reg_prob;
                word_vec[j].regLogProb_ = out_word_vec[0].regLogProb_;
                word_vec[j].wordStr_ = out_word_vec[0].wordStr_;
                continue ;
            }
            // check the candidate word
            for (int k = 0; k < word_vec[j].cand_word_vec.size(); k++) {
                if (word_vec[j].cand_word_vec[k].wordStr_ == out_word_vec[0].wordStr_) {
                    word_vec[j].reg_prob = out_word_vec[0].reg_prob;
                    word_vec[j].regLogProb_ = out_word_vec[0].regLogProb_;
                    word_vec[j].wordStr_ = out_word_vec[0].wordStr_;
                    break;
                }
            }
        }
    }
    return ;
}

int recognize_single_char(const IplImage *p_image,\
    const std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
    CSeqLEngWordReg *recog_processor,\
    std::vector<SSeqLEngWordInfor> &out_word_vec) {
   
    int ret = 0;
    if (p_image == NULL || in_line_dect_vec.size() <= 0) {
        ret = -1;
        return ret;
    }

    for (int i = 0; i < in_line_dect_vec.size(); i++) {
        SSeqLESegDecodeSScanWindow dect_win = in_line_dect_vec[i];
        int dect_w = dect_win.right - dect_win.left + 1;
        int dect_h = dect_win.bottom - dect_win.top + 1;
        if (dect_w < 5 && dect_h < 5) {
            continue ;
        }
        SSeqLEngRect line_rect;
        line_rect.left_ = dect_win.left;
        line_rect.top_ = dect_win.top;
        line_rect.wid_ = dect_win.right - dect_win.left + 1;
        line_rect.hei_ = dect_win.bottom - dect_win.top + 1;
        int black_bg_flag = dect_win.isBlackWdFlag;
        SSeqLEngWordInfor out_word;
        recog_processor->rect_seql_predict_single_char(line_rect, black_bg_flag,\
           (IplImage*)p_image, out_word);
        out_word_vec.push_back(out_word); 
    }
    
    return ret;
}

};

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
