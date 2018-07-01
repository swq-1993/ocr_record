/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
/**
 * @file SeqLAsiaLineBMSSWordDecoder.h
 * @author hanjunyu(com@baidu.com)
 * @date 2015/03/17 16:02:02
 * @brief 
 *  
 **/

#ifndef  APP_SEARCH_VIS_OCR_ALG_SEQLEARNINGDECODE_SEQL_ASIA_LINE_BMSS_WORD_DECODER_H
#define  APP_SEARCH_VIS_OCR_ALG_SEQLEARNINGDECODE_SEQL_ASIA_LINE_BMSS_WORD_DECODER_H

#include "SeqLEngLineBMSSWordDecoder.h"
#include "SeqLEuroWordRecog.h"

class CSeqLAsiaLineBMSSWordDecoder {
public:  
    CSeqLAsiaLineBMSSWordDecoder();
    SSeqLEngLineInfor decode_line(void *recognizer, SSeqLEngLineInfor &inputLine);
    int check_valid_line(SSeqLEngLineInfor &inputLine, int &iValid);
    int set_save_low_prob_flag(bool b_save_low_prob_flag);
    // lx added for subtitle
    void set_remove_head_end_noise(bool flag);
    void set_resnet_strategy_flag(bool flag);
protected:
    // new decode function 
    // only the recognition confidence of characters are used
    int line_word_decode();
    // remove the character which are overlapped together
    int remove_overlap_word(std::vector<SSeqLEngWordInfor> &in_word_vec);

private:
    int refine_line_word(std::vector<SSeqLEngWordInfor> &InwordVec_);
    void fill_char_infor(std::vector<SSeqLEngWordInfor> &InwordVec_);

    // true: preserve the char with low probability
    // false: do not preserve the char with low probability
    bool _m_save_low_prob_flag;
    bool _m_remove_head_end_noise;
    bool _m_resnet_strategy_flag;
    SSeqLEngLineInfor _m_input_line;
    SSeqLEngLineInfor _m_output_line;
};

#endif  //__SEQLASIALINEBMSSWORDDECODER_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
