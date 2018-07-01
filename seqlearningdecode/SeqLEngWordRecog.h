/**
 * @file SeqLEngWordRecog.h
 * Author: hanjunyu(hanjunyu@baidu.com)
 * Create on: 2014-07-02
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **/

#ifndef  __SEQLENGWORDRECOG_H_
#define  __SEQLENGWORDRECOG_H_

#include "cv.h"
#include "highgui.h"
#include "SeqLDecodeDef.h"
#include "SeqLEngPrefixSearch.h"
#define SPACECODE 127

bool  leftSeqLWordRectFirst(const SSeqLEngWordInfor & r1, const SSeqLEngWordInfor & r2);
int SeqLUnicode2GBK(wchar_t uniChar, char *gbkChar);
int SeqLUnicode2UTF8(wchar_t uniChar, char *utf8Char);
int SeqLSCWToX(const wchar_t *sIn, int nInLen, char *codeType, char *&sOut);
bool SeqLEuroUnicodeIsPunct(int c);

bool SeqLUnicodeIsDigitChar(int c);
bool SeqLUnicodeIsEngChar(int c);

int SeqLEuroUnicodeHigh2Low(int A);
int SeqLEuroUnicodeLow2High(int a); 
void SeqLUTF8Str2UnicodeVec(const char* ps, std::vector<int>& vec);
char* SeqLUnicodeVec2UTF8Str(std::vector<int>& vec);
char* SeqLEuroUTF8StrHigh2Low(const char* ps);
char* SeqLEuroUTF8StrLow2High(const char* ps);

class CSeqLEngWordRegModel{
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

    int seqLCDNNModelTableInit(char *modelPath, char *tablePath, float mean,int height, \
        void *& cdnnModel, unsigned short *&cdnnTable, float &cdnnMean,int &cdnnNormHeight);
    int seqLCDNNModelTableRelease( void **cdnnModel, unsigned short **cdnnTable);

    int seqLCDNNModelTableInitUTF8(char *modelPath, char *tablePath, float mean, int height, \
        void *& cdnnModel, unsigned short *&cdnnTable, float &cdnnMean,\
        int &cdnnNormHeight, char * langType);

    CSeqLEngWordRegModel() {
        seqLCdnnModel_ = NULL;
        seqLCdnnTable_ = NULL;
        _use_paddle = false;
        _use_gpu = false;
        _batch_size = 1;
        _attention_decode = false;
    }
    CSeqLEngWordRegModel(const CSeqLEngWordRegModel &model) {
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
    CSeqLEngWordRegModel(char *modelPath, char *tablePath, 
        bool use_paddle_flag, bool use_gpu_flag, float mean, int height, \
        bool useUTF8 = false, char * langType = "Euro") {
        
        seqLCdnnModel_ = NULL;
        seqLCdnnTable_ = NULL;
        _use_paddle = use_paddle_flag;
        _use_gpu = use_gpu_flag;
        _attention_decode = false;
        if (useUTF8) {
            seqLCDNNModelTableInitUTF8(modelPath, tablePath,  mean, height, \
                seqLCdnnModel_, seqLCdnnTable_, seqLCdnnMean_, seqLNormheight_, langType);
        }
        else
        {
            seqLCDNNModelTableInit(modelPath, tablePath,  mean, height, \
                seqLCdnnModel_, seqLCdnnTable_, seqLCdnnMean_, seqLNormheight_);
        }
    }

    // default: it utilize the deepcnn model
    CSeqLEngWordRegModel(char *modelPath, char *tablePath, float mean, int height, \
            bool useUTF8 = false, char * langType = "Euro") {
        seqLCdnnModel_ = NULL;
        seqLCdnnTable_ = NULL;
        _use_paddle = false;
        _use_gpu = false;
        _attention_decode = false;
      if (useUTF8) {
          seqLCDNNModelTableInitUTF8(modelPath, tablePath,  mean, height, \
                  seqLCdnnModel_, seqLCdnnTable_, seqLCdnnMean_, seqLNormheight_, langType);
      }
      else
      {
          seqLCDNNModelTableInit(modelPath, tablePath,  mean, height, \
                  seqLCdnnModel_, seqLCdnnTable_, seqLCdnnMean_, seqLNormheight_ );
      }
    }
    ~CSeqLEngWordRegModel(){
        if (!_use_paddle) {
            if (seqLCdnnModel_!=NULL && seqLCdnnTable_!=NULL) {
                seqLCDNNModelTableRelease(&seqLCdnnModel_, &seqLCdnnTable_); 
            }
        } else {
            if (seqLCdnnModel_) {
                delete seqLCdnnModel_;
                seqLCdnnModel_ = NULL;
            }
            if (seqLCdnnTable_) {
                free(seqLCdnnTable_);
                seqLCdnnTable_ = NULL;
            }
        }
        return ; 
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

class CSeqLEngWordReg {
//protected:
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
protected:    
    void extractDataOneRect(IplImage * image, SSeqLEngRect & tempRect, \
        float *& data, int & width, int & height, int & channel, bool isBlackGdFlag, float horResizeRate=1.0);

    int argMaxActs(float* acts, int numClasses);
    void printInputData(float *data, int len);
    void printOneOutputMatrix(float * mat, int LabelDim, int len);
    std::vector<int> pathElimination( const std::vector<int> & path, int blank );
    int extractOutOneWordInfor(float * matrix, int lablesDim, int len,  SSeqLEngWordInfor & outWord);
    int extractoutonewordinfor(float * matrix, int * output_labels, int len, 
                    SSeqLEngWordInfor & outWord);
    int extractoutonewordinfor(float * matrix, int * output_labels, int labelsDim, 
                    int len,  SSeqLEngWordInfor & outWord);
    int extractoutonewordinfor(float * matrix, int * output_labels, int labelsDim, 
                int len, float * weights_matrix, int weights_size, SSeqLEngWordInfor & outWord);
    std::string pathToString(std::vector<int>& path );
    std::string pathtoaccuratestring(std::vector<int>& path);
    std::string pathToStringHaveBlank(std::vector<int>& path );
    
    int  copyInforFormProjectWordTotal(SSeqLEngWordInfor & perWord, SSeqLEngRect & newRect,\
        SSeqLEngWordInfor & newWord, IplImage * image, int setStartTime=-1, int setEndTime=-1);
    
    int onceRectSeqLPredict( SSeqLEngRect &tempRect, SSeqLEngLineRect & lineRect, IplImage * image, \
        SSeqLEngWordInfor & newWord, float extendRate, float horResizeRate=1.0, bool loadFlag = false);
    
    std::vector<int> stringToPath(const std::string & str);

    std::vector<int> path_elimination(std::vector<int> path, std::vector<int> & path_pos);
    
  public:

    std::vector<int> stringutf8topath(const std::string & str);
    void extendVariationWord(SSeqLEngLineInfor & outLineInfor, int index);

    void pathPostionEstimation(std::vector<int> maxPath, std::vector<int> path, std::vector<int> &startVec, \
                std::vector<int> & endVec, std::vector<float> & centVec, int blank);
    
    void resizeImageToFixHeight(IplImage * & image, const int resizeHeight, float horResizeRate = 1.0);
    void extendSlimImage(IplImage * & image, float widthRate);
    int extract_words(SSeqLEngWordInfor recog_res, SSeqLEngLineInfor & out_line);
    int extract_attention_posistion(SSeqLEngWordInfor& LineWord);
    int extract_words_att_cand(SSeqLEngWordInfor recog_res, SSeqLEngLineInfor & out_line);
    int extract_words_att_space_cand(SSeqLEngWordInfor recog_res, SSeqLEngLineInfor & out_line);
    int add_att_cand_score(SSeqLEngWordInfor recog_res, SSeqLEngLineInfor & out_line);
    void extend_att_word(SSeqLEngLineInfor & outLineInfor);
    int extract_words_att_del_lowprob_cand(SSeqLEngWordInfor recog_res, 
                    SSeqLEngLineInfor & out_line);
    int split_word_by_punct(SSeqLEngLineInfor & line);
    SSeqLEngWordInfor split_word(SSeqLEngWordInfor line, int start, int end, int next_start, bool need_adjust);
    SSeqLEngWordInfor split_word(SSeqLEngWordInfor word,
        int start, int end, int next_start, bool need_adjust, 
        std::vector<int> respond_position, SSeqLEngWordInfor original_word);
    int gen_highlow_type(SSeqLEngWordInfor word);

    CSeqLEngWordReg(){
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
    CSeqLEngWordReg(CSeqLEngWordRegModel & inRegModel){
        seqLCdnnModel_ = inRegModel.seqLCdnnModel_;
        seqLCdnnTable_ = inRegModel.seqLCdnnTable_;
        seqLCdnnMean_  = inRegModel.seqLCdnnMean_;
        seqLNormheight_ = inRegModel.seqLNormheight_;

        tabelStrVec_.clear();
        for (int i = 0; i < inRegModel.tabelStrVec_.size(); i++) {
            tabelStrVec_.push_back(inRegModel.tabelStrVec_[i]);
        }
        tabelLowStrVec_.clear();
        std::cout<<"tabelLowStrVec_.size():"<<inRegModel.tabelLowStrVec_.size()<<std::endl;
        for (int i = 0; i < inRegModel.tabelLowStrVec_.size(); i++) {
            std::cout<<"inRegModel.tabelLowStrVec_[i]:"<<inRegModel.tabelLowStrVec_[i]<<std::endl;
            tabelLowStrVec_.push_back(inRegModel.tabelLowStrVec_[i]);
        }
        std::cout<<"dataDim_:"<<dataDim_<<" "<<"labelsDim_:"<<labelsDim_<<" "<<"blank_:"<<blank_<<std::endl;
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
    bool get_attention_mode() {
        return _m_attention_decode;
    }
 
    ~CSeqLEngWordReg(){
    }
    
    float onePrefixPathLogProbUTF8(const std::string & str, float * orgActs, int actLen);

    virtual float onePrefixPathUpLowerLogProb(std::string str, float * orgActs, int actLen, int &highLowType_);
    virtual int recognizeWordLineRectTotalPerTime(const IplImage * orgImage, SSeqLEngLineRect & lineRect,\
        SSeqLEngLineInfor & outLineInfor, SSeqLEngSpaceConf & spaceConf);

    float onePrefixPathLogProb(const std::string & str, float * orgActs, int actLen);

    public:
    // xie shufu add
    int extract_word_infor_asia(float *matrix, int lablesDim, int len,\
        SSeqLEngWordInfor & outWord);
    int rect_seql_predict_asia(SSeqLEngRect &tempRect, SSeqLEngLineRect &lineRect,\
        const IplImage *image, SSeqLEngWordInfor &tempWord,\
        float horResizeRate);
    void extract_data_rect_asia(const IplImage * image, SSeqLEngRect & tempRect, \
         float *& data, int & width, int & height, int & channel, \
         bool isBlackWdFlag, float horResizeRate);

    void set_candidate_num(int candidate_num);
    // recognize the english function
    // it is called in the chinese ocr module
    int recognize_eng_word_whole_line(const IplImage *orgImage,\
        SSeqLEngLineRect &lineRect, SSeqLEngLineInfor &outLineInfor,\
        SSeqLEngSpaceConf &spaceConf, bool quick_recognize);

    // recognize the english row in batch mode
    int recognize_eng_word_whole_line_batch(const IplImage *orgImage,\
        SSeqLEngLineRect &lineRect, SSeqLEngLineInfor &outLineInfor,\
        SSeqLEngSpaceConf &spaceConf, bool quick_recognize);
    int predict_rect(SSeqLEngRect &tempRect, SSeqLEngLineRect & lineRect, IplImage * image, \
        SSeqLEngWordInfor & tempWord, float extendRate, float horResizeRate);

    int recognize_whole_line_att(const IplImage *orgImage,\
        SSeqLEngLineRect &lineRect, SSeqLEngLineInfor &outLineInfor,\
        SSeqLEngSpaceConf &spaceConf);
    
public:
    // recognize for the single char 
    int extract_single_char_infor(float *matrix, int lablesDim, int len,\
        SSeqLEngWordInfor &outWord);
    int rect_seql_predict_single_char(SSeqLEngRect &tempRect, int black_bg_flag,\
        IplImage *image, SSeqLEngWordInfor &tempWord);
    void extract_data_single_char(IplImage * image, SSeqLEngRect & tempRect, \
        float *&data, int & width, int & height, int & channel, \
        bool isBlackWdFlag);
    
    void set_enhance_recog_model_row(
        /*enhance_recg_model*/CSeqLEngWordRegModel *p_recog_model,
        /*threshold*/float recog_thresh);
    void set_openmp_thread_num(
        /*openmp_thread_num*/int thread_num);
    void set_recog_model_type(
        /*model_type:0,1*/int model_type);
public:
    unsigned short* get_seql_cdnn_table()  {
        return seqLCdnnTable_;
    }
    int get_label_dim() {
        return labelsDim_;
    }

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
    CSeqLEngWordRegModel *_m_enhance_model;
    int _m_openmp_thread_num;
    // model_type flag 0: chn and eng; 1: chn
    int _m_recog_model_type;

    bool _m_attention_decode;
};

bool SeqLLineDecodeIsPunct(char c);

#endif  //__SEQLENGWORDRECOG_H_
