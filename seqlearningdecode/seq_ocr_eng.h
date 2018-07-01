/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file seq_ocr_eng.h
 * @author suzhizhong(com@baidu.com)
 * @date 2016/01/04 13:29:37
 * @brief 
 *  
 **/

#ifndef  SEQ_OCR_ENG_SEQ_OCR_ENG_H
#define  SEQ_OCR_ENG_SEQ_OCR_ENG_H

#include "esc_searchapi.h"
#include "DictCalFactory.h"
#include "SeqLEngSegmentFun.h"
#include "SeqLEngWordRecog.h"
#include "SeqLEuroWordRecog.h"
#include "SeqLEngLineDecodeHead.h" 
#include "SeqLEngLineBMSSWordDecoder.h"
#include "SeqLEuroLineBMSSWordDecoder.h"
#include "SeqLEngSegmentFun.h"
#include "SeqLEngWordSegProcessor.h"
#include <cv.h>
#include <highgui.h>

namespace seq_ocr_eng
{

class CSeqModelEng
{
public:
    CSeqModelEng()
    {
        _clear();
    }
    virtual~ CSeqModelEng()
    {
        _destroy();
    }

public:
    // fast_flag = true: call the fast model version
    // fast_flag = false: call the slow model version
    int recognize_image(const IplImage *p_image,\
            std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
            bool fast_flag,
            std::vector<SSeqLEngLineInfor> &out_line_vec);

    int recognize_image_att(const IplImage *p_image,\
            std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
            std::vector<SSeqLEngLineInfor> &out_line_vec);

    int init_model(const esp::esp_engsearch* eng_search_recog_eng, 
        const LangModel::AbstractScoreCal* inter_lm_eng,  
        const CSeqLEngWordRegModel* recog_model_eng);

    void set_acc_eng_highlowtype_flag(bool flag);

private:
    void _clear();
    void _destroy();

protected:
    int recognize_line_eng(const IplImage *image,
            std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,
            CSeqLEngWordReg *recog_processor_en, bool no_twice_recog,
            std::vector<SSeqLEngLineInfor> &out_line_vec);

    int recognize_line_eng_att(const IplImage *image,
            std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,
            CSeqLEngWordReg *recog_processor_en,
            std::vector<SSeqLEngLineInfor> &out_line_vec);

private:
    esp::esp_engsearch *_m_eng_search_recog_eng;
    LangModel::AbstractScoreCal *_m_inter_lm_eng;
    CSeqLEngWordReg _m_model_eng;
    CSeqLEngWordReg *_m_recog_model_eng;
    bool _m_acc_eng_highlowtype_flag;

};

} //namespace

#endif

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
