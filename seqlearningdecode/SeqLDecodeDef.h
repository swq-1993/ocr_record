/***************************************************************************
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/



/**
 * @file SeqLDecodeDef.h
 * @author hanjunyu(com@baidu.com)
 * @date 2014/06/16 21:17:16
 * @brief
 *
 **/




#ifndef  __SEQLDECODEDEF_H_
#define  __SEQLDECODEDEF_H_

#include <string>
#include <vector>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <iconv.h>
#include <cv.h>
#include <highgui.h>

//#define COMPUT_TIME

const int SEQL_MAX_IMAGE_WIDTH = 20480;
#define SEQL_MAX_IMAGE_HEIGHT 2048
#define SEQL_MIN_IMAGE_WIDTH 5
#define SEQL_MIN_IMAGE_HEIGHT 5

#define SEQL_COUT   \
    std::cout << "[" << __FILE__ << "]["    \
              << __FUNCTION__ << "]["       \
              << __LINE__ << "] "

#define SEQL_CERR   \
    std::cerr << "[" << __FILE__ << "]["    \
              << __FUNCTION__ << "]["       \
              << __LINE__ << "] "

struct SSeqLESegDetectWin
{
    int left;
    int right;
    int top;
    int bottom;
    int rnum;

    SSeqLESegDetectWin() {
        left = 0;
        right = 0;
        top = 0;
        bottom = 0;
        rnum = 0;
    }
};

struct SSeqLESegDecodeSScanWindow
{
    int left;
    int right;
    int top;
    int bottom;
    float detectConf;
    int isBlackWdFlag;
    std::vector<SSeqLESegDetectWin> winSet;
    int rnum;

    SSeqLESegDecodeSScanWindow() {

        left = 0;
        right = 0;
        top = 0;
        bottom = 0;
        detectConf = 0;
        isBlackWdFlag = 1;
        winSet.clear();
        rnum = 0;
    }
};

struct SSeqLEngRect{
    int left_;
    int top_;
    int wid_;
    int hei_;
    int leftSpace_;
    int rightSpace_;
    bool bePredictFlag_;
    bool sigWordFlag_;
    int compNum_;
    float score_;
    SSeqLEngRect() {
        left_ = 0;
        top_ = 0;
        wid_ = 0;
        hei_ = 0;
        leftSpace_ = 0;
        rightSpace_ = 0;
        bePredictFlag_ = true;
        sigWordFlag_ = true;
        compNum_ = 0;
        score_ = 0;
    }
};

struct SSeqLEngLineRect{
    std::vector<SSeqLEngRect> rectVec_;
    int isBlackWdFlag_;
    int left_;
    int top_;
    int wid_;
    int hei_;

    SSeqLEngLineRect() {
        rectVec_.clear();
        isBlackWdFlag_ = 1;
        left_ = 0;
        top_ = 0;
        wid_ = 0;
        hei_ = 0;
    }
};


struct SSeqLEngCharInfor{
    int left_;
    int top_;
    int wid_;
    int hei_;
    char charStr_[4];

    SSeqLEngCharInfor() {
        charStr_[0] = '\0';
        left_ = 0;
        top_ = 0;
        wid_ = 0;
        hei_ = 0;
    }
};

struct SSeqLEngWordInfor{
    int left_;
    int top_;
    int wid_;
    int hei_;
    int leftSpace_;
    int rightSpace_;
    bool sigWordFlag_;
    int compNum_;
    int highLowType_;
    float dectScore_;
    float mulRegScore_;
    std::string wordStr_;
    std::string maxStr_;
    bool toBeAdjustCandi_;
    bool isPunc_;
    bool toBeDelete_;

    int iNormWidth_; // xie shufu add it stores the normalized width for recognition
    int iNormHeight_;
    int timeStart_; // time start
    int timeEnd_; // time end
    // true: the character with the max recognition confidence; false: not the character with max confidence
    bool toBeMaxConf_;
    int duplicateNum_; //the  number of duplicate characters

    // true: the left of this word should add space, e.g., " hello"
    // false: the left of this word should not add space, e.g., "hello"
    bool left_add_space_;

    // xie shufu add
    // if true: the path including this word will be weighted and the score will increase
    bool bRegFlag;
    // language category type 0: Chinese; 1: 0-9; 2: a-z A-Z; 3:punctuation
    int iLcType;
    // the depth of the valid Chinese words(iLcType = 0)
    int iWordDepth;
    std::vector<SSeqLEngCharInfor> charVec_;
    std::vector<std::string> adStrVec_;
    std::vector<bool> strongAdStrVec_;
    std::vector<float> centPosVec_;
    std::vector<int> startPosVec_;
    std::vector<int> endPosVec_;
    std::vector<float> proMat_;
    std::vector<int> maxVec_;
    std::vector<int> pathVec_;
    std::vector<int> cand_label_vec;
    // candidate word vector
    std::vector<SSeqLEngWordInfor> cand_word_vec;
    double regLogProb_;
    // the softmax output probability [0~1]
    double reg_prob;
    int quote_pos_;
    std::vector<int> path_pos_;
    bool need_adjust_;
    // the score for the att acc and main line buff
    float acc_score_;
    bool main_line_;
    // word end log for split word
    float word_flag_log_;
    //attention weights to get position
    std::vector<float> att_weights_;
    int encoder_fea_num_;
    float encoder_fea_length_;
    std::vector<int> att_pos_st_id_set_; //by max att weight
    std::vector<int> att_pos_en_id_set_;

    SSeqLEngWordInfor() {
        left_ = 0;
        top_ = 0;
        wid_ = 0;
        hei_ = 0;
        leftSpace_ = 0;
        rightSpace_ = 0;
        sigWordFlag_ = 0;
        compNum_ = 0;
        highLowType_ = 0;
        dectScore_ = 0;
        mulRegScore_ = 0;
        wordStr_ = "";
        maxStr_ = "";
        toBeAdjustCandi_ = true;
        isPunc_ = true;
        toBeDelete_ = true;

        iNormWidth_ = 0;
        iNormHeight_ = 0;
        timeStart_ = 0;
        timeEnd_ = 0;
        toBeMaxConf_ = true;
        duplicateNum_ = 0;

        left_add_space_ = true;

        bRegFlag = false;
        iLcType = 0;

        iWordDepth = 0;
        charVec_.clear();
        adStrVec_.clear();
        strongAdStrVec_.clear();
        centPosVec_.clear();
        startPosVec_.clear();
        endPosVec_.clear();
        proMat_.clear();
        maxVec_.clear();
        pathVec_.clear();
        cand_label_vec.clear();

        cand_word_vec.clear();
        regLogProb_ = 0;
        reg_prob = 0;
        quote_pos_ = 0;
        path_pos_.clear();
        need_adjust_ = false;
        acc_score_ = 0.0;
        main_line_ = false;
        word_flag_log_ = 0.0;

        att_weights_.clear();
        encoder_fea_num_ = 0;
        encoder_fea_length_ = 0.0;
        att_pos_st_id_set_.clear();
        att_pos_en_id_set_.clear();
    }
    SSeqLEngWordInfor& operator=(const SSeqLEngWordInfor &word) {

        this->left_ = word.left_;
        this->top_ = word.top_;
        this->wid_ = word.wid_;
        this->hei_ = word.hei_;
        this->leftSpace_ = word.leftSpace_;
        this->rightSpace_ = word.rightSpace_;
        this->sigWordFlag_ = word.sigWordFlag_;
        this->compNum_ = word.compNum_;
        this->highLowType_ = word.highLowType_;
        this->dectScore_ = word.dectScore_;
        this->mulRegScore_ = word.mulRegScore_;
        this->wordStr_ = word.wordStr_;
        this->maxStr_ = word.maxStr_;
        this->toBeAdjustCandi_ = word.toBeAdjustCandi_;
        this->isPunc_ = word.isPunc_;
        this->toBeDelete_ = word.toBeDelete_;

        this->iNormWidth_ = word.iNormWidth_;
        this->iNormHeight_ = word.iNormHeight_;
        this->timeStart_ = word.timeStart_;
        this->timeEnd_ = word.timeEnd_;
        this->toBeMaxConf_ = word.toBeMaxConf_;
        this->duplicateNum_ = word.duplicateNum_;

        this->left_add_space_ = word.left_add_space_;

        this->bRegFlag = word.bRegFlag;
        this->iLcType = word.iLcType;

        this->iWordDepth = word.iWordDepth;
        this->charVec_.clear();
        for (int i = 0; i < word.charVec_.size(); i++) {
            this->charVec_.push_back(word.charVec_[i]);
        }
        this->adStrVec_ = word.adStrVec_;
        this->strongAdStrVec_ = word.strongAdStrVec_;
        this->centPosVec_ = word.centPosVec_;
        this->startPosVec_ = word.startPosVec_;
        this->endPosVec_ = word.endPosVec_;
        this->proMat_ = word.proMat_;
        this->maxVec_ = word.maxVec_;
        this->pathVec_ = word.pathVec_;
        this->cand_label_vec = word.cand_label_vec;

        this->cand_word_vec.clear();
        for (int i = 0; i < word.cand_word_vec.size(); i++) {
            this->cand_word_vec.push_back(word.cand_word_vec[i]);
        }
        this->regLogProb_ = word.regLogProb_;
        this->reg_prob = word.reg_prob;
        this->quote_pos_ = word.quote_pos_;
        this->acc_score_ = word.acc_score_;
        this->main_line_ = word.main_line_;
        this->word_flag_log_ = word.word_flag_log_;

        this->att_weights_ = word.att_weights_;
        this->encoder_fea_num_ = word.encoder_fea_num_;
        this->encoder_fea_length_ = word.encoder_fea_length_;
        this->att_pos_st_id_set_ = word.att_pos_st_id_set_;
        this->att_pos_en_id_set_ = word.att_pos_en_id_set_;
        return (*this);
    }
};

struct SSeqLEngLineInfor{
    std::vector<SSeqLEngWordInfor> wordVec_;
    int left_;
    int top_;
    int wid_;
    int hei_;
    int imgWidth_;
    int imgHeight_;
    std::string lineStr_;
    std::vector<float> poly_location;
    std::vector<float> finegrained_poly_location;
    int isBlackWdFlag_;

    double dAvgCharSpace; // the space between two neighbor characters
    double dCombineCharSpace; // the space between first character and third character

    SSeqLEngLineInfor() {
        wordVec_.clear();
        left_ = 0;
        top_ = 0;
        wid_ = 0;
        hei_ = 0;
        imgWidth_ = 0;
        imgHeight_ = 0;
        lineStr_ = "";
        poly_location.clear();
        finegrained_poly_location.clear();
        isBlackWdFlag_ = 1;
        dAvgCharSpace = 0;
        dCombineCharSpace = 0;
    }

    SSeqLEngLineInfor& operator=(const SSeqLEngLineInfor &line) {
        this->wordVec_.clear();
        for (int i = 0; i < line.wordVec_.size(); i++) {
            this->wordVec_.push_back(line.wordVec_[i]);
        }
        this->poly_location = line.poly_location;
        this->finegrained_poly_location = line.finegrained_poly_location;

        this->left_ = line.left_;
        this->top_ = line.top_;
        this->wid_ = line.wid_;
        this->hei_ = line.hei_;
        this->imgWidth_ = line.imgWidth_;
        this->imgHeight_ = line.imgHeight_;
        this->lineStr_ = line.lineStr_;
        this->isBlackWdFlag_ = line.isBlackWdFlag_;
        this->dAvgCharSpace = line.dAvgCharSpace;
        this->dCombineCharSpace = line.dCombineCharSpace;

        return (*this);
    }
};


struct SSeqLEngSpaceConf{
    float highSpaceTh1_;
    float highSpaceTh2_;
    float lowSpaceTh1_;
    float lowSpaceTh2_;
    float highSpaceTh_;
    float lowSpaceTh_;
    float geoWordSpaceTh_;
    float meanBoxWidth_;
    float meanBoxSpace_;
    float avgCharWidth_;
};

////////////////////////////////////////
//structure for batch predict
struct st_batch_img {
    IplImage *img; // row image
    int bg_flag;   // background flag

    st_batch_img() {
        img = NULL;
        bg_flag = 1;
    }
};
///////////////////////////////////////
#endif  //__SEQLDECODEDEF_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
