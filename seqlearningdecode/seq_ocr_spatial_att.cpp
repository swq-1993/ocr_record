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

#include "seq_ocr_spatial_att.h"
#include "seql_asia_att_recog.h"
#include "SeqLEngLineDecodeHead.h"
#include "dll_main.h"

namespace seq_ocr_spatial_att {

int CSeqModelSpatialAtt::init_model_spatial_att(const CSeqLAsiaAttRegModel *p_model_spatial_att) 
{
    if (p_model_spatial_att == NULL) {
        std::cerr << "could not initialize spatial attention model" << std::endl;
        return -1;
    }
    
    // initialize recog. model
    if (p_model_spatial_att != NULL) {
        CSeqLAsiaAttRegModel *temp_recog_model = 
            const_cast<CSeqLAsiaAttRegModel*>(p_model_spatial_att);
        _m_recog_spatial_att = CSeqLAsiaAttReg(*temp_recog_model);
        _p_m_recog_spatial_att = &_m_recog_spatial_att;
    } else {
        _p_m_recog_spatial_att = NULL;
        return -1;
    }

    return 0;
}

int CSeqModelSpatialAtt::recognize_image_spatial_att(const IplImage *p_image, 
        SSeqLEngLineInfor &out_line_vec)
{
    int ret = 0;
    if (p_image == NULL) {
        ret = -1;
        return ret;
    }
    
    SSeqLEngSpaceConf space_conf;
    ret = _m_recog_spatial_att.recognize_lines(p_image, 
            out_line_vec, space_conf);
    
    return ret;
}

void CSeqModelSpatialAtt::_clear() 
{
    _p_m_recog_spatial_att = NULL;
}

void CSeqModelSpatialAtt::_destroy() 
{
}

} //namespace
/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
