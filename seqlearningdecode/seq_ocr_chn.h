/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file service-api.h
 * @author xieshufu(com@baidu.com)
 * @date 2015/09/09 13:29:37
 * @brief 
 *  
 **/

#ifndef  NSMOCR_SEQ_OCR_CHN_H
#define  NSMOCR_SEQ_OCR_CHN_H

#include "esc_searchapi.h"
#include "DictCalFactory.h"
#include "SeqLEngWordRecog.h"
#include "seql_asia_word_recog.h"
#include "seq_recg_model.h"
#include <cv.h>
#include <highgui.h>

namespace nsseqocrchn
{

class CSeqModelCh
{
public:
    CSeqModelCh()
    {
        _clear();
    }
    virtual~ CSeqModelCh()
    {
        _destroy();
    }

public:
    // model_type = 0: use chinese and english model
    // model_type = 1: only use chinese model
    // fast_flag = true: call the fast strategy version
    // fast_flag = false: call the slow strategy version
    // currently, it does not work
    int recognize_image(const IplImage *p_image,\
            const std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
            int model_type,
            bool fast_flag,
            std::vector<SSeqLEngLineInfor> &out_line_vec);

    int recognize_image(const IplImage *p_image,\
            const std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
            int model_type,
            bool fast_flag,
            std::vector<SSeqLEngLineInfor> &out_line_vec, bool is_refine_pos);

    int recognize_image_hw_num(const IplImage *p_image,\
            const std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
            int model_type,
            bool fast_flag,
            std::vector<SSeqLEngLineInfor> &out_line_vec);

    // recognize the single char with chn model
    int recognize_single_char(const IplImage *p_image,\
            const std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
            std::vector<SSeqLEngWordInfor> &out_word_vec);

    void set_debug_line_index(int debug_line_index);
    void set_mul_row_seg_para(bool mul_row_seg_flag);
    void set_remove_head_end_noise(bool remove_head_end_noise_flag);
    void set_no_before_process(bool no_before_process_flag);
    void set_low_prob_recg_flag(bool low_prob_recg_flag);
    void set_save_low_prob_flag(bool save_low_prob_flag);
    void set_save_number_flag(bool save_number_flag);
    void set_acc_eng_highlowtype_flag(bool flag);
    void set_formulate_output_flag(bool flag);
    void set_resnet_strategy_flag(bool flag);
public:
    // recognize the line with paddle predict
    int predict_line(const IplImage *p_image,\
        const std::vector<seq_recg_ns::DectRect> &in_line_dect_vec,\
        seq_recg_ns::CRecgModel *recg_model,
        std::vector<seq_recg_ns::RecgLineInfor> &out_line_vec);
public:
    // the outside call this function and determines which model is used 
    // e.g. fast model or high accuracy model
    int init_model(const esp::esp_engsearch* eng_search_recog_eng, 
        const LangModel::AbstractScoreCal* inter_lm_eng,  
        const CSeqLEngWordRegModel* recog_model_eng, 
        const esp::esp_engsearch* eng_search_recog_chn, 
        const LangModel::AbstractScoreCal* inter_lm_chn,
        const CSeqLEngWordRegModel* recog_model_chn);
    
    // initialize the row recognition model
    int init_row_recog_model(const CSeqLEngWordRegModel* recog_model_chn);
    // initialize the column recognition model
    int init_col_recog_model(const CSeqLEngWordRegModel* recog_model_chn);
    
    // initialize the single char recognition model
    int init_char_recog_model(const CSeqLEngWordRegModel* recog_model_char);
    
    // initialize the enhance recognition model
    int init_enhance_recog_model_row(
        /*recg model*/const CSeqLEngWordRegModel* recog_model_enhance,
        /*recg thresh*/float recog_thresh);

    // set the recognition candidate number
    int set_recg_cand_num(int cand_num);
    // set the recognition model time step number
    int set_time_step_number(int time_step_number);
    // set the enhance thread num
    void set_recg_thread_num(int thread_num);

private:
    void _clear();
    void _destroy();
protected:
    void recognize_line_chn(const IplImage *image,
        const std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
        CSeqLAsiaWordReg *row_recog_processor, 
        CSeqLAsiaWordReg *col_recog_processor, 
        std::vector<SSeqLEngLineInfor> &out_line_vec);

    // recognize the row image
    void recognize_line_row_chn(const IplImage *image,\
        std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
        CSeqLAsiaWordReg *recog_processor, 
        /*enhance model*/CSeqLEngWordRegModel *recog_processor_enhance, 
        /*enhance model thresh*/float recog_thresh,
        std::vector<SSeqLEngLineInfor> &out_line_vec);  

    void recognize_line_row_chn(const IplImage *image,\
        std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
        CSeqLAsiaWordReg *recog_processor, 
        std::vector<SSeqLEngLineInfor> &out_line_vec);

    // divide the detected lines into two parts: row lines and column lines
    int divide_dect_rows(const std::vector<SSeqLESegDecodeSScanWindow> &indectVec,
        std::vector<SSeqLESegDecodeSScanWindow> &vec_row_lines,
        std::vector<SSeqLESegDecodeSScanWindow> &vec_column_lines);
    
    // recognize the column image
    void recognize_line_col_chn(const IplImage *image,
        const std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
        CSeqLAsiaWordReg *recog_processor, std::vector<SSeqLEngLineInfor> &out_line_vec);

    // process the recognition result of the column image
    void rotate_col_recog_rst(std::vector<SSeqLEngLineInfor> in_col_line_vec,
        CvRect dect_rect, std::vector<SSeqLEngLineInfor> &out_col_line_vec);

    // recognize the row with English OCR engine
    void recognize_line_en(const IplImage *image, CSeqLEngWordReg *recog_processor_en,\
        int *ptable_flag, std::vector<SSeqLEngLineInfor> &out_line_vec);
protected:
    void cal_ch_table_in_en_table();

    double compute_en_char_ratio(const SSeqLEngLineInfor &line_info, 
        int *ptable_flag, int &en_char_cnt);

    void recognize_row_whole_eng(const IplImage *image, CSeqLEngWordReg *recog_processor_en,
        bool extend_flag,
        const SSeqLEngLineInfor &out_line_chn, SSeqLEngLineInfor &out_line);
    
    void recognize_row_piece_eng(const IplImage *image, CSeqLEngWordReg *recog_processor_en,
        bool extend_flag,
        const SSeqLEngLineInfor &out_line_chn, SSeqLEngLineInfor &out_line);

    int locate_eng_pos(SSeqLEngLineInfor line_info, 
        std::vector<SSeqLESegDecodeSScanWindow> &line_eng_vec,
        std::vector<int> &start_vec, std::vector<int> &end_vec); 

    int recognize_eng_row(const IplImage *image, 
        std::vector<SSeqLESegDecodeSScanWindow> in_eng_line_dect_vec,
        bool extend_flag,
        CSeqLEngWordReg *recog_processor_en,
        std::vector<SSeqLEngLineInfor>& out_line_vec_en);

    int merge_two_results(SSeqLEngLineInfor chn_line_info, 
        std::vector<SSeqLEngLineInfor> eng_line_vec,
        std::vector<int> start_vec, std::vector<int> end_vec, 
        SSeqLEngLineInfor &merge_line); 

    int divide_row_to_pieces(SSeqLEngLineInfor line_info, int start_idx,
        int *pword_flag, std::vector<SSeqLESegDecodeSScanWindow> &line_eng_vec,
        std::vector<int> &start_vec, std::vector<int> &end_vec);

    int generate_dect_win(SSeqLEngLineInfor line_info, int start_idx, int end_idx,\
        SSeqLESegDecodeSScanWindow &dect_win);

protected:
    esp::esp_engsearch *_m_eng_search_recog_eng;
    LangModel::AbstractScoreCal *_m_inter_lm_eng;
    CSeqLEngWordReg *_m_recog_model_eng;
    CSeqLEngWordReg _m_model_eng;

    esp::esp_engsearch *_m_eng_search_recog_chn;
    LangModel::AbstractScoreCal *_m_inter_lm_chn;
    CSeqLAsiaWordReg *_m_recog_model_row_chn;
    CSeqLAsiaWordReg _m_model_row_chn;
    int _m_model_type; //0: chn and eng 1: chn only 2: eng only
    
    // recognition model for chn
    CSeqLAsiaWordReg *_m_recog_model_col_chn;
    CSeqLAsiaWordReg _m_model_col_chn;

    // recognition model for single char
    CSeqLEngWordReg *_m_recog_model_chn_char;
    CSeqLEngWordReg _m_model_char;
    
    // enhance row recognition model
    CSeqLEngWordRegModel *_m_enhance_recog_model_row;
    float _m_recg_thresh_row;

    // true: call the multi-row segmentation module
    // false: do not call the multi-row segmentation module
    // only used for chinese ocr module
    bool _m_mul_row_seg_flag;

    // true: call the char recognition module 
    // false: do not call the char recognition module
    bool _m_low_prob_recg_flag;

    // true: save the char with low probability
    // false: do not save the char with low probability
    bool _m_save_low_prob_flag;
    bool _m_save_number_flag;

    // true: save eng without upperlittle process
    // false: save the eng with upperlittle process
    bool _m_acc_eng_highlowtype_flag;
    bool _m_resnet_strategy_flag;

    int _m_recg_cand_num;

    int _m_recg_time_step_num;

    int _m_recg_thread_num;

    int *_m_table_flag;
    int _m_debug_line_index;
    //lx added to remove head end noise for subtitle
    bool _m_remove_head_end_noise;
    //lx added to remove the before process
    bool _m_no_before_process;

    //lx added to formulate_output to make num and punc as chn-format
    bool _m_formulate_output_flag;
public:
    int _m_recg_line_num;
};
};

#endif  //__SERVICE-API_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
