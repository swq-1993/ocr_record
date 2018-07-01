/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file SeqLEngLinePatchDecoder.h
 * @author hanjunyu(com@baidu.com)
 * @date 2014/06/18 16:48:49
 * @brief 
 *  
 **/




#ifndef  __SEQLENGLINEPATCHDECODER_H_
#define  __SEQLENGLINEPATCHDECODER_H_

#include "SeqLEngLineDecodeHead.h"

// This decoder is just a adapter of english patch decoder

class CSeqLEngLineADPatchDecoder : public CSeqLEngLineDecoder{
    public:
        CSeqLEngLineADPatchDecoder(){
            std::cout << "Decoder is set to the old second word decoder of EngPatchDecoder " << std::endl;
        }
        SSeqLEngLineInfor decodeLineFunction( SSeqLEngLineInfor & inputLine );
};













#endif  //__SEQLENGLINEPATCHDECODER_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
