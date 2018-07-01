/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file reform_en_rst.h
 * @author xieshufu(com@baidu.com)
 * the function is implemented by xiaohang
 * @date 2015/09/10 10:03:51
 * @brief 
 *  
 **/

#ifndef  APP_SEARCH_VIS_OCR_ALG_SEQLEARNINGDECODE_REFORM_EN_RST_H
#define  APP_SEARCH_VIS_OCR_ALG_SEQLEARNINGDECODE_REFORM_EN_RST_H

#include "SeqLDecodeDef.h"

namespace nsseqocrchn
{
void reform_line_infor_vec(std::vector<SSeqLEngLineInfor> &inLineInforVec,\
        std::vector<SSeqLEngLineInfor> &outLineInforVec);

// xie shufu add
// this function will save both word and char information
void reform_line_infor_vec_split_char(std::vector<SSeqLEngLineInfor> &inLineInforVec,\
     int image_width, std::vector<SSeqLEngLineInfor> & outLineInforVec);

// compute the position of each character
void est_line_split_char(std::vector<SSeqLEngLineInfor> &inLineInforVec,\
     int image_width, std::vector<SSeqLEngLineInfor> &outLineInforVec);

int formulate_output(const std::vector<SSeqLEngLineInfor> &in_line_vec,
    std::vector<SSeqLEngLineInfor> &out_line_vec);

int formulate_output_resnet(const std::vector<SSeqLEngLineInfor> &in_line_vec,
    std::vector<SSeqLEngLineInfor> &out_line_vec);
};

#endif  //__REFORM_EN_RST_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
