/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file seq_ocr_model_eng.cpp
 * @author suzhizhong(com@baidu.com)
 * @date 2016/01/04 14:29:40
 * @brief 
 *  
 **/

#include "seq_ocr_model_eng.h"
#include "SeqLEngLineDecodeHead.h"
#include "dll_main.h"

namespace seq_ocr_eng {

int CSeqModel::init(char *model_name_eng, char *model_name_eng_fast,
        char *esp_model_conf_name_eng, char *inter_lm_trie_name_eng,
        char *inter_lm_target_name_eng, char *table_name_eng) {

    // load English ocr engine and table
    _m_eng_search_recog_eng =  new esp::esp_engsearch;
    if (_m_eng_search_recog_eng->esp_init(esp_model_conf_name_eng) < 0)  {
        std::cerr << "Failed to load the english spell check module!" << std::endl;
        return -1;
    }

    _m_recog_model_eng = new CSeqLEngWordRegModel(model_name_eng,
            table_name_eng, 127.5, 48);
    if (_m_recog_model_eng == NULL) {
        std::cerr << "could not initialize English engine model" << std::endl;
        return -1;
    }

    _m_recog_model_eng_fast = new CSeqLEngWordRegModel(model_name_eng_fast,
            table_name_eng, 127.5, 48);
    if (_m_recog_model_eng_fast == NULL) {
        std::cerr << "could not initialize fast English engine model" << std::endl;
        return -1;
    }

    // load English language model
    const LangModel::AbstractScoreCal & interWordLm_eng = \
        LangModel::GetModel(LangModel::CalType_CompactTrie,
                inter_lm_trie_name_eng, 3, true, true, SEQLENGINTRAGRAMOOPRO,
                inter_lm_target_name_eng);
    _m_inter_lm_eng = (LangModel::AbstractScoreCal*) & interWordLm_eng;
    if (_m_inter_lm_eng == NULL) {
        std::cerr << "could not initialize English language model" << std::endl;
        return -1;
    }
    std::cout << "english model initialized ok!" << std::endl;

    return 0;
}

int CSeqModel::init_att(char *model_name_eng, char *esp_model_conf_name_eng,
        char *inter_lm_trie_name_eng, char *inter_lm_target_name_eng, char *table_name_eng) {

    // load English ocr engine and table
    _m_eng_search_recog_eng =  new esp::esp_engsearch;
    if (_m_eng_search_recog_eng->esp_init(esp_model_conf_name_eng) < 0)  {
        std::cerr << "Failed to load the english spell check module!" << std::endl;
        return -1;
    }

    _m_recog_model_eng_att = new CSeqLEngWordRegModel(model_name_eng,
            table_name_eng, true, false, 127.5, 48);

    // set attention decode
    _m_recog_model_eng_att->set_decode_mode(true);
    if (_m_recog_model_eng_att == NULL) {
        std::cerr << "could not initialize English engine model" << std::endl;
        return -1;
    }

    // load English language model
    const LangModel::AbstractScoreCal & interWordLm_eng = \
        LangModel::GetModel(LangModel::CalType_CompactTrie,
                inter_lm_trie_name_eng, 3, true, true, SEQLENGINTRAGRAMOOPRO,
                inter_lm_target_name_eng);
    _m_inter_lm_eng = (LangModel::AbstractScoreCal*) & interWordLm_eng;
    if (_m_inter_lm_eng == NULL) {
        std::cerr << "could not initialize English language model" << std::endl;
        return -1;
    }
    std::cout << "english model initialized ok!" << std::endl;

    int ret = init_para_file();
    if (ret != 0) {
        std::cerr << "could not initlize the parameter file!" << std::endl;
        return -1;
    }

    return 0;
}

void CSeqModel::_clear() {

    _m_eng_search_recog_eng = NULL;
    _m_inter_lm_eng = NULL;
    _m_recog_model_eng = NULL;
    _m_recog_model_eng_fast = NULL;
    _m_recog_model_eng_att = NULL;

}

void CSeqModel::_destroy() {
    
    LangModel::RemoveModel();
    
    if (_m_eng_search_recog_eng) {
        delete _m_eng_search_recog_eng;
    }
    if (_m_recog_model_eng) {
        delete _m_recog_model_eng;
    }
    if (_m_recog_model_eng_fast) {
        delete _m_recog_model_eng_fast;
    }
    if (_m_recog_model_eng_att) {
        delete _m_recog_model_eng_att;
    }
    
    _clear();
}

} //namespace
/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
