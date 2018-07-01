/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file seq_ocr_model.cpp
 * @author xieshufu(com@baidu.com)
 * @date 2015/11/25 14:29:40
 * @brief 
 *  
 **/

#include "seq_ocr_model.h"
#include "SeqLEngLineDecodeHead.h"
#include "dll_main.h"

namespace ns_seq_ocr_model {

int CSeqModel::init(char *model_name_ch, char *model_name_ch_fast,
        char *esp_model_conf_name_ch, char *inter_lm_trie_name_ch,\
        char *inter_lm_target_name_ch, char *table_name_ch,\
        char *model_name_en, char *esp_model_conf_name_en, char *inter_lm_trie_name_en,\
        char *inter_lm_target_name_en, char *table_name_en,\
        char *model_name_ch_wm) {
    int ret = 0;
    
    // load the parameter file
    SEQL_COUT << "Initialize parameter file!" << std::endl;
    ret = init_para_file();
    if (ret != 0) {
        std::cerr << "could not initlize the parameter file!" << std::endl;
        return -1;
    }

    // load Chinese ocr engine and table and this model is not used in this version
    _m_eng_search_recog_ch =  new esp::esp_engsearch;

    SEQL_COUT << "Initialize large Chinese model!" << std::endl;
    _m_recog_model_ch = new CSeqLEngWordRegModel(\
            model_name_ch, table_name_ch, 127.5, 48, true);
    if (_m_recog_model_ch == NULL) {
        std::cerr << "could not initialize large Chinese engine model" << std::endl;
        return -1;
    } 

    SEQL_COUT << "Initialize small Chinese model!" << std::endl;
    _m_recog_model_ch_fast = new CSeqLEngWordRegModel(\
            model_name_ch_fast, table_name_ch, 127.5, 48, true);
    if (_m_recog_model_ch_fast == NULL) {
        std::cerr << "could not initialize small Chinese engine model" << std::endl;
        return -1;
    } 
    // load Chinese language model 
    const LangModel::AbstractScoreCal & interWordLm_ch = \
            LangModel::GetModel(LangModel::CalType_CompactTrie, inter_lm_trie_name_ch, 3, \
            true, true, SEQLENGINTRAGRAMOOPRO, inter_lm_target_name_ch);
    _m_inter_lm_ch = (LangModel::AbstractScoreCal*) &interWordLm_ch;
    if (_m_inter_lm_ch == NULL) {
        std::cerr << "could not initialize Chinese language model" << std::endl;
        return -1;
    }

    // load English ocr engine and table
    SEQL_COUT << "Load English spell check module!" << std::endl;
    _m_eng_search_recog_en =  new esp::esp_engsearch;
    if (_m_eng_search_recog_en->esp_init(esp_model_conf_name_en) < 0)  {
        std::cerr << "Failed to load the english spell check module!" << std::endl;
        return -1;
    }

    SEQL_COUT << "Initialize English model!" << std::endl;
    _m_recog_model_en = new CSeqLEngWordRegModel(\
       model_name_en, table_name_en, 127.5, 48);
    if (_m_recog_model_en == NULL) {
        std::cerr << "could not initialize English engine model" << std::endl;
        return -1;
    }

    // load English language model
    SEQL_COUT << "Initialize English language model!" << std::endl;
    const LangModel::AbstractScoreCal & interWordLm_en = \
        LangModel::GetModel(LangModel::CalType_CompactTrie, inter_lm_trie_name_en, 3, \
        true, true, SEQLENGINTRAGRAMOOPRO,  inter_lm_target_name_en);
    _m_inter_lm_en = (LangModel::AbstractScoreCal*) & interWordLm_en;
    if (_m_inter_lm_en == NULL) {
        std::cerr << "could not initialize English language model" << std::endl;
        return -1;
    }

    // load waimai ocr recognition model
    SEQL_COUT << "Initialize waimai model!" << std::endl;
    _m_recog_model_ch_wm = new CSeqLEngWordRegModel(\
            model_name_ch_wm, table_name_ch, 127.5, 48, true);
    if (_m_recog_model_ch_wm == NULL) {
        std::cerr << "could not initialize waimai Chinese engine model" << std::endl;
        return -1;
    } 

    return ret;
}

// initialize the chinese ocr model
int CSeqModel::init_chn_recog_model(char *model_name_ch, char *model_name_ch_fast,
    char *inter_lm_target_name_ch,
    char *esp_model_conf_name_ch, char *inter_lm_trie_name_ch,\
    char *table_name_ch, bool use_paddle_flag, bool use_gpu_flag) {
    
    // load Chinese ocr engine and table and this model is not used in this version
    _m_eng_search_recog_ch =  new esp::esp_engsearch;
    _m_recog_model_ch = new CSeqLEngWordRegModel(\
            model_name_ch, table_name_ch, use_paddle_flag, use_gpu_flag, 127.5, 48, true);
    if (_m_recog_model_ch == NULL) {
        std::cerr << "could not initialize large Chinese engine model" << std::endl;
        return -1;
    } 
    int batch_size = g_ini_file->get_int_value("GPU_BATCH_SIZE", IniFile::_s_ini_section_global);
    _m_recog_model_ch->set_batch_size(batch_size);
    std::cout << "chinese large model initialized ok!" << std::endl;

    _m_recog_model_ch_fast = new CSeqLEngWordRegModel(\
            model_name_ch_fast, table_name_ch, use_paddle_flag, use_gpu_flag, 127.5, 48, true);
    if (_m_recog_model_ch_fast == NULL) {
        std::cerr << "could not initialize small Chinese engine model" << std::endl;
        return -1;
    } 
    _m_recog_model_ch_fast->set_batch_size(batch_size);
    // load Chinese language model 
    const LangModel::AbstractScoreCal & interWordLm_ch = \
            LangModel::GetModel(LangModel::CalType_CompactTrie, inter_lm_trie_name_ch, 3, \
            true, true, SEQLENGINTRAGRAMOOPRO, inter_lm_target_name_ch);
    _m_inter_lm_ch = (LangModel::AbstractScoreCal*) &interWordLm_ch;
    if (_m_inter_lm_ch == NULL) {
        std::cerr << "could not initialize Chinese language model" << std::endl;
        return -1;
    }
    std::cout << "chinese small model initialized ok!" << std::endl;
    
    return 0;
}

// initialize the english ocr model
int CSeqModel::init_eng_recog_model(char *model_name_en,\
    char *esp_model_conf_name_en, char *inter_lm_trie_name_en,\
    char *inter_lm_target_name_en, char *table_name_en,\
    bool use_paddle_flag, bool use_gpu_flag) {

    // load English ocr engine and table
    _m_eng_search_recog_en =  new esp::esp_engsearch;
    if (_m_eng_search_recog_en->esp_init(esp_model_conf_name_en) < 0)  {
        std::cerr << "Failed to load the english spell check module!" << std::endl;
        return -1;
    }
    _m_recog_model_en = new CSeqLEngWordRegModel(\
       model_name_en, table_name_en, use_paddle_flag, use_gpu_flag, 127.5, 48);
    if (_m_recog_model_en == NULL) {
        std::cerr << "could not initialize English engine model" << std::endl;
        return -1;
    }
    int batch_size = 1;
    if (use_gpu_flag) {
        batch_size = 2;
    }
    std::cout << "batch_size " << batch_size << std::endl;
    _m_recog_model_en->set_batch_size(batch_size);

    // load English language model
    const LangModel::AbstractScoreCal & interWordLm_en = \
        LangModel::GetModel(LangModel::CalType_CompactTrie, inter_lm_trie_name_en, 3, \
        true, true, SEQLENGINTRAGRAMOOPRO,  inter_lm_target_name_en);
    _m_inter_lm_en = (LangModel::AbstractScoreCal*) & interWordLm_en;
    if (_m_inter_lm_en == NULL) {
        std::cerr << "could not initialize English language model" << std::endl;
        return -1;
    }
    std::cout << "english model initialized ok!" << std::endl;
    
    return 0;
}
    
// initialize the single char chinese ocr model
int CSeqModel::init_chn_char_recog_model(char *model_name_ch, char *table_name_ch) {

    _m_recog_model_ch_char = new CSeqLEngWordRegModel(\
            model_name_ch, table_name_ch, 127.5, 48, true);
    if (_m_recog_model_ch_char == NULL) {
        std::cerr << "could not initialize single char Chinese recognition model" << std::endl;
        return -1;
    } 
    std::cout << "single char chinese recognition model initialized ok!" << std::endl;
    
    return 0;
}

// initialize the single char chinese ocr model
int CSeqModel::init_recog_model_enhance(char *model_name, char *table_name) {

    _m_recog_model_enhance = new CSeqLEngWordRegModel(\
            model_name, table_name, 127.5, 48, true);
    if (_m_recog_model_enhance == NULL) {
        std::cerr << "could not initialize enhance recognition model" << std::endl;
        return -1;
    } 
    std::cout << "enhance recognition model initialized ok!" << std::endl;
    
    return 0;
}

// initialize the column recognition model for Chiniese
int CSeqModel::init_col_chn_recog_model(char *model_name_col_chn, char *table_name_chn) {
    SEQL_COUT << "Initialize column Chinese recognition model!" << std::endl;
    _m_recog_model_col_chn = new CSeqLEngWordRegModel(\
            model_name_col_chn, table_name_chn, 127.5, 48, true);
    if (_m_recog_model_col_chn == NULL) {
        std::cerr << "could not initialize column Chinese recognition model" << std::endl;
        return -1;
    } 
    return 0;
}

// initialize the recognition model for Japanese
int CSeqModel::init_jap_recog_model(char *model_name_jap,
    char *model_name_col_jap, char *table_name_jap) {
    _m_recog_model_jap = new CSeqLEngWordRegModel(\
            model_name_jap, table_name_jap, 127.5, 48, true);
    if (_m_recog_model_jap == NULL) {
        std::cerr << "could not initialize row Japanese recognition model" << std::endl;
        return -1;
    } 
    std::cout << "row Japanese recognition model initialized ok!" << std::endl;
    _m_recog_model_col_jap = new CSeqLEngWordRegModel(\
            model_name_col_jap, table_name_jap, 127.5, 48, true);
    if (_m_recog_model_col_jap == NULL) {
        std::cerr << "could not initialize column Japanese recognition model" << std::endl;
        return -1;
    } 
    return 0;
}

// initialize the recognition model for Korean
int CSeqModel::init_kor_recog_model(char *model_name_kor, char *table_name_kor) {
    _m_recog_model_kor = new CSeqLEngWordRegModel(\
            model_name_kor, table_name_kor, 127.5, 48, true);
    if (_m_recog_model_kor == NULL) {
        std::cerr << "could not initialize Korean recognition model" << std::endl;
        return -1;
    } 
    std::cout << "Korean recognition model initialized ok!" << std::endl;
    return 0;
}

// initialize ocr_attr model for direction classification
int CSeqModel::init_ocr_attr_model() {
    char model_name[256];
    char table_name[256];

    snprintf(table_name, 256, "data/attr_data/table_10784");
    snprintf(model_name, 256, "data/attr_data/ocr_attr.bin");
    _m_recog_model_attr = new ocr_attr::CRecgModel();
    int init_val = _m_recog_model_attr->init_model(\
        model_name, table_name, 127.5, 48);
    if (init_val != 0) {
        return -1;
    }
    return 0;
}


// initialize the para file
int CSeqModel::init_para() {
    int ret = 0;
    // load the parameter file
    ret = init_para_file();
    if (ret != 0) {
        std::cerr << "could not initlize the parameter file!" << std::endl;
        return -1;
    }
    std::cout << "parameter file initialized ok!" << std::endl;
    return ret;
}

void CSeqModel::_clear() {

    _m_eng_search_recog_ch = NULL;
    _m_inter_lm_ch = NULL;
    _m_recog_model_ch = NULL;
    _m_recog_model_ch_fast = NULL;

    _m_eng_search_recog_en = NULL;
    _m_inter_lm_en = NULL;
    _m_recog_model_en = NULL;

    _m_recog_model_ch_wm = NULL;
    
    _m_recog_model_col_chn = NULL;
    
    // the model for Japanese ocr recognition
    _m_recog_model_jap = NULL;
    // the model for Korean ocr recognition
    _m_recog_model_kor = NULL;
    // the model for Chinese single char model 
    _m_recog_model_ch_char = NULL;
    _m_recog_model_enhance = NULL;
}

void CSeqModel::_destroy() {
    
    LangModel::RemoveModel();
    if (_m_eng_search_recog_ch) {
        delete _m_eng_search_recog_ch;
    }
    if (_m_recog_model_ch) {
        delete _m_recog_model_ch;
    }
    if (_m_recog_model_ch_fast) {
        delete _m_recog_model_ch_fast;
    }
    if (_m_eng_search_recog_en) {
        delete _m_eng_search_recog_en;
    }
    if (_m_recog_model_en) {
        delete _m_recog_model_en;
    }
    
    if (_m_recog_model_ch_wm) {
        delete _m_recog_model_ch_wm;
    }
    if (_m_recog_model_col_chn) {
        delete _m_recog_model_col_chn;
    }
    if (_m_recog_model_jap) {
        delete _m_recog_model_jap;
    }
    if (_m_recog_model_kor) {
        delete _m_recog_model_kor;
    }
    if (_m_recog_model_ch_char) {
        delete _m_recog_model_ch_char;
    }
    if (_m_recog_model_enhance) {
        delete _m_recog_model_enhance;
    }
    free_para_file();

    _clear();
}

};
/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
