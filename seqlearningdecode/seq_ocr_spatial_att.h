/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
/**
 * @file seq_ocr_spatial_att.h
 * @author sunyipeng(com@baidu.com)
 * @date 2017/12/04 11:19:11
 * @brief 
 *  
 **/

#ifndef  SEQLEARNINGDECODE_SEQ_OCR_SPATIAL_ATT_H
#define  SEQLEARNINGDECODE_SEQ_OCR_SPATIAL_ATT_H

#include "DictCalFactory.h"
#include "seql_asia_att_recog.h"

namespace seq_ocr_spatial_att {

class CSeqModelSpatialAtt
{
public:
    CSeqModelSpatialAtt()
    {
        _clear();
    }
    ~CSeqModelSpatialAtt()
    {
        _destroy();
    }

public:
    //error code:srv_errno
    int init_model_spatial_att(const CSeqLAsiaAttRegModel * p_model_spatial_att);
    int recognize_image_spatial_att(const IplImage *p_image,
            SSeqLEngLineInfor &out_line_vec);

private:
    void _clear();
    void _destroy();

public:
    // attention model
    //CSeqLAsiaAttRegModel *_m_recog_model_spatial_att;
    CSeqLAsiaAttReg _m_recog_spatial_att;
    CSeqLAsiaAttReg * _p_m_recog_spatial_att;
};

} //namespace

#endif  //__SEQ_OCR_MODEL_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
