/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file SeqLEuroLineBMSSWordDecoder.h
 * @author hanjunyu(com@baidu.com)
 * @date 2014/12/15 17:45:40
 * @brief 
 *  
 **/
#ifndef  __SEQLEUROLINEBMSSWORDDECODER_H_
#define  __SEQLEUROLINEBMSSWORDDECODER_H_

#include "SeqLEngLineBMSSWordDecoder.h"
#include "SeqLEuroWordRecog.h"

class CSeqLEuroLineBMSSWordDecoder : public CSeqLEngLineBMSSWordDecoder {
    public:  
        CSeqLEuroLineBMSSWordDecoder();
        SSeqLEngLineInfor decodeLineFunction(SSeqLEngLineInfor & inputLine);
        void extractEuroCandisVector();
    protected:
        void SeqLEuroHighLowOutput(SSeqLEngLineInfor & line);
        void SeqLEuroPostPuntDigWordText(SSeqLEngLineInfor & line);
};

#endif  //__SEQLEuroLINEBMSSWORDDECODER_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
