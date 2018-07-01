/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
/**
 * @file SeqLAsiaWordRecog.h
 * @author hanjunyu(com@baidu.com)
 * @date 2015/03/17 15:54:41
 * @brief 
 * revised by xieshufu
 * @date 2017/4/6
 **/

#ifndef  APP_SEARCH_VIS_OCR_ALG_SEQLEARNINGDECODE_SEQL_ASIA_WORD_RECOG_H
#define  APP_SEARCH_VIS_OCR_ALG_SEQLEARNINGDECODE_SEQL_ASIA_WORD_RECOG_H

#include <vector>
#include "SeqLEngWordRecog.h"
#include "SeqLEuroWordRecog.h"

class CSeqLAsiaWordReg {
public:
    void * seqLCdnnModel_;
    unsigned short * seqLCdnnTable_;
    float seqLCdnnMean_;
    int seqLNormheight_;
    std::vector<std::string> tabelStrVec_;
    std::vector<std::string> tabelLowStrVec_;
    int dataDim_;
    int labelsDim_;
    int blank_;
    bool _use_paddle;
    bool _use_gpu;
    int _batch_size;
    int _g_split_length;
    
    // candidate num
    int _m_cand_num;
    float _m_recog_thresh;
    CSeqLEngWordRegModel *_m_enhance_model;
    int _m_openmp_thread_num;
    // model_type flag 0: chn and eng; 1: chn
    int _m_recog_model_type;
    bool _m_resnet_strategy_flag;
private:
    void split_to_one_word(std::vector<SSeqLEngWordInfor> &inVec,\
            std::vector<SSeqLEngWordInfor> & outVec, float fExtendRatio);
    
public:
    CSeqLAsiaWordReg();
    ~CSeqLAsiaWordReg();
    CSeqLAsiaWordReg(const CSeqLEngWordRegModel &inRegModel);
    
    int recognize_word_line_rect(const IplImage *image, SSeqLEngLineRect & lineRect,\
            SSeqLEngLineInfor &outLineInfor);
    
    int recognize_word_line_rect_paddle(const IplImage *image, SSeqLEngLineRect & lineRect,\
            SSeqLEngLineInfor &outLineInfor);
    
    void add_word_candidate(std::vector<SSeqLEngWordInfor> &outVec);

    void refine_word_position(std::vector<SSeqLEngWordInfor> &outVec, int iStart,\
            int iWordNum, int iLineLeft, int iLineRight);
    
    void cal_word_infor(SSeqLEngWordInfor &perWord, SSeqLEngRect &newRect,\
            SSeqLEngWordInfor & newWord, int setStartTime, int setEndTime);

public:
    int recognize_line_batch(const std::vector<st_batch_img> vec_img,\
        std::vector<SSeqLEngLineRect> &vec_line_rect,\
        std::vector<SSeqLEngLineInfor> &vec_out_line);

    void set_enhance_recog_model_row(
        /*enhance_recg_model*/CSeqLEngWordRegModel *p_recog_model,
        /*threshold*/float recog_thresh);
    void set_candidate_num(int candidate_num);
    void set_resnet_strategy_flag(bool flag);
    void set_openmp_thread_num(
        /*openmp_thread_num*/int thread_num);
    void set_recog_model_type(
        /*model_type:0,1*/int model_type);
public:
    unsigned short* get_seql_cdnn_table()  {
        return seqLCdnnTable_;
    }
    int get_label_dim() {
        return labelsDim_;
    }
public:
    // recognize the image in batch
    // resize the image data to the same size
    // do not use any processing
    int recognize_row_img_batch(const std::vector<st_batch_img> vec_img,\
        std::vector<SSeqLEngLineInfor> &vec_out_line);
private:
    void extract_data_batch(const IplImage *image, 
        SSeqLEngRect &tempRect, \
        float *&data, int &width, int &height, int &channel, \
        int isBlackWdFlag, int fixed_wid,
        float hor_rsz_rate);
    void resize_image_to_fix_size(IplImage *&image, 
        const int resizeHeight, 
        const int fixed_wid, 
        float hor_rsz_rate);

    void post_proc_batch(const std::vector<int *> &output_labels, 
        const std::vector<float *> &vec_output,
        const std::vector<int> &vec_out_len, 
        const std::vector<int> &img_width_v, 
        const std::vector<int> &img_height_v,
        const int beam_size, 
        const std::vector<int> &vec_rect_row_idxs,
        std::vector<SSeqLEngLineInfor> &vec_out_line);

protected:
    int cal_line_range(SSeqLEngLineRect &line_rect, 
        int &left, int &top, int &right, int &bottom);

    int extract_word_infor(float *matrix, int lablesDim, int len,\
        SSeqLEngWordInfor &outWord);
    void extract_data(const IplImage *image, SSeqLEngRect & tempRect, \
        float *&data, int & width, int & height, int & channel, \
        int isBlackWdFlag, float horResizeRate); 
    void estimate_path_pos(std::vector<int> maxPath, std::vector<int> path,\
        std::vector<int> &startVec, std::vector<int> &endVec,\
        std::vector<float> &centVec, int blank);

    void post_proc(const std::vector<int *> &output_labels, 
        const std::vector<float *> &vec_output,
        const std::vector<int> &vec_out_len, 
        const std::vector<int> &img_width_v, 
        const std::vector<int> &img_height_v,
        const int beam_size, 
        const std::vector<int> &vec_rect_row_idxs,
        const std::vector<SSeqLEngRect> &vec_rects,
        float resize_ratio,
        std::vector<SSeqLEngLineInfor> &vec_out_line);
    
    int recognize_rect(const IplImage *image, SSeqLEngRect temp_rect,
        int bg_flag, 
        float hor_rsz_ratio,
        std::vector<SSeqLEngWordInfor> &split_word_vec
        );

    int find_max(float* acts, int numClasses);
    std::vector<int> path_elimination( const std::vector<int> & path, int blank);
    std::string path_to_string(std::vector<int>& path);
    std::string path_to_accuratestring(std::vector<int>& path);
    std::string path_to_string_have_blank(std::vector<int>& path);
    
    void extend_slim_image(IplImage *&image, float widthRate);
    void resize_image_to_fix_height(IplImage *&image, 
        const int resizeHeight, float horResizeRate);

protected: 
    void split_to_one_word_beam(std::vector<SSeqLEngWordInfor> &in_vec,\
            std::vector<int> times_vec, std::vector<int> out_len_vec,
            std::vector<int*> out_label_vec,
            std::vector<SSeqLEngWordInfor> & out_vec, float fExtendRatio);
    int extract_word_infor_beam(float *matrix, int *vec_labels, int beam_size, int len,\
        SSeqLEngWordInfor &out_word);
    void cal_word_infor_beam(SSeqLEngWordInfor &perWord, SSeqLEngRect &newRect,\
        int *p_label, int beam_size, SSeqLEngWordInfor &newWord,\
        int setStartTime, int setEndTime);
    void add_word_candidate_beam(std::vector<SSeqLEngWordInfor> &outVec, int beam_size);
private:
    void draw_time_line_data(SSeqLEngWordInfor &tempWord,\
            float fExtendRatio,  IplImage *pTmpImage);
}; 

#endif  //__SEQLASIAWORDRECOG_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
