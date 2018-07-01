/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file SeqLEngWordSegProcessor.h
 * @author hanjunyu(com@baidu.com)
 * @date 2014/06/20 11:55:47
 * @brief 
 *  
 **/

#ifndef  __SEQLENGWORDSEGPROCESSOR_H_
#define  __SEQLENGWORDSEGPROCESSOR_H_

#include "SeqLDecodeDef.h"
#include "SeqLEngSegmentFun.h"
#include "cv.h"
#include "highgui.h"

class CSeqLEngWordSegProcessor{
    public:
        CSeqLEngWordSegProcessor() {
            _m_pinput_img = NULL;
            _m_extend_flag = true;
            _m_row_segment_flag = true;
        }
        ~CSeqLEngWordSegProcessor() {
            _m_pinput_img = NULL;
        }
    public:
        int loadFileWordSegRects( std::string fileName );
        int loadFileDetectLineRects( std::string fileName );
        int set_file_detect_line_rects(const std::vector<SSeqLESegDecodeSScanWindow> & lineDectVec);

        /* group the nearby (groupWidth) seg wins to a word */
        void wordSegProcessGroupNearby(int groupWidth);

        //void word_group_nearby_adaptive(int groupWidth);
        int save_file_word_wins(std::string fileName);
        // get the combined rectangle in the row image
        std::vector<SSeqLEngLineRect> & getSegLineRectVec(){
            return wordLineRectVec_;
        }
        std::vector<SSeqLESegDecodeSScanWindow> & getDetectLineVec(){
            return inLineDectVec_;
        }
        
        SSeqLEngSpaceConf & getSegSpaceConf(int index){
            return spaceConfVec_[index];
        }

        std::vector<SSeqLEngLineRect> & get_line_cc_vec()  {
            return inSegLineRectVec_;
        }
        void processOneImage(const IplImage * image, std::vector<SSeqLESegDecodeSScanWindow> & indectVec);
    public:
        void process_image_eng_ocr(const IplImage * image,\
                std::vector<SSeqLESegDecodeSScanWindow> &indectVec);

        int gen_dect_line_rect(int iWidth, int iHeight);
        
        // interface for different modules
        // it needs to set the suitable parameter values
        // otherwise, the default parameter is used
        void set_exd_parameter(bool extend_flag);
        // set the row segment flag
        void set_row_seg_parameter(bool row_segment_flag);

    private:
        void EstimateSpaceTh(SSeqLEngSegmentInfor & segInfor, SSeqLEngSpaceConf & spaceConf );
        bool isRealWordCutPoint(SSeqLEngRect & a, SSeqLEngRect & b,  SSeqLEngSpaceConf & spaceConf );

        std::string fileName_;
        std::vector<SSeqLEngSegmentInfor> segInforVec_;
        std::vector<SSeqLEngSpaceConf>    spaceConfVec_;
        std::vector<SSeqLESegDecodeSScanWindow> inLineDectVec_;
        std::vector<SSeqLEngLineRect> inSegLineRectVec_;
        std::vector<SSeqLEngLineRect> wordLineRectVec_;
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

#endif  //__SEQLENGWORDSEGPROCESSOR_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
