/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file seq_ocr_eng.cpp
 * @author suzhizhong(com@baidu.com)
 * @date 2016/01/04 13:29:17
 * @brief 
 *  
 **/
#include "seq_ocr_eng.h"

extern char g_pimage_title[256];
extern char g_psave_dir[256];

namespace seq_ocr_eng{

void CSeqModelEng::_clear() {
    _m_eng_search_recog_eng = NULL;
    _m_inter_lm_eng = NULL;
    _m_recog_model_eng = NULL;
    _m_acc_eng_highlowtype_flag = false;
}

void CSeqModelEng::_destroy() {
    _clear();
}

void CSeqModelEng::set_acc_eng_highlowtype_flag(bool flag) {
    _m_acc_eng_highlowtype_flag = flag;
}
// fast_flag = true: call the fast model version
// fast_flag = false: call the slow model version
int CSeqModelEng::recognize_image(const IplImage *p_image,\
        std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
        bool fast_flag,
        std::vector<SSeqLEngLineInfor> &out_line_vec) {

    int ret = 0;
    if (p_image == NULL || in_line_dect_vec.size() <= 0) {
        ret = -1;
        return ret;
    }

    if (fast_flag) {
        // call the fast version with the small model
        recognize_line_eng(p_image, in_line_dect_vec, \
          _m_recog_model_eng, true, out_line_vec);
    } else {
        // call the slow version with the large model
        recognize_line_eng(p_image, in_line_dect_vec, \
          _m_recog_model_eng, false, out_line_vec);
    }
    
    return ret;
}

int CSeqModelEng::recognize_image_att(const IplImage *p_image,\
        std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,\
        std::vector<SSeqLEngLineInfor> &out_line_vec) {

    int ret = 0;
    if (p_image == NULL || in_line_dect_vec.size() <= 0) {
        ret = -1;
        return ret;
    }

    recognize_line_eng_att(p_image, in_line_dect_vec, \
          _m_recog_model_eng, out_line_vec);
    
    return ret;
}
int CSeqModelEng::init_model(const esp::esp_engsearch* eng_search_recog_en, 
    const LangModel::AbstractScoreCal* inter_lm_en,  
    const CSeqLEngWordRegModel* recog_model_eng) {

    int ret = 0;
    
    _m_eng_search_recog_eng = (esp::esp_engsearch*)eng_search_recog_en; 
    _m_inter_lm_eng = (LangModel::AbstractScoreCal*)inter_lm_en;
    if (recog_model_eng != NULL) {
        CSeqLEngWordRegModel* temp_model_eng = const_cast<CSeqLEngWordRegModel *>(recog_model_eng);
        _m_model_eng = CSeqLEngWordReg(*temp_model_eng);
        _m_recog_model_eng = &_m_model_eng;
    } else {
        _m_recog_model_eng = NULL;
        ret = -1;
    }

    return ret;
}

//recognize the row with English OCR engine
int CSeqModelEng::recognize_line_eng(const IplImage *image,
    std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,
    CSeqLEngWordReg *recog_processor_en,
    bool no_twice_recog,
    std::vector<SSeqLEngLineInfor> &out_line_vec) {

    CSeqLEngWordSegProcessor seg_processor_en;

    seg_processor_en.processOneImage(image, in_line_dect_vec);
    seg_processor_en.wordSegProcessGroupNearby(4);
        
    std::vector<SSeqLEngLineRect> line_rect_vec_en;
    line_rect_vec_en = seg_processor_en.getSegLineRectVec();

    std::vector<SSeqLEngLineInfor> line_infor_vec_en;
    line_infor_vec_en.resize(line_rect_vec_en.size());
    for (unsigned int i = 0; i < line_rect_vec_en.size(); i++) {
        SSeqLEngLineInfor & temp_line = line_infor_vec_en[i];
        // true: do not utilize the 2nd recognition function
        // false: utilize the 2nd recognition function
        recog_processor_en->recognize_eng_word_whole_line(image, line_rect_vec_en[i], \
            temp_line, seg_processor_en.getSegSpaceConf(i), no_twice_recog);
    }

    for (unsigned int i = 0; i < line_infor_vec_en.size(); i++) {
        CSeqLEngLineDecoder * line_decoder = new CSeqLEngLineBMSSWordDecoder; 
        line_decoder->setWordLmModel(_m_eng_search_recog_eng, _m_inter_lm_eng);
        line_decoder->setSegSpaceConf(seg_processor_en.getSegSpaceConf(i));
        line_decoder->set_acc_eng_highlowtype_flag(_m_acc_eng_highlowtype_flag);
        SSeqLEngLineInfor out_line = line_decoder->decodeLine(\
                recog_processor_en, line_infor_vec_en[i]);
        delete line_decoder;
        out_line_vec.push_back(out_line);
        //out_line_vec.push_back(line_infor_vec_en[i]);
    }
        
    return 0;
}

//recognize the row with attention model
int CSeqModelEng::recognize_line_eng_att(const IplImage *image,
    std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec,
    CSeqLEngWordReg *recog_processor_en,
    std::vector<SSeqLEngLineInfor> &out_line_vec) {

    CSeqLEngWordSegProcessor seg_processor_en;

    seg_processor_en.processOneImage(image, in_line_dect_vec);
    seg_processor_en.wordSegProcessGroupNearby(4);
        
    std::vector<SSeqLEngLineRect> line_rect_vec_en;
    line_rect_vec_en = seg_processor_en.getSegLineRectVec();

    std::vector<SSeqLEngLineInfor> line_infor_vec_en;
    line_infor_vec_en.resize(line_rect_vec_en.size());
    for (unsigned int i = 0; i < line_rect_vec_en.size(); i++) {
        SSeqLEngLineInfor & temp_line = line_infor_vec_en[i];
        recog_processor_en->recognize_whole_line_att(image, line_rect_vec_en[i], \
            temp_line, seg_processor_en.getSegSpaceConf(i));
    }
    for (unsigned int i = 0; i < line_infor_vec_en.size(); i++) {
        CSeqLEngLineDecoder * line_decoder = new CSeqLEngLineBMSSWordDecoder; 
        line_decoder->setWordLmModel(_m_eng_search_recog_eng, _m_inter_lm_eng);
        line_decoder->setSegSpaceConf(seg_processor_en.getSegSpaceConf(i));

        // set decode dim
        line_decoder->set_labeldim(recog_processor_en->labelsDim_);

        SSeqLEngLineInfor out_line = line_decoder->decodeline_att(\
                recog_processor_en, line_infor_vec_en[i]);
        delete line_decoder;
        out_line_vec.push_back(out_line);
    }
        
    return 0;
}

} //namespace
/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
