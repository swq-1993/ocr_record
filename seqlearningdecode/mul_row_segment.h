/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file mul_row_segment.h
 * @author xieshufu(com@baidu.com)
 * @date 2015/06/24 17:46:59
 * @brief 
 *  
 **/

#ifndef APP_SEARCH_VIS_OCR_ALG_SEQLEARNINGDECODE_MUL_ROW_SEGMENT_H
#define APP_SEARCH_VIS_OCR_ALG_SEQLEARNINGDECODE_MUL_ROW_SEGMENT_H

#include "SeqLEngSegmentFun.h"
#include "cv.h"
#include "highgui.h"
#include <vector>

//Input: pImage: image data
//      pRect: the row rectangle 
//Output: vecRect: the vector of row rectangles      
int segment_mulrow_data(IplImage *pImage, SeqLESegDecodeRect *pRect,\
        std::vector<SeqLESegDecodeRect> &vecRect);

#endif  //__MULROWSEGMENT_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
