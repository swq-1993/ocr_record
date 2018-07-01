/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file SeqLEngLineDecodeHead.h
 * @author hanjunyu(com@baidu.com)
 * @date 2014/06/18 11:37:29
 * @brief 
 *  
 **/




#ifndef  __SEQLENGLINEDECODEHEAD_H_
#define  __SEQLENGLINEDECODEHEAD_H_

#include <iostream>
#include "SeqLDecodeDef.h"
#include "esc_searchapi.h"
#include "DictCalFactory.h"
//#include "SeqLEngWordRecog.h"
//#define SEQLENGINTRAGRAMOOPRO (-40)
#define SEQLENGINTRAGRAMOOPRO (-20)

class CSeqLEngLineDecoder{

    public:
        CSeqLEngLineDecoder(){
          //std::cout << " CSeqLEngLineDecoder() " << std::endl;
          espModel_ = NULL;
          interWordLm_ = NULL;
        }
        virtual ~CSeqLEngLineDecoder(){
          //std::cout << " ~ CSeqLEngLineDecoder() " << std::endl;
        }
        int setWordLmModel( esp::esp_engsearch *inEspModel, LangModel::AbstractScoreCal * inWordLm ){
            espModel_ = inEspModel;
            interWordLm_ = inWordLm;
            return 0;
        }

        virtual SSeqLEngLineInfor decodeLineFunction( SSeqLEngLineInfor & inputLine ){
          std::cerr << "Decoder line fuction is undefined! ";
          return inputLine;
        }
        virtual SSeqLEngLineInfor decodelinefunction_att(SSeqLEngLineInfor & inputLine) {
            std::cerr << "Decoder line fuction is undefined! ";
            return inputLine;
        }
        SSeqLEngLineInfor decodeline_att(void * recognizer,  SSeqLEngLineInfor & inputLine) {
            if (espModel_ == NULL) {
                std::cerr << "espModel is undifined! " << std::endl;
                return inputLine;
            }  
            if (interWordLm_ == NULL) {
                std::cerr << "interWordLm_ is undefined!" << std::endl;
                return inputLine;
            }
            recognizer_ = recognizer;
            return decodelinefunction_att(inputLine);        
        }
        SSeqLEngLineInfor decodeLine(void * recognizer,  SSeqLEngLineInfor & inputLine ){
           if(espModel_ == NULL){
              std::cerr << "espModel is undifined! " << std::endl;
              return inputLine;
           }  
           if( interWordLm_ == NULL){
              std::cerr << "interWordLm_ is undefined!" << std::endl;
              return inputLine;
           }
           recognizer_ = recognizer;
           return decodeLineFunction(inputLine);        
        }

        int setSegSpaceConf(SSeqLEngSpaceConf & spaceConf ){
           spaceConf_ = spaceConf;
        }

        int set_labeldim(int labeldim) {
            _labelsdim = labeldim;
        }

    void set_acc_eng_highlowtype_flag(bool flag) {
        _m_acc_eng_highlowtype_flag = flag;       
    }
    protected:
        void * recognizer_;
        SSeqLEngSpaceConf spaceConf_;
        esp::esp_engsearch *espModel_;
        LangModel::AbstractScoreCal * interWordLm_; 
        int _labelsdim;
        bool _m_acc_eng_highlowtype_flag;
};















#endif  //__SEQLENGLINEDECODEHEAD_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
