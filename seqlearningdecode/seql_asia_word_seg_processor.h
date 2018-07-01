/***************************************************************************
 * 
 * Copyright (c) 2016 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file seql_asia_word_seg_processor.h
 * @author xieshufu(com@baidu.com)
 * @date 2016/07/04 15:23:04
 * @brief 
 *  
 **/

#ifndef  APP_SEARCH_VIS_OCR_ALG_SEQLEARNINGDECODE_SEQL_ASIA_WORD_SEG_PROCESSOR_H
#define  APP_SEARCH_VIS_OCR_ALG_SEQLEARNINGDECODE_SEQL_ASIA_WORD_SEG_PROCESSOR_H

#include "SeqLDecodeDef.h"
#include "SeqLEngSegmentFun.h"
#include "cv.h"
#include "highgui.h"

class CSeqLAsiaWordSegProcessor{
public:
    CSeqLAsiaWordSegProcessor() {
        _m_pinput_img = NULL;
        _m_extend_flag = true;
        _m_row_segment_flag = true;
    }
    ~CSeqLAsiaWordSegProcessor() {
        _m_pinput_img = NULL;
    }
public:
    int set_file_detect_line_rects(const std::vector<SSeqLESegDecodeSScanWindow> & lineDectVec);

    void word_group_nearby_adaptive(int groupWidth);
    // get the combined rectangle in the row image
    std::vector<SSeqLEngLineRect> & get_seg_line_rect_vec() {
        return _m_word_line_rect_vec;
    }
    std::vector<SSeqLESegDecodeSScanWindow> & get_detect_line_vec(){
        return _m_in_line_dect_vec;
    }
    
    std::vector<SSeqLEngLineRect> & get_line_cc_vec()  {
        return _m_in_seg_line_rect_vec;
    }
public:
    void process_mulrow(const IplImage *pcolor_img, \
            std::vector<SSeqLESegDecodeSScanWindow> &indectVec);

    int gen_dect_line_rect(int iWidth, int iHeight);
    
    // interface for different modules
    // it needs to set the suitable parameter values
    // otherwise, the default parameter is used
    void set_exd_parameter(bool extend_flag);
    // set the row segment flag
    void set_row_seg_parameter(bool row_segment_flag);

private:
    // process the dectection results 
    // row segmentation module might be called
    int proc_dect_rows(const std::vector<SSeqLESegDecodeSScanWindow> &indectVec,
        std::vector<SeqLESegDecodeRect> &vec_row_lines,
        IplImage *gray_img);

    // segment in the horizontal row / vertical column
    // hor_flag:true the horizontal row
    // hor_flag:false the vertical column
    int segment_detect_lines(unsigned char *gim, int gw, int gh,
        std::vector<SeqLESegDecodeRect> &vec_lines, 
        bool hor_flag, std::vector<SSeqLEngLineRect> &vec_seg_line_rect);

    // draw the cc result
    void draw_cc_rst(const IplImage *pOrgColorImg,
        std::vector<SeqLESegDecodeRect> &vec_row_lines);

    // combine the rectangle in the row/column
    void group_line_rects(const std::vector<SSeqLEngLineRect> &seg_line_rect_vec,
        int group_width, 
        std::vector<SSeqLEngLineRect> &combine_line_rect_vec);
private:
    void combine_rect_set_ada(const SSeqLEngLineRect &tempOrgLine, SSeqLEngLineRect &groupTempLine);
    void cal_row_extend_range(std::vector<SeqLESegDecodeRect> &vecLines, int iImageH);

    std::vector<SSeqLESegDecodeSScanWindow> _m_in_line_dect_vec;
    std::vector<SSeqLEngLineRect> _m_in_seg_line_rect_vec;
    std::vector<SSeqLEngLineRect> _m_word_line_rect_vec;
    IplImage *_m_pinput_img;
private:
    // true: the region will be extended
    // false: the region will not be extended
    // this is mainly used for english ocr under "CHN-ENG" request
    // the piecewise recognition will introduce noise text 
    bool _m_extend_flag;
    
    // true: call the mul-row segmentation module
    // false: do not call the mul-row segmentation module
    bool _m_row_segment_flag;
};

#endif  //__SEQL_ASIA_WORD_SEG_PROCESSOR_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
