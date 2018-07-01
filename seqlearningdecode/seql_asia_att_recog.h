/***************************************************************************
 * 
 * Copyright (c) 2017 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file seql_asia_att_recog.h
 * @author sunyipeng(com@baidu.com)
 * @date 2017/11/17 14:44:38
 * @brief 
 *  
 **/

#ifndef  SEQLEARNINGDECODE_SEQL_ASIA_ATT_RECOG_H
#define  SEQLEARNINGDECODE_SEQL_ASIA_ATT_RECOG_H

#include "cv.h"
#include "highgui.h"
#include "SeqLDecodeDef.h"
#include "SeqLEngPrefixSearch.h"
//#define SPACECODE 127

// The class definition of attention-based recognition model
class CSeqLAsiaAttRegModel {
  
public:
    void * seqLCdnnModel_;
    unsigned short * seqLCdnnTable_;
    float seqLCdnnMean_;
    int seqLNormheight_;
    std::vector<std::string> tabelStrVec_;
    std::vector<std::string> tabelLowStrVec_;
    int dataDim_;
    int labelsDim_;
    int blank_;
    bool _use_paddle;
    bool _use_gpu;
    int _batch_size;
    bool _attention_decode;

    int seql_cdnn_model_table_init(char *modelPath, char *tablePath, float mean, 
            int height, void *& cdnnModel, unsigned short *&cdnnTable, 
            float &cdnnMean, int &cdnnNormHeight);
    int seql_cdnn_model_table_release(void **cdnnModel, unsigned short **cdnnTable);

    int seql_cdnn_model_table_init_utf8(char *modelPath, char *tablePath, float mean,
            int height, void *& cdnnModel, unsigned short *&cdnnTable,
            float &cdnnMean, int &cdnnNormHeight, char * langType);

    CSeqLAsiaAttRegModel() { 
        seqLCdnnModel_ = NULL;
        seqLCdnnTable_ = NULL;
        _use_paddle = true;
        _use_gpu = false;
        _batch_size = 1;
        _attention_decode = true;
    }
    CSeqLAsiaAttRegModel(const CSeqLAsiaAttRegModel &model) {
        seqLCdnnModel_ = model.seqLCdnnModel_;
        seqLCdnnTable_ = model.seqLCdnnTable_;
        seqLCdnnMean_ = model.seqLCdnnMean_;
        seqLNormheight_ = model.seqLNormheight_;
        tabelStrVec_ = model.tabelStrVec_;
        tabelLowStrVec_ = model.tabelLowStrVec_;
        dataDim_ = model.dataDim_;
        labelsDim_ = model.labelsDim_;
        blank_ = model.blank_;
        _use_paddle = model._use_paddle;
        _use_gpu = model._use_gpu;
        _batch_size = model._batch_size;
    }
   
    // it utilize the paddle or deepcnn model	
    // true: use paddle; false: do not use paddle
    CSeqLAsiaAttRegModel(char *modelPath, char *tablePath, 
        bool use_paddle_flag, bool use_gpu_flag, float mean, int height, \
        bool useUTF8 = false, char * langType = "Euro") {
        
        //std::cout << "Model path: " << modelPath << std::endl;
        //std::cout << "Table path: " << tablePath << std::endl;
        
        seqLCdnnModel_ = NULL;
        seqLCdnnTable_ = NULL;
        _use_paddle = use_paddle_flag;
        _use_gpu = use_gpu_flag;
        _attention_decode = false;
        
        if (useUTF8) {
            seql_cdnn_model_table_init_utf8(modelPath, tablePath, mean, height,
                seqLCdnnModel_, seqLCdnnTable_, seqLCdnnMean_, seqLNormheight_, langType);
        }
        else
        {
            seql_cdnn_model_table_init(modelPath, tablePath,  mean, height, \
                seqLCdnnModel_, seqLCdnnTable_, seqLCdnnMean_, seqLNormheight_);
        }
    }

    ~CSeqLAsiaAttRegModel(){
        if (seqLCdnnModel_!=NULL && seqLCdnnTable_!=NULL) {
            seql_cdnn_model_table_release(&seqLCdnnModel_, &seqLCdnnTable_); 
        }
    }
    
    int set_batch_size(int batch_size) {
        _batch_size = 1;
        if (batch_size > 1 && batch_size < 100) {
            _batch_size = batch_size;
        }
        return 0;
    }
    int set_decode_mode(bool attention_decode) {
        _attention_decode = attention_decode;
        return 0;
    }
    bool get_attention_mode() {
        return _attention_decode;
    }

};

// The class definition of attention-based recognizer
class CSeqLAsiaAttReg {

public:	
    void * seqLCdnnModel_;
    unsigned short * seqLCdnnTable_;
    float seqLCdnnMean_;
    int seqLNormheight_;
    std::vector<std::string> tabelStrVec_;
    std::vector<std::string> tabelLowStrVec_;
    int dataDim_;
    int labelsDim_;
    int blank_;
    float extendRate_;
    
    //width and height of the normalized image
    int _m_norm_img_h;
    int _m_norm_img_w;
    int _m_en_ch_model;

    // candidate num
    int _m_cand_num;
    bool _use_paddle;
    bool _use_gpu;
    int _batch_size;
    
    float _m_recog_thresh;
    //CSeqLEngWordRegModel *_m_enhance_model;
    int _m_openmp_thread_num;
    // model_type flag 0: chn and eng; 1: chn
    int _m_recog_model_type;
    bool _m_attention_decode;

public:
    CSeqLAsiaAttReg(){
        seqLCdnnModel_ = NULL;
        seqLCdnnTable_ = NULL;
        _m_en_ch_model = 0;
        _m_cand_num = -1;
        _use_paddle = false;
        _use_gpu = false;
        _batch_size = 1;
        _m_norm_img_h = 0;
        _m_norm_img_w = 0;
        _m_recog_thresh = 0;
        _m_openmp_thread_num = 0;
        _m_recog_model_type = 0;
        _m_attention_decode = false;
    }

    CSeqLAsiaAttReg(CSeqLAsiaAttRegModel & inRegModel){
        seqLCdnnModel_ = inRegModel.seqLCdnnModel_;
        seqLCdnnTable_ = inRegModel.seqLCdnnTable_;
        seqLCdnnMean_  = inRegModel.seqLCdnnMean_;
        seqLNormheight_ = inRegModel.seqLNormheight_;

        tabelStrVec_.clear();
        for (int i = 0; i < inRegModel.tabelStrVec_.size(); i++) {
            tabelStrVec_.push_back(inRegModel.tabelStrVec_[i]);
        }
        tabelLowStrVec_.clear();
        for (int i = 0; i < inRegModel.tabelLowStrVec_.size(); i++) {
            tabelLowStrVec_.push_back(inRegModel.tabelLowStrVec_[i]);
        }
        dataDim_ = inRegModel.dataDim_;
        labelsDim_ = inRegModel.labelsDim_;
        blank_ = inRegModel.blank_;
        extendRate_ = 0.00;
        _m_en_ch_model = 0;
        _m_cand_num = -1;
        _use_paddle = inRegModel._use_paddle;
        _use_gpu = inRegModel._use_gpu;
        _batch_size = inRegModel._batch_size;
        _m_norm_img_h = 0;
        _m_norm_img_w = 0;
        _m_recog_thresh = 0;
        _m_openmp_thread_num = 0;
        _m_recog_model_type = 0;
        _m_attention_decode = inRegModel._attention_decode;
    }
 
    ~CSeqLAsiaAttReg(){
    }
    
    bool get_attention_mode() {
        return _m_attention_decode;
    }

    int extract_words(SSeqLEngWordInfor recog_res, SSeqLEngLineInfor &out_line);
    
    SSeqLEngWordInfor split_word(SSeqLEngWordInfor line, int start, int end,
            int next_start, bool need_adjust);
    
    void resize_image_to_fix_height(IplImage * &image, const int resizeHeight,
            float horResizeRate = 1.0);
    void extend_slim_image(IplImage * &image, float widthRate);
    int gen_highlow_type(SSeqLEngWordInfor word);
    
    int recognize_lines(const IplImage *orgImage,
            SSeqLEngLineInfor &outLineInfor,
            SSeqLEngSpaceConf &spaceConf);

protected:  
    
    void extract_data_from_rect(IplImage * image, SSeqLEngRect &tempRect,
            float * &data, int &width, int &height, int &channel, 
            bool isBlackGdFlag, float horResizeRate = 1.0);

    int extract_word_infor(float * matrix, int lablesDim,
            int len, SSeqLEngWordInfor &outWord);
    
    int extract_word_infor_att(float * matrix, int * output_labels, int labelsDim, 
            int len,  SSeqLEngWordInfor & outWord);
    
    int predict_lines(IplImage *image, SSeqLEngRect &inTempRect,
            SSeqLEngWordInfor &outTempInfor, 
            float extendRate, float horResizeRate = 1.0, bool loadFlag = false);

    std::string path_to_string(std::vector<int> &path);
    
    std::vector<int> string_to_path(const std::string &str);
    
    std::vector<int> path_elimination(std::vector<int> path, std::vector<int> & path_pos);
};

// other functions declare
void load_predict_matrix(int predict_num, float * &matrix, int &len);

#endif  //SEQL_ASIA_ATT_RECOG_H
/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
