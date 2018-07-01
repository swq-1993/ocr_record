/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file seq_ocr_model.h
 * @author xieshufu(com@baidu.com)
 * @date 2015/11/25 14:29:10
 * @brief 
 *  
 **/

#ifndef  NMOCR_SEQ_OCR_MODEL_H
#define  NMOCR_SEQ_OCR_MODEL_H

#include "esc_searchapi.h"
#include "DictCalFactory.h"
#include "SeqLEngWordRecog.h"
#include "ocrattr_model.h"

namespace ns_seq_ocr_model {
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
    int init(char *model_name_ch, char *model_name_ch_fast,
        char *inter_lm_target_name_ch,
        char *esp_model_conf_name_ch, char *inter_lm_trie_name_ch,\
        char *table_name_ch,\
        char *model_name_en, char *esp_model_conf_name_en, char *inter_lm_trie_name_en,\
        char *inter_lm_target_name_en, char *table_name_en,\
        char *model_name_ch_wm);

    // initialize the chinese ocr model
    int init_chn_recog_model(char *model_name_ch, char *model_name_ch_fast,
        char *inter_lm_target_name_ch,
        char *esp_model_conf_name_ch, char *inter_lm_trie_name_ch,\
        char *table_name_ch, bool use_paddle_flag, bool use_gpu_flag);
    
    // initialize the single char chinese ocr model
    int init_chn_char_recog_model(char *model_name_ch, char *table_name_ch);
    // initialize the enhance recognition model
    int init_recog_model_enhance(char *model_name, char *table_name);

    // initialize the english ocr model
    int init_eng_recog_model(char *model_name_en,\
        char *esp_model_conf_name_en, char *inter_lm_trie_name_en,\
        char *inter_lm_target_name_en, char *table_name_en,\
        bool use_paddle_flag, bool use_gpu_flag);
    // initialize the column recognition model for Chiniese
    int init_col_chn_recog_model(char *model_name_chn, char *table_name_chn);
    // intialize the para file
    int init_para();
    
    // initialize the recognition model for Japanese
    int init_jap_recog_model(char *model_name_jap, char *model_name_col_jap, char *table_name_jap);
    
    // initialize the recognition model for Korean
    int init_kor_recog_model(char *model_name_kor, char *table_name_kor);

    // initialize ocr_attr model for direction classification
    int init_ocr_attr_model();
private:
    void _clear();
    void _destroy();

public:
    esp::esp_engsearch *_m_eng_search_recog_en;
    LangModel::AbstractScoreCal *_m_inter_lm_en;
    CSeqLEngWordRegModel *_m_recog_model_en;

    esp::esp_engsearch *_m_eng_search_recog_ch;
    LangModel::AbstractScoreCal *_m_inter_lm_ch;
    // the large model for more accurate recognition
    CSeqLEngWordRegModel *_m_recog_model_ch;
    // the small model for fast recognition
    CSeqLEngWordRegModel *_m_recog_model_ch_fast;
    
    // the model for waimai receipe recognition
    CSeqLEngWordRegModel *_m_recog_model_ch_wm;

    // the model for column image recognition
    CSeqLEngWordRegModel *_m_recog_model_col_chn;
    
    // the model for Japanese ocr recognition
    CSeqLEngWordRegModel *_m_recog_model_jap;
    // the model for column Japanese ocr recognition
    CSeqLEngWordRegModel *_m_recog_model_col_jap;

    // the model for Korean ocr recognition
    CSeqLEngWordRegModel *_m_recog_model_kor;
    
    // the single chinese char small model for recognition
    CSeqLEngWordRegModel *_m_recog_model_ch_char;
    
    // the enhance recognition model
    CSeqLEngWordRegModel *_m_recog_model_enhance;

    //ocr attr model begin
    ocr_attr::CRecgModel *_m_recog_model_attr;
    //ocr attr model end

};

};

#endif  //__SEQ_OCR_MODEL_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
