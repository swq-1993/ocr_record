/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file service-api.cpp
 * @author xieshufu(com@baidu.com)
 * @date 2015/09/09 13:29:17
 * @brief 
 *  
 **/
#include "seq_ocr_chn.h"
#include "SeqLEngLineBMSSWordDecoder.h"
#include "SeqLEngWordSegProcessor.h"
#include "seql_asia_word_seg_processor.h"
#include "seql_asia_line_bmss_word_decoder.h"
#include "dll_main.h"
#include "reform_en_rst.h" 
#include "common_func.h"
#include "char_recg.h"

extern char g_pimage_title[256];
extern char g_psave_dir[256];

namespace nsseqocrchn
{

const double g_vertical_aspect_ratio = 2.0;
void draw_detection_result(IplImage *image, CSeqLEngWordSegProcessor seg_processor);
void draw_segmentation_result(IplImage *image, std::vector<SSeqLEngLineRect> line_rect_vec);

// merge the word in the same position for different rows
void merge_two_line(SSeqLEngLineInfor& line_a, SSeqLEngLineInfor& line_b);


float overlap_ratio(SSeqLEngWordInfor word, SSeqLESegDetectWin box) {
    float area_word = (float)word.wid_ * word.hei_;
    if (box.bottom <= box.top || box.right <= box.left) {
        return 0.0;
    }
    float area_box = (float)(box.bottom - box.top) * (box.right - box.left);
    float overlap_wid = (float)(box.right - box.left) + word.wid_ - (
        std::max(word.wid_ + word.left_, box.right) - 
        std::min(word.left_, box.left));
    float overlap_hei = (float)(box.bottom - box.top) + word.hei_ - (
        std::max(word.hei_ + word.top_, box.bottom) - 
        std::min(word.top_, box.top));
    if (overlap_wid < 0.0 || overlap_hei < 0.0) {
        return 0.0;
    }
    float overlap_ratio = (overlap_wid * overlap_hei) / (area_word + area_box);
    return overlap_ratio;
}

void refine_recg_char_pos(SSeqLEngLineInfor &line, 
        std::vector<SSeqLESegDetectWin> detected_boxes_post) {

    std::vector<bool> box_flag(detected_boxes_post.size(), true); //true means valid box
    std::vector<SSeqLEngWordInfor> &word_vec = line.wordVec_;
    std::vector<bool> word_flag(word_vec.size(), true); // true means valid res pos
    std::vector<std::vector<int> > word_correspond_box_id; // word corresponding to box id
    word_correspond_box_id.clear();
    word_correspond_box_id.resize(detected_boxes_post.size());

    for (int word_idx = 0; word_idx < word_vec.size(); word_idx++) {
        int word_overlaped_count = 0; // the number of boxes that the word lies on
        
        for (int box_idx = 0; box_idx < detected_boxes_post.size(); box_idx++) {
            if (!box_flag[box_idx]) {
                continue;
            }
            if (overlap_ratio(word_vec[word_idx], detected_boxes_post[box_idx]) > 0.1f) {
                word_correspond_box_id[box_idx].push_back(word_idx);
                word_overlaped_count++;
            }
        }
        if (word_overlaped_count > 1) {
            word_flag[word_idx] = false;
        } 
    }

    //word position refine
    for (int box_idx = 0; box_idx < detected_boxes_post.size(); box_idx++) {
        for (int cor_idx = 0; cor_idx < word_correspond_box_id[box_idx].size(); cor_idx++) {
            int word_idx = word_correspond_box_id[box_idx][cor_idx];
            if (!word_flag[word_idx]) {
                continue;
            }
            int ileft = 0;
            int iright = 0;
            if (cor_idx == 0) {
                ileft = detected_boxes_post[box_idx].left;
            }
            else {
                int left_idx = word_correspond_box_id[box_idx][cor_idx-1];
                ileft = word_vec[left_idx].left_ + word_vec[left_idx].wid_;
            }
            if (cor_idx == word_correspond_box_id[box_idx].size() - 1) {
                iright = detected_boxes_post[box_idx].right;
            }
            else {
                int right_idx = word_correspond_box_id[box_idx][cor_idx+1];
                iright = word_vec[right_idx].left_; 
            }
            word_vec[word_idx].left_ = std::min(word_vec[word_idx].left_, ileft);
            word_vec[word_idx].wid_ = std::max(word_vec[word_idx].left_ + 
                    word_vec[word_idx].wid_, iright) - word_vec[word_idx].left_;
        }   
    }

}

void CSeqModelCh::_clear() {

    _m_eng_search_recog_chn = NULL;
    _m_inter_lm_chn = NULL;
    _m_recog_model_row_chn = NULL;
    _m_recog_model_col_chn = NULL;
    _m_recog_model_chn_char = NULL;

    _m_eng_search_recog_eng = NULL;
    _m_inter_lm_eng = NULL;
    _m_recog_model_eng = NULL;

    _m_table_flag = NULL;
    _m_debug_line_index = -1;
    _m_mul_row_seg_flag = true;
    
    _m_low_prob_recg_flag = true;
    _m_recg_cand_num = -1;
    _m_recg_time_step_num = 3;
    _m_save_low_prob_flag = false;
    _m_acc_eng_highlowtype_flag = false;
    _m_recg_line_num = 0;
    _m_remove_head_end_noise = false;
    _m_save_number_flag = false;
    _m_no_before_process = false;
    _m_recg_thresh_row = 0;
    _m_enhance_recog_model_row = NULL;
    _m_recg_thread_num = 0;
    _m_formulate_output_flag = false;
    _m_resnet_strategy_flag = false;
}

void CSeqModelCh::_destroy() {
    if (_m_table_flag) {
        delete []_m_table_flag;
    }
    _clear();
}

//only for handwritten number, no col, low probabilty thresh, post act from candidate
int CSeqModelCh::recognize_image_hw_num(const IplImage *p_image,\
        const std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
        int model_type,
        bool fast_flag,
        std::vector<SSeqLEngLineInfor> &out_line_vec) {
    int ret = 0;
    if (p_image == NULL || in_line_dect_vec.size() <= 0 ||
          (model_type != 0 && model_type != 1 && model_type != 2)) {
        ret = -1;
        return ret;
    }

    //resize the image to the 48 height
    IplImage *image = NULL;
    double ratio = 48.0 / p_image->height;
    int new_width = p_image->width * ratio;
    int new_height = 48;
    int width_shift = 0;
    if (new_width < 48) {
        new_width = new_width * 2;
        width_shift = new_width / 4;
        CvSize new_size;
        new_size.width = new_width;
        new_size.height = new_height;
        image = cvCreateImage(new_size, p_image->depth, p_image->nChannels);
        cvSet(image, CV_RGB(255, 255, 255), NULL);

        CvSize resize_size;
        resize_size.width = new_width / 2;
        resize_size.height = new_height;
        IplImage *image_tmp = NULL;
        image_tmp = cvCreateImage(resize_size, p_image->depth, p_image->nChannels);
        cvResize(p_image, image_tmp);
        
        cvSetImageROI(image, cvRect(width_shift, 0, image_tmp->width, image_tmp->height));
        cvCopy(image_tmp, image);
        cvResetImageROI(image);
        cvReleaseImage(&image_tmp);

    } else { //no shift
        CvSize new_size;
        new_size.width = new_width;
        new_size.height = new_height;
        image = cvCreateImage(new_size, p_image->depth, p_image->nChannels);
        cvResize(p_image, image);
    } 

    _m_model_type = model_type;
    /*recognize_line_chn(p_image, in_line_dect_vec, \
        _m_recog_model_row_chn, _m_recog_model_col_chn, out_line_vec);*/

    /*recognize_line_row_chn(p_image, in_line_dect_vec,\
            _m_recog_model_row_chn, 
            _m_enhance_recog_model_row,
            _m_recg_thresh_row,
            out_line_vec);*/

    std::vector<SSeqLEngLineInfor> line_infor_vec;
    std::vector<SSeqLEngLineRect> line_rect_vec;
    for (int i = 0; i < in_line_dect_vec.size(); i++) {
        SSeqLEngLineRect temp_rect;
        temp_rect.left_ = 0;
        temp_rect.top_ = 0;
        temp_rect.wid_ = new_width; 
        temp_rect.hei_ = new_height;
        temp_rect.isBlackWdFlag_ = 1;

        SSeqLEngRect temp_unit_rect;
        temp_unit_rect.left_ = 0;
        temp_unit_rect.top_ = 0;
        temp_unit_rect.wid_ = new_width;
        temp_unit_rect.hei_ = new_height;
        temp_rect.rectVec_.push_back(temp_unit_rect);
        line_rect_vec.push_back(temp_rect);
    }
    /*
    CSeqLAsiaWordSegProcessor seg_processor;

    seg_processor.set_file_detect_line_rects(in_line_dect_vec);
    seg_processor.set_row_seg_parameter(_m_mul_row_seg_flag);

    int row_nb_char_size = g_ini_file->get_int_value(\
            "SEG_ROW_NB_CHAR_SIZE", IniFile::_s_ini_section_global);
    seg_processor.process_mulrow(p_image, seg_processor.get_detect_line_vec());
    seg_processor.word_group_nearby_adaptive(row_nb_char_size);

    line_rect_vec = seg_processor.get_seg_line_rect_vec();*/
    line_infor_vec.resize(line_rect_vec.size());
    
    for (unsigned int i = 0; i < line_rect_vec.size(); i++) {
        if (_m_debug_line_index != -1 && _m_debug_line_index !=i) {
            continue ;
        }
        _m_recog_model_row_chn->set_candidate_num(_m_recg_cand_num); //-1
        SSeqLEngLineInfor & temp_line = line_infor_vec[i];

        // deepcnn predict
        CSeqLAsiaWordReg *asia_recog = (CSeqLAsiaWordReg*)_m_recog_model_row_chn;
        asia_recog->recognize_word_line_rect(\
            image, line_rect_vec[i], temp_line);
    }

    out_line_vec.clear();
    for (unsigned int i = 0; i < line_infor_vec.size(); i++) {
        if (_m_debug_line_index != -1 && _m_debug_line_index !=i) {
            continue ;
        }
        /*
        CSeqLAsiaLineBMSSWordDecoder * line_decoder = new CSeqLAsiaLineBMSSWordDecoder; 
        SSeqLEngLineInfor out_line; 
        line_decoder->set_save_low_prob_flag(_m_save_low_prob_flag);

        //lx added to remove the head end noise for the subtitle
        if (_m_remove_head_end_noise) {
            line_decoder->set_remove_head_end_noise(\
                _m_remove_head_end_noise);
        }
        out_line = line_decoder->decode_line(_m_recog_model_row_chn, line_infor_vec[i]);

        delete line_decoder;
        out_line_vec.push_back(out_line);*/
        out_line_vec.push_back(line_infor_vec[i]);
    }

    // merge the result of the same row
    int out_line_vec_size = out_line_vec.size();
    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        std::string str_line_rst = "";
        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            str_line_rst += out_line_vec[i].wordVec_[j].wordStr_;
        }
        out_line_vec[i].lineStr_ = str_line_rst;
    }

    //remove empty line
    for (std::vector<SSeqLEngLineInfor>::iterator it = out_line_vec.begin();
            it != out_line_vec.end();) {
        if (it->lineStr_.length() <= 0) {
            it = out_line_vec.erase(it);
        } else {
            it++;
        }
    }
    out_line_vec_size = out_line_vec.size();
    post_proc_line_recog_result(out_line_vec);
    
    //from candidate to get number
    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        for (std::vector<SSeqLEngWordInfor>::iterator it = out_line_vec[i].wordVec_.begin(); \
                it != out_line_vec[i].wordVec_.end(); it++) {

            //is num
            if (it->wordStr_.size() == 1 && 
                    it->wordStr_[0] >= '0' && it->wordStr_[0] <= '9') {
                continue;
            } else {
                for (std::vector<SSeqLEngWordInfor>::iterator cand_it = it->\
                    cand_word_vec.begin(); cand_it != it->cand_word_vec.end(); cand_it++) {
                    if (cand_it->wordStr_.size() == 1 &&
                            cand_it->wordStr_[0] >= '0' && cand_it->wordStr_[0] <= '9') {
                        it->wordStr_ = cand_it->wordStr_;
                        it->regLogProb_ = cand_it->regLogProb_;
                        it->reg_prob = cand_it->reg_prob;
                        break;
                    }
                }
            }
        }
    }
    
    //refine the position with ratio and refine the string
    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        out_line_vec[i].left_ = p_image->width;
        out_line_vec[i].top_ = p_image->height;
        out_line_vec[i].wid_ = 0;
        out_line_vec[i].hei_ = 0;
        int line_right = 0;
        int line_bottom = 0;

        std::string str_line_rst = "";
        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {

            out_line_vec[i].wordVec_[j].left_ = (out_line_vec[i].wordVec_[j].left_ - 
                    width_shift) / ratio;
            out_line_vec[i].wordVec_[j].top_ = out_line_vec[i].wordVec_[j].top_ / ratio;
            out_line_vec[i].wordVec_[j].wid_ = (out_line_vec[i].wordVec_[j].wid_ - 
                    width_shift) / ratio;
            out_line_vec[i].wordVec_[j].hei_ = out_line_vec[i].wordVec_[j].hei_ / ratio;

            if (out_line_vec[i].wordVec_[j].left_add_space_) {
                str_line_rst += out_line_vec[i].wordVec_[j].wordStr_ + " ";
            } else {
                str_line_rst += out_line_vec[i].wordVec_[j].wordStr_;
            }

            for (unsigned int k = 0; k < out_line_vec[i].wordVec_[j].charVec_.size(); k++) {
                out_line_vec[i].wordVec_[j].charVec_[k].left_ = 
                    (out_line_vec[i].wordVec_[j].charVec_[k].left_ - width_shift) / ratio;
                out_line_vec[i].wordVec_[j].charVec_[k].top_ = 
                    out_line_vec[i].wordVec_[j].charVec_[k].top_ / ratio;
                out_line_vec[i].wordVec_[j].charVec_[k].wid_ = 
                    (out_line_vec[i].wordVec_[j].charVec_[k].wid_ - width_shift) / ratio;
                out_line_vec[i].wordVec_[j].charVec_[k].hei_ = 
                    out_line_vec[i].wordVec_[j].charVec_[k].hei_ / ratio;

                out_line_vec[i].wordVec_[j].charVec_[k].left_ = std::max(0, \
                        out_line_vec[i].wordVec_[j].charVec_[k].left_); 
                out_line_vec[i].wordVec_[j].charVec_[k].top_ = std::max(0, \
                        out_line_vec[i].wordVec_[j].charVec_[k].top_);
                out_line_vec[i].wordVec_[j].charVec_[k].wid_ = std::min(p_image->width - \
                        out_line_vec[i].wordVec_[j].charVec_[k].left_, \
                        out_line_vec[i].wordVec_[j].charVec_[k].wid_); 
                out_line_vec[i].wordVec_[j].charVec_[k].hei_ = std::min(p_image->height - \
                       out_line_vec[i].wordVec_[j].charVec_[k].top_, \
                        out_line_vec[i].wordVec_[j].charVec_[k].hei_);
            }

            line_right = std::max(line_right, out_line_vec[i].wordVec_[j].left_ + 
                    out_line_vec[i].wordVec_[j].wid_);
            line_bottom = std::max(line_bottom, out_line_vec[i].wordVec_[j].top_ + 
                    out_line_vec[i].wordVec_[j].hei_);
            out_line_vec[i].left_ = std::min(out_line_vec[i].left_, 
                    out_line_vec[i].wordVec_[j].left_);
            out_line_vec[i].top_ = std::min(out_line_vec[i].top_, 
                    out_line_vec[i].wordVec_[j].top_);
        }
        line_right = std::min(p_image->width, line_right);
        line_bottom = std::min(p_image->height, line_bottom);
        out_line_vec[i].left_ = std::max(0, out_line_vec[i].left_);
        out_line_vec[i].top_ = std::max(0, out_line_vec[i].top_);

        out_line_vec[i].wid_ = line_right - out_line_vec[i].left_;
        out_line_vec[i].hei_ = line_bottom - out_line_vec[i].top_;
        out_line_vec[i].lineStr_ = str_line_rst;
    }

    cvReleaseImage(&image);
    return ret;
}

// model_type = 0: use chinese and english model
// model_type = 1: only use chinese model
// fast_flag = true: call the fast model version
// fast_flag = false: call the slow model version
int CSeqModelCh::recognize_image(const IplImage *p_image,\
        const std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
        int model_type,
        bool fast_flag,
        std::vector<SSeqLEngLineInfor> &out_line_vec) {
    int ret = 0;
    if (p_image == NULL || in_line_dect_vec.size() <= 0 ||
          (model_type != 0 && model_type != 1 && model_type != 2)) {
        ret = -1;
        return ret;
    }

    _m_model_type = model_type;
    recognize_line_chn(p_image, in_line_dect_vec, \
        _m_recog_model_row_chn, _m_recog_model_col_chn, out_line_vec);
    
    // merge the charcters which are overlapped
    // the small english model is called here
    // no moer removal function
    // no second english recognition
    post_proc_line_recog_result(out_line_vec);
    if (model_type == 0 || model_type == 2) {
        recognize_line_en(p_image, _m_recog_model_eng,\
            _m_table_flag, out_line_vec);
    } 
    int out_line_vec_size = out_line_vec.size();
    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        std::string str_line_rst = "";
        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            if (out_line_vec[i].wordVec_[j].left_add_space_ && j > 0) {
                str_line_rst += " " + out_line_vec[i].wordVec_[j].wordStr_;
            } else {
                str_line_rst += out_line_vec[i].wordVec_[j].wordStr_;
            }
        }
        out_line_vec[i].lineStr_ = str_line_rst;
    }
    return ret;
}

// model_type = 0: use chinese and english model
// model_type = 1: only use chinese model
// fast_flag = true: call the fast model version
// fast_flag = false: call the slow model version
int CSeqModelCh::recognize_image(const IplImage *p_image,\
        const std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
        int model_type,
        bool fast_flag,
        std::vector<SSeqLEngLineInfor> &out_line_vec, bool is_refine_pos) {
    int ret = 0;
    if (p_image == NULL || in_line_dect_vec.size() <= 0 ||
          (model_type != 0 && model_type != 1 && model_type != 2)) {
        ret = -1;
        return ret;
    }

    _m_model_type = model_type;
    recognize_line_chn(p_image, in_line_dect_vec, \
        _m_recog_model_row_chn, _m_recog_model_col_chn, out_line_vec);
    
    // merge the charcters which are overlapped
    // the small english model is called here
    // no moer removal function
    // no second english recognition
    post_proc_line_recog_result(out_line_vec);

    // refine the position
    if (is_refine_pos) {
        for (int line_id = 0; line_id < out_line_vec.size(); line_id++) {
            refine_recg_char_pos(out_line_vec[line_id], in_line_dect_vec[0].winSet);
        }
    }

    if (model_type == 0 || model_type == 2) {
        recognize_line_en(p_image, _m_recog_model_eng,\
            _m_table_flag, out_line_vec);
    } 
    int out_line_vec_size = out_line_vec.size();
    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        std::string str_line_rst = "";
        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            if (out_line_vec[i].wordVec_[j].left_add_space_) {
                str_line_rst += out_line_vec[i].wordVec_[j].wordStr_ + " ";
            } else {
                str_line_rst += out_line_vec[i].wordVec_[j].wordStr_;
            }
        }
        out_line_vec[i].lineStr_ = str_line_rst;
    }

    return ret;
}

// recognize the single char with chn model
int CSeqModelCh::recognize_single_char(const IplImage *p_image,\
        const std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
        std::vector<SSeqLEngWordInfor> &out_word_vec) {
    int ret = 0;
    if (p_image == NULL || in_line_dect_vec.size() <= 0) {
        ret = -1;
        return ret;
    }

    int img_w = p_image->width;
    int img_h = p_image->height;
    for (int i = 0; i < in_line_dect_vec.size(); i++) {
        SSeqLESegDecodeSScanWindow dect_win = in_line_dect_vec[i];
        if (dect_win.left < 0 || dect_win.left >= img_w ||
            dect_win.right < 0 || dect_win.right >= img_w ||
            dect_win.top < 0 || dect_win.top >= img_h ||
            dect_win.bottom < 0 || dect_win.bottom >= img_h) {
            continue;
        }
        if (dect_win.left >= dect_win.right || 
            dect_win.top >= dect_win.bottom) {
            continue;
        }
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
        _m_recog_model_chn_char->rect_seql_predict_single_char(line_rect, black_bg_flag,\
           (IplImage*)p_image, out_word);
        out_word_vec.push_back(out_word);
    }
    
    return ret;
}
int CSeqModelCh::predict_line(const IplImage *p_image,\
    const std::vector<seq_recg_ns::DectRect> &in_line_dect_vec,\
    seq_recg_ns::CRecgModel *recg_model,
    std::vector<seq_recg_ns::RecgLineInfor> &out_line_vec) {
    int ret = 0;
    if (recg_model == NULL) {
        return -1;
    }
    recg_model->recognize_line(p_image, in_line_dect_vec,
        out_line_vec);

    return ret; 
}

void CSeqModelCh::set_debug_line_index(int debug_line_index) {
    if (debug_line_index >= 0) {
        _m_debug_line_index = debug_line_index;
    }
}
void CSeqModelCh::set_mul_row_seg_para(bool mul_row_seg_flag) {
    _m_mul_row_seg_flag = mul_row_seg_flag;
    return ;
}
void CSeqModelCh::set_low_prob_recg_flag(bool low_prob_recg_flag) {
    _m_low_prob_recg_flag = low_prob_recg_flag;
    return ;
}
void CSeqModelCh::set_save_low_prob_flag(bool save_low_prob_flag) {
    _m_save_low_prob_flag = save_low_prob_flag;
    return ;
}
void CSeqModelCh::set_save_number_flag(bool save_number_flag) {
    _m_save_number_flag = save_number_flag;
    return ;
}
void CSeqModelCh::set_acc_eng_highlowtype_flag(bool flag) {
    _m_acc_eng_highlowtype_flag = flag;
    return ;
}
void CSeqModelCh::set_remove_head_end_noise(bool remove_head_end_noise_flag) {
    _m_remove_head_end_noise = remove_head_end_noise_flag;
    return ;
}
void CSeqModelCh::set_no_before_process(bool no_before_process_flag) {
    _m_no_before_process = no_before_process_flag;
    return ;
}
void CSeqModelCh::set_formulate_output_flag(bool flag) {
    _m_formulate_output_flag = flag;
    return ;
}
void CSeqModelCh::set_resnet_strategy_flag(bool flag) {
    _m_resnet_strategy_flag = flag;
    return ;
}

// set the enhance thread num
void CSeqModelCh::set_recg_thread_num(int thread_num) {
    _m_recg_thread_num = thread_num;
    return ;
}
int CSeqModelCh::init_model(const esp::esp_engsearch* eng_search_recog_eng, 
        const LangModel::AbstractScoreCal* inter_lm_eng,  
        const CSeqLEngWordRegModel* recog_model_eng, 
        const esp::esp_engsearch* eng_search_recog_chn, 
        const LangModel::AbstractScoreCal* inter_lm_chn,
        const CSeqLEngWordRegModel* recog_model_chn) {
    int ret = 0;
    
    _m_eng_search_recog_eng = (esp::esp_engsearch*)eng_search_recog_eng; 
    _m_inter_lm_eng = (LangModel::AbstractScoreCal*)inter_lm_eng;

    if (recog_model_eng != NULL) {
        CSeqLEngWordRegModel* temp_model_eng =
                const_cast<CSeqLEngWordRegModel *>(recog_model_eng);
        _m_model_eng = CSeqLEngWordReg(*temp_model_eng);
        _m_recog_model_eng = &_m_model_eng;
    } else {
        _m_recog_model_eng = NULL;
    }

    _m_eng_search_recog_chn = (esp::esp_engsearch*)eng_search_recog_chn;
    _m_inter_lm_chn = (LangModel::AbstractScoreCal*)inter_lm_chn;

    if (recog_model_chn != NULL) {
        CSeqLEngWordRegModel* temp_model_chn =
                const_cast<CSeqLEngWordRegModel *>(recog_model_chn);
        _m_model_row_chn = CSeqLAsiaWordReg(*temp_model_chn);
        _m_recog_model_row_chn = (CSeqLAsiaWordReg*)&_m_model_row_chn;
    } else {
        _m_recog_model_row_chn = NULL;
    }

    cal_ch_table_in_en_table();

    return ret;
}

// initialize the row recognition model
int CSeqModelCh::init_row_recog_model(const CSeqLEngWordRegModel* recog_model_chn) {
    int ret = 0;
    _m_model_row_chn = CSeqLAsiaWordReg(*recog_model_chn);
    _m_recog_model_row_chn = (CSeqLAsiaWordReg*)&_m_model_row_chn;
    return ret; 
}
// initialize the column recognition model
int CSeqModelCh::init_col_recog_model(const CSeqLEngWordRegModel* recog_model_chn) {
    int ret = 0;
    _m_model_col_chn = CSeqLAsiaWordReg(*recog_model_chn);
    _m_recog_model_col_chn = (CSeqLAsiaWordReg*)&_m_model_col_chn;

    return ret;
}

// initialize the single char recognition model
int CSeqModelCh::init_char_recog_model(const CSeqLEngWordRegModel* recog_model_char) {
    int ret = 0;
    CSeqLEngWordRegModel* char_model_chn = (CSeqLEngWordRegModel*)recog_model_char; 
    _m_model_char = CSeqLEngWordReg(*char_model_chn);
    _m_recog_model_chn_char = (CSeqLEngWordReg*)&_m_model_char;

    return ret;
}
// initialize the enhance recognition model
int CSeqModelCh::init_enhance_recog_model_row(
    const CSeqLEngWordRegModel* recog_model_enhance,
    float recog_thresh) {
    int ret = 0;
    _m_enhance_recog_model_row = (CSeqLEngWordRegModel*)recog_model_enhance;
    _m_recg_thresh_row = recog_thresh;

    return ret;
}

// set the recognition candidate number
int CSeqModelCh::set_recg_cand_num(int cand_num) {
    int ret = 0;
    if (cand_num > 0 && cand_num < 20) {
        _m_recg_cand_num = cand_num;
    } else {
        _m_recg_cand_num = -1;
    }

    return ret;
}
// set the recognition model time step number
int CSeqModelCh::set_time_step_number(int time_step_number) {
    int ret = 0;
    _m_recg_time_step_num = time_step_number;
    return ret;
}

// recognize the row image
void CSeqModelCh::recognize_line_row_chn(const IplImage *image,\
    std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
    CSeqLAsiaWordReg *recog_processor, 
    CSeqLEngWordRegModel *recog_processor_enhance, 
    float recog_thresh,
    std::vector<SSeqLEngLineInfor> &out_line_vec)  {

    std::vector<SSeqLEngLineInfor> line_infor_vec;
    std::vector<SSeqLEngLineRect> line_rect_vec;

    if (_m_no_before_process) {
        for (int i = 0; i < in_line_dect_vec.size(); i++) {
            SSeqLEngLineRect temp_rect;
            temp_rect.left_ = 0;
            temp_rect.top_ = 0;
            temp_rect.wid_ = image->width; 
            temp_rect.hei_ = image->height;
            temp_rect.isBlackWdFlag_ = 1;

            SSeqLEngRect temp_unit_rect;
            temp_unit_rect.left_ = 0;
            temp_unit_rect.top_ = 0;
            temp_unit_rect.wid_ = image->width;
            temp_unit_rect.hei_ = image->height;
            temp_rect.rectVec_.push_back(temp_unit_rect);
            line_rect_vec.push_back(temp_rect);
        }
    }
    else {
        CSeqLAsiaWordSegProcessor seg_processor;

        seg_processor.set_file_detect_line_rects(in_line_dect_vec);
        seg_processor.set_row_seg_parameter(_m_mul_row_seg_flag);

        int row_nb_char_size = g_ini_file->get_int_value(\
            "SEG_ROW_NB_CHAR_SIZE", IniFile::_s_ini_section_global);
        seg_processor.process_mulrow(image, seg_processor.get_detect_line_vec());
        seg_processor.word_group_nearby_adaptive(row_nb_char_size);

        line_rect_vec = seg_processor.get_seg_line_rect_vec();
    }

    line_infor_vec.resize(line_rect_vec.size());
    for (unsigned int i = 0; i < line_rect_vec.size(); i++) {
        if (_m_debug_line_index != -1 && _m_debug_line_index !=i) {
            continue ;
        }
        recog_processor->set_candidate_num(_m_recg_cand_num); //-1
        SSeqLEngLineInfor & temp_line = line_infor_vec[i]; 

        // deepcnn predict
        recog_processor->set_openmp_thread_num(_m_recg_thread_num); //0
        recog_processor->set_enhance_recog_model_row(recog_processor_enhance, recog_thresh); 
        CSeqLAsiaWordReg *asia_recog = (CSeqLAsiaWordReg*)recog_processor;
        
        //liushanshan07 set recognize model time step number. now it maybe 6 or 3.
        asia_recog->_g_split_length = _m_recg_time_step_num;

        asia_recog->recognize_word_line_rect(\
            image, line_rect_vec[i], temp_line);
        // paddle predict
        //((CSeqLAsiaWordReg*)recog_processor)->recognize_word_line_rect_paddle(\
            (IplImage*)image, line_rect_vec[i], temp_line);
    }

    out_line_vec.clear();
    for (unsigned int i = 0; i < line_infor_vec.size(); i++) {
        if (_m_debug_line_index != -1 && _m_debug_line_index !=i) {
            continue ;
        }

        CSeqLAsiaLineBMSSWordDecoder * line_decoder = new CSeqLAsiaLineBMSSWordDecoder; 
        //line_decoder->setWordLmModel(_m_eng_search_recog_chn, _m_inter_lm_chn);
        SSeqLEngLineInfor out_line; 
        line_decoder->set_save_low_prob_flag(_m_save_low_prob_flag);

        //lx added to remove the head end noise for the subtitle
        if (_m_remove_head_end_noise) {
            line_decoder->set_remove_head_end_noise(\
                _m_remove_head_end_noise);
        }
        out_line = line_decoder->decode_line(recog_processor, line_infor_vec[i]);

        delete line_decoder;
        out_line_vec.push_back(out_line);
    }

    // merge the result of the same row
    int out_line_vec_size = out_line_vec.size();
    // liushanshan07 number_recog
    if (!_m_save_number_flag)
    {
        for (unsigned int i = 0; i < out_line_vec_size; i++) {
            std::string str_line_rst = "";
            for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
                str_line_rst += out_line_vec[i].wordVec_[j].wordStr_;
            }
            out_line_vec[i].lineStr_ = str_line_rst;
        }
    }
    else
    {
        for (vector<SSeqLEngLineInfor>::iterator iter = out_line_vec.begin(); iter!=out_line_vec.end(); )
        {
            std::string str_line_str = "";
            for (vector<SSeqLEngWordInfor>::iterator iword = iter->wordVec_.begin(); iword!= iter->wordVec_.end(); ){
                if (iword->wordStr_ != "" && (int)(iword->wordStr_[0]) >= 48 && (int)(iword->wordStr_[0]) <= 57){
                    str_line_str += iword->wordStr_;
                    iword++;
                }
                else{
                    iter->wordVec_.erase(iword);
                }
            }
            if (str_line_str == "")
            {
                out_line_vec.erase(iter);
            }
            else{
                iter->lineStr_ = str_line_str;
                iter++;
            }
        }
    }

    return ;
}

void CSeqModelCh::recognize_line_row_chn(const IplImage *image,\
    std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
    CSeqLAsiaWordReg *recog_processor, 
    std::vector<SSeqLEngLineInfor> &out_line_vec)  {

    std::vector<SSeqLEngLineInfor> line_infor_vec;
    std::vector<SSeqLEngLineRect> line_rect_vec;
    CSeqLAsiaWordSegProcessor seg_processor;

    seg_processor.set_file_detect_line_rects(in_line_dect_vec);
    seg_processor.set_row_seg_parameter(_m_mul_row_seg_flag);

    int row_nb_char_size = g_ini_file->get_int_value(\
            "SEG_ROW_NB_CHAR_SIZE", IniFile::_s_ini_section_global);
    seg_processor.process_mulrow(image, seg_processor.get_detect_line_vec());
    seg_processor.word_group_nearby_adaptive(row_nb_char_size);

    line_rect_vec = seg_processor.get_seg_line_rect_vec();
    line_infor_vec.resize(line_rect_vec.size());

    for (unsigned int i = 0; i < line_rect_vec.size(); i++) {
        if (_m_debug_line_index != -1 && _m_debug_line_index !=i) {
            continue ;
        }
        recog_processor->set_candidate_num(_m_recg_cand_num);
        SSeqLEngLineInfor & temp_line = line_infor_vec[i]; 
        // deepcnn predict
        recog_processor->set_enhance_recog_model_row(
            NULL, 0); 
        recog_processor->recognize_word_line_rect(\
            (IplImage*)image, line_rect_vec[i], temp_line);
        // paddle predict
        //((CSeqLAsiaWordReg*)recog_processor)->recognize_word_line_rect_paddle(\
            (IplImage*)image, line_rect_vec[i], temp_line);
    }

    out_line_vec.clear();
    for (unsigned int i = 0; i < line_infor_vec.size(); i++) {
        if (_m_debug_line_index != -1 && _m_debug_line_index !=i) {
            continue ;
        }

        CSeqLAsiaLineBMSSWordDecoder * line_decoder = new CSeqLAsiaLineBMSSWordDecoder; 
        SSeqLEngLineInfor out_line; 
        line_decoder->set_save_low_prob_flag(_m_save_low_prob_flag);
        out_line = line_decoder->decode_line(recog_processor, line_infor_vec[i]);

        delete line_decoder;
        out_line_vec.push_back(out_line);
    }

    // merge the result of the same row
    int out_line_vec_size = out_line_vec.size();
    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        std::string str_line_rst = "";
        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            str_line_rst += out_line_vec[i].wordVec_[j].wordStr_;
        }
        out_line_vec[i].lineStr_ = str_line_rst;
    }

    return ;
}


// process the recognition result of the column image
void CSeqModelCh::rotate_col_recog_rst(std::vector<SSeqLEngLineInfor> in_col_line_vec,
    CvRect dect_rect,  std::vector<SSeqLEngLineInfor> &out_col_line_vec) {
    
    out_col_line_vec = in_col_line_vec;
    for (int line_idx = 0; line_idx < in_col_line_vec.size(); line_idx++) {

        out_col_line_vec[line_idx].left_ = dect_rect.x;
        out_col_line_vec[line_idx].top_ = dect_rect.y;
        out_col_line_vec[line_idx].wid_ = dect_rect.width;
        out_col_line_vec[line_idx].hei_ = dect_rect.height;

        for (int i = 0; i < in_col_line_vec[line_idx].wordVec_.size(); i++) {
            SSeqLEngWordInfor in_word = in_col_line_vec[line_idx].wordVec_[i];
            SSeqLEngWordInfor& out_word = out_col_line_vec[line_idx].wordVec_[i];
            out_word.wid_ = in_word.hei_;
            out_word.hei_ = in_word.wid_;
            out_word.left_ = in_word.top_ + dect_rect.x;
            int shift_y = (int)(out_word.hei_ * 0.25);
            out_word.top_ = std::max(in_word.left_ + dect_rect.y - shift_y, 0);
        }
    }

    return ;
}

// recognize the column image
void CSeqModelCh::recognize_line_col_chn(const IplImage *image,
    const std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
    CSeqLAsiaWordReg *recog_processor, std::vector<SSeqLEngLineInfor> &out_line_vec) {
    
    for (unsigned int i = 0; i < in_line_dect_vec.size(); i++) {
        if (_m_debug_line_index != -1 && _m_debug_line_index !=i) {
            continue ;
        }

        // 1.get the cropped colum image
        SSeqLESegDecodeSScanWindow line_rect = in_line_dect_vec[i];
        int dect_w = line_rect.right - line_rect.left + 1;
        int dect_h = line_rect.bottom - line_rect.top + 1;
        CvRect dect_rect;
        dect_rect = cvRect(line_rect.left, line_rect.top, dect_w, dect_h); 
        cvSetImageROI((IplImage*)image, dect_rect);
        IplImage *crop_img = cvCreateImage(cvSize(dect_w, dect_h),
            image->depth, image->nChannels);
        cvCopy(image, crop_img, 0);
        cvResetImageROI((IplImage*)image);

        // 2.rotate the cropped image patch
        IplImage* rot_col_img = rotate_image(crop_img, 90, false);
        int flag = g_ini_file->get_int_value(
                "DRAW_COL_ROTATE_FLAG", IniFile::_s_ini_section_global);
        if (flag == 1) {
            char save_path[256];
            snprintf(save_path, 256, "./temp/%s-%d.jpg", g_pimage_title, i);  
            cvSaveImage(save_path, rot_col_img);
        }

        // 3.convert the detection result in the cropped image patch    
        std::vector<SSeqLESegDecodeSScanWindow> col_rect_vec;
        SSeqLESegDecodeSScanWindow dect_win;
        dect_win.left = 0;
        dect_win.top = 0;
        dect_win.right = rot_col_img->width - 1;
        dect_win.bottom = rot_col_img->height - 1;
        dect_win.isBlackWdFlag = in_line_dect_vec[i].isBlackWdFlag;
        col_rect_vec.push_back(dect_win);

        // 4.get the recognition result of the rotated image
        std::vector<SSeqLEngLineInfor> out_col_line_vec;
        recognize_line_row_chn(rot_col_img, col_rect_vec,\
            recog_processor, out_col_line_vec);
        
        // 5.convert the result to get the position in the orginal image
        std::vector<SSeqLEngLineInfor> rot_out_col_line_vec;
        rotate_col_recog_rst(out_col_line_vec, dect_rect, rot_out_col_line_vec);
        for (int j = 0; j < rot_out_col_line_vec.size(); j++) {
            out_line_vec.push_back(rot_out_col_line_vec[j]);
        }

        if (crop_img) {
            cvReleaseImage(&crop_img);
            crop_img = NULL;
        }
        if (rot_col_img) {
            cvReleaseImage(&rot_col_img);
            rot_col_img = NULL;
        }
    }

    return ;
}

// divide the detected lines into two parts: row lines and column lines
int CSeqModelCh::divide_dect_rows(const std::vector<SSeqLESegDecodeSScanWindow> &indectVec,
    std::vector<SSeqLESegDecodeSScanWindow> &vec_row_lines,
    std::vector<SSeqLESegDecodeSScanWindow> &vec_column_lines) {

    int ret = 0;
    // call the MultipleRowSegmentation module
    int idect_line_num = indectVec.size();
    for (unsigned int iline_idx = 0; iline_idx < idect_line_num; iline_idx++) {
        int line_height = indectVec[iline_idx].bottom - indectVec[iline_idx].top + 1;
        int line_width = indectVec[iline_idx].right - indectVec[iline_idx].left + 1;
        double vertical_height_thresh =  g_vertical_aspect_ratio*line_width;
        // classify whether the input line image belongs to vertical column
        if (line_height > vertical_height_thresh) { 
            // vertical columns
            vec_column_lines.push_back(indectVec[iline_idx]);
        } else {
            // horizontal rows
            vec_row_lines.push_back(indectVec[iline_idx]);
        }
    }

    return ret;
}

void CSeqModelCh::recognize_line_chn(const IplImage *image,\
        const std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
        CSeqLAsiaWordReg *row_recog_processor, 
        CSeqLAsiaWordReg *col_recog_processor, 
        std::vector<SSeqLEngLineInfor> &out_line_vec)  {

    // 1. divide the detection lines into rows and columns 
    std::vector<SSeqLESegDecodeSScanWindow> vec_row_lines;
    std::vector<SSeqLESegDecodeSScanWindow> vec_column_lines;
    divide_dect_rows(in_line_dect_vec, vec_row_lines, vec_column_lines);

    // 0: both row and columns are recognized
    // 1: only row is recognized
    // 2: only column is recognized
    int row_col_recog_mode = g_ini_file->get_int_value(\
            "ROW_COL_RECOG_MODE", IniFile::_s_ini_section_global);
        
    if (row_col_recog_mode == 0) {
        // 2. recognize the row vectors
        std::vector<SSeqLEngLineInfor> out_row_line_vec;
        recognize_line_row_chn(image, vec_row_lines,\
            row_recog_processor, 
            _m_enhance_recog_model_row,
            _m_recg_thresh_row,
            out_row_line_vec);
        for (int i = 0; i < out_row_line_vec.size(); i++) {
            out_line_vec.push_back(out_row_line_vec[i]);
        }

        // 3. recognize the column rectangle set
        if (col_recog_processor != NULL) {
            std::vector<SSeqLEngLineInfor> out_col_line_vec;
            recognize_line_col_chn(image, vec_column_lines, col_recog_processor, out_col_line_vec);
            for (int i = 0; i < out_col_line_vec.size(); i++) {
                out_line_vec.push_back(out_col_line_vec[i]);
            }
        }
    } else if (row_col_recog_mode == 1) {
        // 2. recognize the row vectors
        std::vector<SSeqLEngLineInfor> out_row_line_vec;
        recognize_line_row_chn(image, vec_row_lines,\
            row_recog_processor, _m_enhance_recog_model_row,
            _m_recg_thresh_row, out_row_line_vec);
        for (int i = 0; i < out_row_line_vec.size(); i++) {
            out_line_vec.push_back(out_row_line_vec[i]);
        }
    } else if (row_col_recog_mode == 2 && col_recog_processor != NULL) {
        // 3. recognize the column rectangle set
        std::vector<SSeqLEngLineInfor> out_col_line_vec;
        recognize_line_col_chn(image, vec_column_lines, col_recog_processor, out_col_line_vec);
        for (int i = 0; i < out_col_line_vec.size(); i++) {
            out_line_vec.push_back(out_col_line_vec[i]);
        }
    }

    return ;
}

void CSeqModelCh::cal_ch_table_in_en_table() {
    unsigned short *seql_cdnn_table_chn = NULL;
    int labels_dim_chn = 0;
    if (_m_recog_model_row_chn != NULL) {
        seql_cdnn_table_chn = _m_recog_model_row_chn->get_seql_cdnn_table();
        labels_dim_chn = _m_recog_model_row_chn->get_label_dim();
    } else {
        //SEQL_COUT << "_m_recog_model_ch is null." << std::endl;
        return;
    }

    if (_m_table_flag) {
        delete []_m_table_flag;
        _m_table_flag = NULL;
    } 
    _m_table_flag = new int[labels_dim_chn];
    memset(_m_table_flag, 0, sizeof(int)*labels_dim_chn);

    unsigned short *seql_cdnn_table_en = NULL;
    int labels_dim_en = 0;
    if (_m_recog_model_eng != NULL) {
        seql_cdnn_table_en = _m_recog_model_eng->get_seql_cdnn_table();
        labels_dim_en = _m_recog_model_eng->get_label_dim();
    } else {
        //SEQL_COUT << "_m_recog_model_eng is null." << std::endl;
        return;
    }

    for (int i = 0; i < labels_dim_chn - 1; i++) {
        for (int j = 0; j < labels_dim_en - 1; j++)  {
            if (seql_cdnn_table_chn[i] == seql_cdnn_table_en[j]) {
                _m_table_flag[i] = 1;
                break;
            }
        }
    }

    return;
}

double CSeqModelCh::compute_en_char_ratio(const SSeqLEngLineInfor &line_info,
    int *ptable_flag, int &en_char_cnt) {
    double en_char_ratio = 0; 
    int iword_num = line_info.wordVec_.size();
    int numerical_cnt = 0;
    int punc_cnt = 0;
    
    en_char_cnt = 0;
    for (int word_idx = 0; word_idx < iword_num; word_idx++) {
        const SSeqLEngWordInfor &word = line_info.wordVec_[word_idx];
        int class_index = word.pathVec_[0];
        if (ptable_flag[class_index] == 1) {
            en_char_cnt++;
        }        
        if (word.iLcType == 1) {
            numerical_cnt++;
        } else if (word.iLcType == 3) {
            punc_cnt++;
        }
    }
    double num_punc_ratio = (double)(numerical_cnt + punc_cnt) / iword_num;
    if (num_punc_ratio < 0.6) {
        en_char_ratio = (double)en_char_cnt / iword_num;
    }

    return en_char_ratio;
}

// the whole row is recognized with english ocr model
void CSeqModelCh::recognize_row_whole_eng(const IplImage *image,
    CSeqLEngWordReg *recog_processor_en,
    bool extend_flag,
    const SSeqLEngLineInfor &out_line_chn, SSeqLEngLineInfor &out_line) {
    
    std::vector<SSeqLESegDecodeSScanWindow> in_eng_line_dect_vec;
    SSeqLESegDecodeSScanWindow temp_wind;
    temp_wind.left = out_line_chn.left_;
    temp_wind.right =  out_line_chn.left_ + out_line_chn.wid_ - 1;
    temp_wind.top = out_line_chn.top_;
    temp_wind.bottom = out_line_chn.top_ + out_line_chn.hei_ - 1;
    temp_wind.isBlackWdFlag = out_line_chn.isBlackWdFlag_;
    temp_wind.detectConf = 0;
    in_eng_line_dect_vec.push_back(temp_wind);
         
    std::vector<SSeqLEngLineInfor> out_line_vec_en;
    recognize_eng_row(image, in_eng_line_dect_vec, 
        extend_flag, recog_processor_en, out_line_vec_en);
        
    std::vector<SSeqLEngLineInfor> out_line_vec_en_new;
    reform_line_infor_vec_split_char(out_line_vec_en, image->width, out_line_vec_en_new);
    
    if (_m_formulate_output_flag) {
        for (unsigned int i = 0; i < out_line_vec_en_new.size(); i++) {
            for (unsigned int j = 0; j < out_line_vec_en_new[i].wordVec_.size(); j++) {
                out_line_vec_en_new[i].wordVec_[j].left_add_space_ = true;
            }
        }
    }

    int line_vec_ch_size = out_line_chn.wordVec_.size();
    int line_eng_row_size = out_line_vec_en_new.size();

    out_line = out_line_chn;
    if (line_eng_row_size > 0) {
        int line_vec_en_size = 0;             
        for (unsigned int j = 0; j < out_line_vec_en_new[0].wordVec_.size(); j++) {
            line_vec_en_size += out_line_vec_en_new[0].wordVec_[j].charVec_.size();
        }
        if (line_vec_en_size > line_vec_ch_size / 4.0) {
            out_line  = out_line_vec_en_new[0];
        }
    } 
    return ;
}

//
void CSeqModelCh::recognize_row_piece_eng(const IplImage *image, 
    CSeqLEngWordReg *recog_processor_en, bool extend_flag, 
    const SSeqLEngLineInfor &out_line_chn,
    SSeqLEngLineInfor &out_line) {

    int ret_val = 0;
    std::vector<SSeqLESegDecodeSScanWindow> in_eng_line_dect_vec;
    std::vector<int> start_vec;
    std::vector<int> end_vec;
    out_line = out_line_chn;
    ret_val = locate_eng_pos(out_line_chn, in_eng_line_dect_vec, start_vec, end_vec);
    if (ret_val != 0) {
        return ;
    }

    // recognize the row with english ocr
    std::vector<SSeqLEngLineInfor> out_line_vec_en;
    ret_val = recognize_eng_row(image, in_eng_line_dect_vec, extend_flag,
        recog_processor_en, out_line_vec_en);
    if (ret_val != 0) {
        return ;
    }

    // get the position of each character
    std::vector<SSeqLEngLineInfor> out_vec_new;
    reform_line_infor_vec_split_char(out_line_vec_en, image->width, out_vec_new);

    SSeqLEngLineInfor merge_line;
    ret_val = merge_two_results(out_line, out_vec_new, start_vec, end_vec, merge_line);
    if (ret_val == 0) {
        out_line = merge_line;
    }

    return ;
}
// recognize the row with English OCR engine
void CSeqModelCh::recognize_line_en(const IplImage *image, CSeqLEngWordReg *recog_processor_en,\
     int *ptable_flag, std::vector<SSeqLEngLineInfor> &out_line_vec) {

    // threshold for the whole row using english ocr
    const float row_whole_eng_thresh = 0.95;
    // threshsold for piecewise english ocr
    const int row_piece_eng_thresh = 5;

    CSeqLEngWordSegProcessor seg_processor_en;
    int iout_line_vec_size = out_line_vec.size();
    for (int i = 0; i < iout_line_vec_size; i++) {

        int iword_num = out_line_vec[i].wordVec_.size();
        int line_w = out_line_vec[i].wid_;
        int line_h = out_line_vec[i].hei_;
        double ver_line_thresh = line_w * g_vertical_aspect_ratio;
        if (iword_num <= 1 || line_h > ver_line_thresh) {
            continue ;
        }

        int eng_char_cnt = 0;
        double d_num_en_ratio = compute_en_char_ratio(out_line_vec[i], ptable_flag, eng_char_cnt);
        SSeqLEngLineInfor out_line;
        if (d_num_en_ratio >= row_whole_eng_thresh) {
            // call the whole row english model recognition
            // true: the region will be extended
            recognize_row_whole_eng(image, recog_processor_en, true, out_line_vec[i], out_line);
            out_line_vec[i] = out_line;
        } else if (eng_char_cnt >= row_piece_eng_thresh) {
            // call the piecewise english model recognition
            // false: the region will not be extended
            recognize_row_piece_eng(image, recog_processor_en, false, out_line_vec[i], out_line);
            out_line_vec[i] = out_line;
        }
    }
    
    if (_m_formulate_output_flag) {
        // formulate the output information for chinese/english output
        // english: word as the units
        std::vector<SSeqLEngLineInfor> new_line_vec;
        formulate_output(out_line_vec, new_line_vec);
        out_line_vec = new_line_vec;
    }
    
    return ;
}

void draw_detection_result(IplImage *image, CSeqLEngWordSegProcessor seg_processor) {
    IplImage *ptmp_image = cvCloneImage(image);
    std::vector<SSeqLESegDecodeSScanWindow> vec_line_dect = seg_processor.getDetectLineVec();
    for (unsigned int iline_idx = 0; iline_idx < vec_line_dect.size(); iline_idx++) {
        int left = vec_line_dect[iline_idx].left;
        int top = vec_line_dect[iline_idx].top;
        int right = vec_line_dect[iline_idx].right;
        int bottom = vec_line_dect[iline_idx].bottom;
        cvRectangle(ptmp_image, cvPoint(left, top), cvPoint(right, bottom), cvScalar(0, 255, 0), 1);
    }

    char file_path[256];
    snprintf(file_path, 256, "%s/%s-line-dect.jpg", g_psave_dir, g_pimage_title);
    cvSaveImage(file_path, ptmp_image);
    cvReleaseImage(&ptmp_image);

    return ;
}

void draw_segmentation_result(IplImage *image, std::vector<SSeqLEngLineRect> line_rect_vec) {
    IplImage *ptmp_image = cvCloneImage(image);

    std::cout << "line_rect_vec size:" << line_rect_vec.size() << std::endl;
    for (unsigned int iline_idx = 0; iline_idx < line_rect_vec.size(); iline_idx++) {
        vector<SSeqLEngRect> vec_rect = line_rect_vec[iline_idx].rectVec_;
        std::cout << "vec_rect size: " << vec_rect.size() << std::endl;
        for (unsigned int irct_idx = 0; irct_idx < vec_rect.size(); irct_idx++) {
            SSeqLEngRect row_rect = vec_rect[irct_idx];
            int left = row_rect.left_;
            int top = row_rect.top_;
            int width = row_rect.wid_;
            int height = row_rect.hei_;
            int right = left + width;
            int bottom = top + height;
            bool predict_flag = vec_rect[irct_idx].bePredictFlag_;

            std::cout << "rect:" << irct_idx << ": " \
                << left << ", " << top << ", " << width << ", " << height << std::endl;

            if (predict_flag) {
                cvRectangle(ptmp_image, cvPoint(left, top),\
                        cvPoint(right, bottom), cvScalar(0, 255, 0), 1);
            }
        }
    }

    char file_path[256];
    snprintf(file_path, 256, "%s/%s-rect-group-1.jpg", g_psave_dir, g_pimage_title);
    cvSaveImage(file_path, ptmp_image);
    cvReleaseImage(&ptmp_image);
}

int CSeqModelCh::locate_eng_pos(SSeqLEngLineInfor line_info, 
    std::vector<SSeqLESegDecodeSScanWindow> &line_eng_vec,
    std::vector<int> &start_vec, std::vector<int> &end_vec) {

    int ret_val = 0;
    std::vector<SSeqLEngWordInfor> &word_vec = line_info.wordVec_;
    int word_num = word_vec.size();

    // note: the information of line_info is characters(Chinese OCR output), not words
    int *pword_flag = new int[word_num];
    int start_idx = -1;

    memset(pword_flag, 0, sizeof(int) * word_num);
    for (int i = 0; i < word_num; i++) {
        if (word_vec[i].iLcType == 2) {
            pword_flag[i] = 1;
        } 
        if (pword_flag[i] == 1 && start_idx == -1) {
            start_idx = i;
        }
    }

    ret_val = divide_row_to_pieces(line_info, start_idx, 
        pword_flag, line_eng_vec,
        start_vec, end_vec);
    
    if (pword_flag) {
        delete []pword_flag;
        pword_flag = NULL;
    }

    return ret_val;
}

// recognize the english rows
int CSeqModelCh::recognize_eng_row(const IplImage *image, 
    std::vector<SSeqLESegDecodeSScanWindow> in_eng_line_dect_vec,
    bool extend_flag,
    CSeqLEngWordReg *recog_processor_en,
    std::vector<SSeqLEngLineInfor>& out_line_vec_en) {
    
    int ret_val = 0;
    CSeqLEngWordSegProcessor seg_processor_en;
    std::vector<SSeqLEngLineRect> line_rect_vec_en;

    seg_processor_en.set_exd_parameter(extend_flag);
    seg_processor_en.process_image_eng_ocr(image, in_eng_line_dect_vec);
    seg_processor_en.wordSegProcessGroupNearby(4);
    
    line_rect_vec_en = seg_processor_en.getSegLineRectVec();
    int line_rect_vec_size = line_rect_vec_en.size();
    
    std::vector<SSeqLEngLineInfor> line_infor_vec_en;
    for (unsigned int i = 0; i < line_rect_vec_size; i++) {
        
        SSeqLEngLineInfor temp_line; 
        // true: do not utilize the 2nd recognition function
        // false: utilize the 2nd recognition function
        bool fast_recg_flag = true;
        SSeqLEngSpaceConf space_conf = seg_processor_en.getSegSpaceConf(i);
        recog_processor_en->recognize_eng_word_whole_line(image, line_rect_vec_en[i], \
                temp_line, space_conf, fast_recg_flag);
        line_infor_vec_en.push_back(temp_line);

        CSeqLEngLineDecoder * line_decoder = new CSeqLEngLineBMSSWordDecoder; 
        line_decoder->setWordLmModel(_m_eng_search_recog_eng, _m_inter_lm_eng);
        line_decoder->setSegSpaceConf(space_conf);
        line_decoder->set_acc_eng_highlowtype_flag(_m_acc_eng_highlowtype_flag);
        SSeqLEngLineInfor out_line = line_decoder->decodeLine(\
                recog_processor_en, temp_line);
        out_line_vec_en.push_back(out_line);

        delete line_decoder;
        line_decoder = NULL;
    }

    return ret_val;
}

int CSeqModelCh::merge_two_results(SSeqLEngLineInfor chn_line_info, 
    std::vector<SSeqLEngLineInfor> eng_line_vec,
    std::vector<int> start_vec, std::vector<int> end_vec, 
    SSeqLEngLineInfor &merge_line) {

    if (start_vec.size() <= 0 || end_vec.size() <= 0) {
        return -1;
    }
    
    merge_line = chn_line_info;
    merge_line.wordVec_.clear();
  
    std::vector<SSeqLEngWordInfor> word_vec;
    int chn_word_num = chn_line_info.wordVec_.size();
    int *pflag = new int[chn_word_num];
    memset(pflag, 0, sizeof(int) * chn_word_num);

    for (int i = 0; i < start_vec.size(); i++) {
        for (int j = start_vec[i]; j <= end_vec[i]; j++) {
            pflag[j] = 1;
        }
    }

    for (int i = 0; i < chn_word_num; i++) {
        if (pflag[i] == 0) {
            merge_line.wordVec_.push_back(chn_line_info.wordVec_[i]);
        }
    }

    for (int i = 0; i < eng_line_vec.size(); i++) {
        SSeqLEngLineInfor &line_info = eng_line_vec[i];
        for (int j = 0; j < line_info.wordVec_.size(); j++) {
            merge_line.wordVec_.push_back(line_info.wordVec_[j]);
        }
    }

    sort(merge_line.wordVec_.begin(), merge_line.wordVec_.end(), leftSeqLWordRectFirst);
    if (pflag) {
        delete []pflag;
        pflag = NULL;
    }

    return 0;
}

int CSeqModelCh::divide_row_to_pieces(SSeqLEngLineInfor line_info, int start_idx,
    int *pword_flag, std::vector<SSeqLESegDecodeSScanWindow> &line_eng_vec,
    std::vector<int> &start_vec, std::vector<int> &end_vec) {
    
    int ret_val = 0;
    if (start_idx == -1) {
        return -1;
    }
    // threshold for recognition using english ocr 
    const int min_eng_cnt = 5;
    int word_num = line_info.wordVec_.size();
    int end_idx = -1;
    
    while (1) {
        //
        end_idx = -1;
        for (int i = start_idx + 1; i < word_num; i++) {
            if (pword_flag[i] != 1) {
                end_idx = i - 1;
                break;
            }
            if (i == word_num - 1 && pword_flag[i] == 1) {
                end_idx = i;
                break;
            } else if (pword_flag[i] == 1 && pword_flag[i + 1] != 1) {
                end_idx = i;
                break;
            }
        }
        if (end_idx == -1) {
            break;
        }
        int eng_char_num = end_idx - start_idx + 1;
        if (eng_char_num >= min_eng_cnt) {
            SSeqLESegDecodeSScanWindow dect_win;
            generate_dect_win(line_info, start_idx, end_idx, dect_win);
            line_eng_vec.push_back(dect_win);
            start_vec.push_back(start_idx);
            end_vec.push_back(end_idx);
        }
        if (end_idx >= word_num - 1) {
            break;
        }
        start_idx = -1;
        for (int i = end_idx + 1; i < word_num - 1; i++) {
            if (pword_flag[i-1] != 1 && pword_flag[i] == 1) {
                start_idx = i;
                break;
            }
        }
        if (start_idx == -1 || start_idx == word_num - 1) {
            break;
        }
    }

    return ret_val;
}

int CSeqModelCh::generate_dect_win(SSeqLEngLineInfor line_info, int start_idx, int end_idx,\
    SSeqLESegDecodeSScanWindow &dect_win) {
    int ret_val = 0;
    if (end_idx < start_idx) {
        return -1;
    }
    std::vector<SSeqLEngWordInfor> & word_vec = line_info.wordVec_;
    int win_left = word_vec[start_idx].left_;
    int win_top = word_vec[start_idx].top_;
    int win_right = win_left + word_vec[start_idx].wid_ - 1;
    int win_bottom = win_top + word_vec[start_idx].hei_ - 1;
    for (int i = start_idx + 1; i <= end_idx; i++) {
        win_left = std::min(win_left, word_vec[i].left_);
        win_top = std::min(win_top, word_vec[i].top_);
        win_right = std::max(win_right, word_vec[i].left_ + word_vec[i].wid_ - 1);
        win_bottom = std::max(win_bottom, word_vec[i].top_ + word_vec[i].hei_ - 1);
    }

    dect_win.left = win_left;
    dect_win.right = win_right;
    dect_win.top = win_top;
    dect_win.bottom = win_bottom;
    dect_win.isBlackWdFlag = line_info.isBlackWdFlag_;
    dect_win.detectConf = 0;

    return ret_val;
}

};
/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
