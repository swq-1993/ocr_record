/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file SeqLEuroWordRecog.cpp
 * @author hanjunyu(com@baidu.com)
 * @date 2014/12/15 15:26:51
 * @brief 
 *  
 **/

#include "SeqLEuroWordRecog.h"

void SeqLEuroTransformHigh2Low(std::string & str) {
    char * tempStr = SeqLEuroUTF8StrHigh2Low(str.c_str());
    str = tempStr;
    if (tempStr!=NULL) {
        delete [] tempStr;
        tempStr = NULL;
    }
}

void SeqLEuroTransformLow2High(std::string & str) {
    char * tempStr = SeqLEuroUTF8StrLow2High(str.c_str());
    str = tempStr;
    if (tempStr!=NULL) {
        delete [] tempStr;
        tempStr = NULL;
    }
}

void SeqLEuroTransformFirstChar2High(std::string & str){
    using std::vector;
    vector<int> unicodeVec;
    SeqLUTF8Str2UnicodeVec(str.c_str(), unicodeVec);
    if (unicodeVec.size()>0) {
        unicodeVec[0] = SeqLEuroUnicodeLow2High(unicodeVec[0]);
        char * tempStr = SeqLUnicodeVec2UTF8Str(unicodeVec);
        str = tempStr;
        if (tempStr !=NULL) {
            delete [] tempStr;
            tempStr = NULL;
        }
    }
}

float CSeqLEuroWordReg::onePrefixPathUpLowerLogProb(std::string str, float * orgActs,\
        int actLen, int & highLowType) {
    using std::vector;

    SeqLEuroTransformHigh2Low(str);
    float logLow = onePrefixPathLogProbUTF8(str, \
               orgActs, actLen);
    float maxProb = logLow;
    highLowType = 0;

    float logUpHead = safeLog(0);
    vector<int> unicodeVec;
    SeqLUTF8Str2UnicodeVec(str.c_str(), unicodeVec);
    if (unicodeVec.size()>1) {
        unicodeVec[0] = SeqLEuroUnicodeLow2High(unicodeVec[0]);
        char * tempStr = SeqLUnicodeVec2UTF8Str(unicodeVec);
        str = tempStr;
        if (tempStr != NULL) {
            delete [] tempStr;
            tempStr = NULL;
        }
        logUpHead =  onePrefixPathLogProbUTF8(str, \
               orgActs, actLen);
        if (logUpHead > maxProb) {
            maxProb = logUpHead;
            highLowType = 1;
        }
    }
    SeqLEuroTransformLow2High(str);
    float logUp = onePrefixPathLogProbUTF8(str, \
            orgActs, actLen);
    if (logUp > maxProb) {
        maxProb = logUp;
        highLowType = 2;
    }
    return logAdd(logUpHead, logAdd(logLow, logUp));
}
    
int CSeqLEuroWordReg::recognizeWordLineRectTotalPerTime(const IplImage * orgImage,\
        SSeqLEngLineRect & lineRect,\
        SSeqLEngLineInfor & outLineInfor, SSeqLEngSpaceConf & spaceConf) {

    if (lineRect.rectVec_.size()<1) {
        return -1;
    }
    IplImage * image = cvCreateImage(cvSize(orgImage->width, orgImage->height), \
            orgImage->depth, orgImage->nChannels);
    cvCopy(orgImage, image, NULL);

    outLineInfor.wordVec_.clear();
    outLineInfor.left_ = image->width;
    outLineInfor.wid_  = 0;
    outLineInfor.top_  = image->height;
    outLineInfor.hei_  = 0;
    outLineInfor.imgWidth_ = image->width;
    outLineInfor.imgHeight_ = image->height;

    if (orgImage->width < SEQL_MIN_IMAGE_WIDTH\
            || orgImage->height < SEQL_MIN_IMAGE_HEIGHT\
            || orgImage->width > SEQL_MAX_IMAGE_WIDTH\ 
            || orgImage->height > SEQL_MAX_IMAGE_HEIGHT) {
        cvReleaseImage(&image);
        image=NULL;
        return -1;
    }

    // extract the line box 
    int lineLeft = lineRect.rectVec_[0].left_;
    int lineTop = lineRect.rectVec_[0].top_;
    int lineRight = lineRect.rectVec_[0].left_ + lineRect.rectVec_[0].wid_;
    int lineBottom = lineRect.rectVec_[0].top_ + lineRect.rectVec_[0].hei_;
    for (unsigned int i = 1; i < lineRect.rectVec_.size(); i++) {
        SSeqLEngRect & tempRect = lineRect.rectVec_[i];
        lineLeft = std::min(lineLeft, tempRect.left_);
        lineTop = std::min(lineTop, tempRect.top_);
        lineRight = std::max(lineRight, tempRect.left_ + tempRect.wid_);
        lineBottom = std::max(lineBottom, tempRect.top_ + tempRect.hei_);
    }
   
    /*std::cout << "l, :" << lineLeft << " r," <<lineRight << " t," \
     * << lineTop << " b," << lineBottom << std::endl;*/ //for debug
    
    int extBoudary = extendRate_*(lineBottom - lineTop);
    int extBoudary2 = -0.05 * (lineBottom - lineTop);
    SSeqLEngRect tempLineRect = lineRect.rectVec_[0];
    tempLineRect.left_ = std::max(0, lineLeft - extBoudary);
    tempLineRect.top_ = std::max(0, lineTop - int(1.0*extBoudary2));
    tempLineRect.wid_ = std::min(lineRight + extBoudary, image->width) - tempLineRect.left_;
    tempLineRect.hei_ = std::min(lineBottom + int(1.0*extBoudary2), image->height)\
                        - tempLineRect.top_;

    /*std::cout << "Before tempLineWord.rect L " <<  tempLineRect.left_ \
        << "T " <<  tempLineRect.top_ << " W " <<  tempLineRect.wid_ << " H "<<tempLineRect.hei_ \
        <<  std::endl;*/  // for debug

    SSeqLEngWordInfor tempLineWord;
    onceRectSeqLPredict(tempLineRect, lineRect, image, tempLineWord, 0, 1.2, /*true */false);
    
    /*std::cout << "tempLineWord.rect L " <<  tempLineRect.left_ \
        << "T " <<  tempLineRect.top_ << " W " <<  tempLineRect.wid_ << " H "<<tempLineRect.hei_ \
        <<"str:" << tempLineWord.wordStr_ << std::endl;*/ // for debug

    std::vector<int> unicodeVec;
    SeqLUTF8Str2UnicodeVec(tempLineWord.wordStr_.c_str(), unicodeVec);
    if ((unicodeVec.size())>0) {
        spaceConf.avgCharWidth_ = tempLineRect.wid_ / (unicodeVec.size());
    }
    else {
        spaceConf.avgCharWidth_ = tempLineRect.wid_;
    }

    if (lineRect.rectVec_.size() < 5 * tempLineWord.wordStr_.length()) { 
        for (unsigned int i=0; i<lineRect.rectVec_.size(); i++) {
            SSeqLEngRect & tempRect = lineRect.rectVec_[i];
            SSeqLEngWordInfor tempWord;
            //if(1.0*tempRect.hei_ >tempRect.wid_){
            if (true && ( tempRect.hei_ > tempRect.wid_ 
                   || tempRect.compNum_ == 1)) {
                //onceRectSeqLPredict( tempRect,  lineRect,  image, tempWord, 0.19, true);
                bool loadFlag = true;
                onceRectSeqLPredict(tempRect,  lineRect,  image, tempWord, 0.19, 1, false);
                outLineInfor.wordVec_.push_back(tempWord);
                /*if (loadFlag == false) {
                    std::cout << "load false Str:" <<tempWord.wordStr_ << std::endl;
                }*/ //for debug
                //g_predict_num++;
            }
            copyInforFormProjectWordTotal(tempLineWord, tempRect, tempWord, image);
            outLineInfor.wordVec_.push_back(tempWord);
        }
    }

    tempLineRect.left_ = std::max(0, lineLeft - extBoudary);
    tempLineRect.top_ = std::max(0, lineTop - 1*int(0.5*extBoudary2));
    tempLineRect.wid_ = std::min(lineRight + extBoudary, image->width) - tempLineRect.left_;
    tempLineRect.hei_ = std::min(lineBottom + 1 * int(0.5 * extBoudary2), image->height)\
                        - tempLineRect.top_;

    int lineTimeNum = tempLineWord.proMat_.size() / labelsDim_;
    int saveWordSize = outLineInfor.wordVec_.size();
    for (unsigned int i=0; i<lineTimeNum; i++) {
        for (unsigned int j=i; j<lineTimeNum; j++) {
            SSeqLEngRect tempRect = tempLineRect;
            int adExtBoundary = -0.0*tempLineWord.hei_;
            int newExtBoundary = 0.05*tempLineWord.hei_;
            tempRect.left_ = tempLineWord.left_ + i * tempLineWord.wid_ / lineTimeNum + adExtBoundary ;
            tempRect.wid_ = tempLineWord.wid_ * (j - i + 1) / lineTimeNum - 2 * adExtBoundary;
            
            bool haveCorrespondRect = false;
            int correspondIndex = 0;
            int perImageLeft = tempLineWord.left_;        
            int perImageWid  = tempLineWord.wid_;
            int outImageLeft = std::max(perImageLeft, tempRect.left_-newExtBoundary);
            int outImageRight = std::min(tempRect.left_ + tempRect.wid_+newExtBoundary - 1,\
                    perImageLeft + perImageWid - 1);
            int adI = std::max(int((outImageLeft - tempLineWord.left_) * lineTimeNum / tempLineWord.wid_), 0);
            int adJ = std::min(int((outImageRight - \
                        tempLineWord.left_ + 1) * lineTimeNum / tempLineWord.wid_), lineTimeNum - 1);

            if (!(i == 0 || tempLineWord.proMat_[(i - 1) * labelsDim_ + (labelsDim_ - 2)] > 0.1)) {
                continue;
            }

            if (!(j == lineTimeNum - 1  || tempLineWord.proMat_[(j + 1) * labelsDim_ + (labelsDim_ - 2)] > 0.1 )) {
                continue;
            }
            
            bool deleteFlag = false;
            for (unsigned k=i; k<=j; k++) {
                if (tempLineWord.proMat_[(k) * labelsDim_ + (labelsDim_ - 2)] > 0.5) {
                    deleteFlag = true;
                    break;
                }
            }
            if (deleteFlag) {
                continue;
            }
            
            SSeqLEngWordInfor tempWord;
            int wordFlagType =  copyInforFormProjectWordTotal(tempLineWord, tempRect, tempWord, image, i, j);
            if (tempWord.wordStr_.length() < 1) {
                continue;
            }

            tempWord.compNum_ = 0;
            outLineInfor.wordVec_.push_back(tempWord);
        }
    }
    
    if ((outLineInfor.wordVec_.size() - saveWordSize) > 3 * lineTimeNum) {
       outLineInfor.wordVec_.resize(saveWordSize);
    }

    outLineInfor.left_ = lineLeft;
    outLineInfor.top_  = lineTop;
    outLineInfor.wid_ = lineRight - lineLeft;
    outLineInfor.hei_ = lineBottom - lineTop;

    std::vector<SSeqLEngWordInfor>::iterator ita = outLineInfor.wordVec_.begin();
    while (ita != outLineInfor.wordVec_.end())
    {
        if ((ita->wordStr_.length()) == 0 || ita->toBeDelete_)
            ita= outLineInfor.wordVec_.erase(ita);
        else
            ita++;
    }    
    /*  std::cout << "Begin Variznaize Infor ... " << std::endl;*/ // for debug
    saveWordSize = outLineInfor.wordVec_.size();
    for (unsigned int i=0; i< saveWordSize; i++) {
        SSeqLEngWordInfor &tempWord = outLineInfor.wordVec_[i];
        extendVariationWord(outLineInfor, i);
    }
    if (outLineInfor.wordVec_.size()>300) {
       outLineInfor.wordVec_.resize(saveWordSize); 
    }
    sort(outLineInfor.wordVec_.begin(), outLineInfor.wordVec_.end(), leftSeqLWordRectFirst);

    cvReleaseImage(&image);
    image=NULL;
    return 0;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
