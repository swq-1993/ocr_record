/***************************************************************************
 * 
 * Copyright (c) 2016 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
/**
 * @file char_recg.h
 * @author xieshufu(com@baidu.com)
 * @date 2016/09/20 13:55:01
 * @brief 
 *  
 **/

#ifndef  NSMOCR_CHAR_RECG_H
#define  NSMOCR_CHAR_RECG_H

#include <cv.h>
#include <highgui.h>
#include "SeqLEngWordRecog.h"

namespace nsseqocrchn
{
// recognize the row image if the char confidence is low
void recognize_img_char(const IplImage *image,\
    CSeqLEngWordReg *recog_processor,\
    std::vector<SSeqLEngLineInfor> &out_line_vec);
};
#endif  //__CHAR_RECG_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
