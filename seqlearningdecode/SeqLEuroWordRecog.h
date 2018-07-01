/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file SeqLEuroWordRecog.h
 * @author hanjunyu(com@baidu.com)
 * @date 2014/12/15 14:36:47
 * @brief 
 *  
 **/




#ifndef  __SEQLEUROWORDRECOG_H_
#define  __SEQLEUROWORDRECOG_H_

#include "SeqLEngWordRecog.h"

void SeqLEuroTransformHigh2Low(std::string & str);
void SeqLEuroTransformLow2High(std::string & str);
void SeqLEuroTransformFirstChar2High(std::string & str);

class CSeqLEuroWordReg : public CSeqLEngWordReg {
    public:
        CSeqLEuroWordReg(CSeqLEngWordRegModel & inRegModel) : CSeqLEngWordReg(inRegModel) {
        }
        float onePrefixPathUpLowerLogProb(std::string str, float * orgActs,\
                int actLen, int & highLowType);
        int recognizeWordLineRectTotalPerTime(const IplImage * orgImage,\
                SSeqLEngLineRect & lineRect,\
                SSeqLEngLineInfor & outLineInfor, SSeqLEngSpaceConf & spaceConf);
}; 

#endif  //__SEQLEuroWORDRECOG_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
