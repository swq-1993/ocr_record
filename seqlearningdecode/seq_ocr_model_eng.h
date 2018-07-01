/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
/**
 * @file seq_ocr_model_eng.h
 * @author suzhizhong(com@baidu.com)
 * @date 2016/01/04 14:29:10
 * @brief 
 *  
 **/

#ifndef  SEQ_OCR_ENG_SEQ_OCR_MODEL_ENG_H
#define  SEQ_OCR_ENG_SEQ_OCR_MODEL_ENG_H

#include "esc_searchapi.h"
#include "DictCalFactory.h"
#include "SeqLEngWordRecog.h"

namespace seq_ocr_eng {

class CSeqModel
{
public:
    CSeqModel()
    {
        _clear();
    }
    ~CSeqModel()
    {
        _destroy();
    }

public:
    //error code:srv_errno
    int init(char *model_name_en, char *model_name_en_fast, char *esp_model_conf_name_en,
            char *inter_lm_trie_name_en, char *inter_lm_target_name_en, char *table_name_en);

    int init_att(char *att_model_name_en, char *esp_model_conf_name_en,
            char *inter_lm_trie_name_en, char *inter_lm_target_name_en, char *table_name_en);
private:
    void _clear();
    void _destroy();

public:
    esp::esp_engsearch *_m_eng_search_recog_eng;
    LangModel::AbstractScoreCal *_m_inter_lm_eng;
    // the large model for more accurate recognition
    CSeqLEngWordRegModel *_m_recog_model_eng;
    // the small model for fast recognition
    CSeqLEngWordRegModel *_m_recog_model_eng_fast;
    // attention model
    CSeqLEngWordRegModel *_m_recog_model_eng_att;

};

} //namespace

#endif  //__SEQ_OCR_MODEL_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
