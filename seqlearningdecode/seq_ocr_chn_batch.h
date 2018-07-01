/***************************************************************************
 * 
 * Copyright (c) 2016 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
/**
 * @file seq_ocr_chn_batch.h
 * @author xieshufu(com@baidu.com)
 * @date 2016/12/23 13:21:56
 * @brief 
 *  
 **/
#ifndef  NSMOCR_SEQ_OCR_CHN_BATCH_H
#define  NSMOCR_SEQ_OCR_CHN_BATCH_H

#include "seq_ocr_chn.h"

namespace nsseqocrchn
{
class CSeqModelChBatch : public CSeqModelCh
{
public:
    // model_type = 0: use chinese and english model
    // model_type = 1: only use chinese model
    // fast_flag = true: call the fast strategy version
    // fast_flag = false: call the slow strategy version
    // currently, it does not work
    int recognize_image_batch(const std::vector<st_batch_img> vec_imgs,\
            int model_type,
            bool fast_flag,
            std::vector<SSeqLEngLineInfor> &out_line_vec);

    int recognize_image_batch_only_eng(const std::vector<st_batch_img> vec_imgs,\
            int model_type,
            bool fast_flag,
            std::vector<SSeqLEngLineInfor> &out_line_vec);
protected:
    int divide_dect_imgs(const std::vector<st_batch_img> &indectVec,
        std::vector<st_batch_img> &vec_row_imgs,
        std::vector<st_batch_img> &vec_column_imgs,
        std::vector<int> &vec_row_idxs,
        std::vector<int> &vec_column_idxs);

    void recognize_line_chn(const std::vector<st_batch_img> vec_imgs,\
        CSeqLAsiaWordReg *row_recog_processor, 
        CSeqLAsiaWordReg *col_recog_processor, 
        std::vector<SSeqLEngLineInfor> &out_line_vec);

    void recognize_line_row_chn(\
        const std::vector<st_batch_img> &vec_row_imgs,\
        CSeqLAsiaWordReg *recog_processor, std::vector<SSeqLEngLineInfor> &out_line_vec);

    void recognize_line_col_chn(
        const std::vector<st_batch_img> &vec_col_imgs,\
        CSeqLAsiaWordReg *recog_processor, std::vector<SSeqLEngLineInfor> &out_line_vec);

    void rotate_col_recog_rst(std::vector<SSeqLEngLineInfor> in_col_line_vec,
        std::vector<CvRect> dect_rect_vec,  std::vector<SSeqLEngLineInfor> &out_col_line_vec);
protected:
    // recognize with english module
    void recognize_line_eng_batch(const std::vector<st_batch_img> &vec_imgs,\
        CSeqLEngWordReg *recog_processor_en,\
        int *ptable_flag, std::vector<SSeqLEngLineInfor> &out_line_vec);
    
    //sunwanqi add
    void recognize_line_eng_batch_whole(const std::vector<st_batch_img> &vec_imgs,
      CSeqLEngWordReg *recog_processor_en,\
     int *ptable_flag, std::vector<SSeqLEngLineInfor> &out_line_vec);

    void recognize_row_whole_eng_batch_v3(const IplImage *image,
      CSeqLEngWordReg *recog_processor_en,
      bool extend_flag,
      SSeqLEngLineInfor &out_line);

    void recognize_row_whole_eng_batch(const IplImage *image,
         CSeqLEngWordReg *recog_processor_en,
         bool extend_flag,
         const SSeqLEngLineInfor &out_line_chn, SSeqLEngLineInfor &out_line);

    void recognize_row_piece_eng_batch(const IplImage *image, 
        CSeqLEngWordReg *recog_processor_en, bool extend_flag, 
        const SSeqLEngLineInfor &out_line_chn,
        SSeqLEngLineInfor &out_line);

    int recognize_eng_row_batch(const IplImage *image, 
        std::vector<SSeqLESegDecodeSScanWindow> in_eng_line_dect_vec,
        bool extend_flag,
        CSeqLEngWordReg *recog_processor_en,
        std::vector<SSeqLEngLineInfor>& out_line_vec_en);

    int merge_seg_line(std::vector<SSeqLEngLineRect> line_rect_vec,
            std::vector<SSeqLEngLineInfor> in_line_vec,
            std::vector<SSeqLEngLineInfor>& out_line_vec);

    void recognize_line_seg(const std::vector<st_batch_img> vec_row_imgs,\
        std::vector<SSeqLEngLineRect> line_rect_vec,\
        CSeqLAsiaWordReg *recog_processor,\
        std::vector<SSeqLEngLineInfor>& out_line_vec);
};
};
#endif  //__SEQ_OCR_CHN_BATCH_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
