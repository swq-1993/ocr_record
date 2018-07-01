/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file SeqLEuroLineBMSSWordDecoder.cpp
 * @author hanjunyu(com@baidu.com)
 * @date 2014/12/15 17:45:36
 * @brief 
 *  
 **/

#include "SeqLEuroLineBMSSWordDecoder.h"
using namespace esp;

CSeqLEuroLineBMSSWordDecoder::CSeqLEuroLineBMSSWordDecoder(){
            beamWidth_   = 20;
            lmWeight_    = 0.9;
            lmWeight0_    = 1;
            spWeight_    = 0;
            spWeight2_   = 0.0;
            regWeight_   = 0.4;
            mulRegWeight_ = 100;
            sjWeight_    = 0;
            cdWeight_    = 20.0;
            nodeExtStep_ = 1000;
            spellCheckNum_ = 5;
            shortLineMaxWordNum_ = 1;
            shortLmDiscount_ = 1.0;
}

SSeqLEngLineInfor CSeqLEuroLineBMSSWordDecoder::decodeLineFunction(SSeqLEngLineInfor & inputLine) {
    inputLine_ = inputLine;
    /* step 0: collect the word candis*/
    extractEuroCandisVector();
    /*for debug 
     * SSeqLEngLineInfor outputLine = inputLine_;
     * catCandisVectorToLine(outputLine_); */
    beamSearchDecode();
    SeqLEuroPostPuntDigWordText(outputLine_);
    SeqLEuroHighLowOutput(outputLine_);
    return outputLine_;
}

void CSeqLEuroLineBMSSWordDecoder::extractEuroCandisVector() {
    SSeqLEngLineInfor & inputLine = inputLine_;
    candisVec_.clear();
    candisVec_.resize(inputLine.wordVec_.size());

    vector< pair<string, double> > output;
    pthread_t threadID = pthread_self();

    for (unsigned int i=0; i < inputLine.wordVec_.size(); i++) {
        /*std::cout << " extractEuroCandisVector i " << i << "size "\
         * << inputLine.wordVec_.size() <<  std::endl;*/ // for debug

        SSeqLEngWordInfor tempWord = inputLine.wordVec_[i];
    
        std::vector<int> unicodeVec;
        SeqLUTF8Str2UnicodeVec(tempWord.wordStr_.c_str(), unicodeVec);
        //assert(unicodeVec.size()>0);
        if (unicodeVec.size() <= 0) {
            continue;
        }
        int endCode = unicodeVec[unicodeVec.size()-1];
        char endCodeStr[6];
        SeqLUnicode2UTF8(endCode, endCodeStr);
        bool endPunctFlag = SeqLEuroUnicodeIsPunct(endCode) && unicodeVec.size()>1; 
     
        SSeqLEngWordCandis & tempCandi = candisVec_[i];
     
        if (endPunctFlag) {
            unicodeVec.resize(unicodeVec.size()-1);
            char * tempStr =  SeqLUnicodeVec2UTF8Str(unicodeVec);
            tempWord.wordStr_ = tempStr;
            if (tempStr!=NULL) {
                delete [] tempStr;
                tempStr = NULL;
            }
        }

        SeqLEuroTransformHigh2Low(tempWord.wordStr_);
        if (inputLine_.wordVec_.size()>1) {
            espModel_->esp_get((threadType)threadID, tempWord.wordStr_.c_str(),\
                    output, 2, spellCheckNum_);
            tempCandi.candiNum_ = std::min((unsigned int)(output.size()), spellCheckNum_) + 1;
        }
        else if (inputLine.wordVec_[0].wordStr_.length()>5) {
            espModel_->esp_get((threadType)threadID, tempWord.wordStr_.c_str(), output, 2, 1);
            tempCandi.candiNum_ = std::min((unsigned int)(output.size()), spellCheckNum_) + 1;
            output.clear();
        }
        else {
            tempCandi.candiNum_ =  1;
            output.clear();
        }

        tempCandi.strVec_.resize(tempCandi.candiNum_);
        tempCandi.rmPunctStrVec_.resize(tempCandi.candiNum_);
        tempCandi.regLogProbVec_.resize(tempCandi.candiNum_);
        tempCandi.checkScoreVec_.resize(tempCandi.candiNum_);
        tempCandi.editDistVec_.resize(tempCandi.candiNum_);
        tempCandi.highLowTypeVec_.resize(tempCandi.candiNum_);

        //the quote_pos_vec_ is only useful in English, we add it here because
        //Euro also used the Eng decode func and may cause core without
        //quote_pos_vec_ being set.
        tempCandi.quote_pos_vec_.resize(tempCandi.candiNum_);
        for (int j = 0; j < tempCandi.candiNum_; ++j) {
            tempCandi.quote_pos_vec_[j] = 0;
        }
        if (endPunctFlag) {
            tempCandi.rmPunctStrVec_[0] = tempWord.wordStr_;
            tempCandi.strVec_[0] = tempWord.wordStr_;
            tempCandi.strVec_[0] += endCodeStr;
        }
        else {
            tempCandi.rmPunctStrVec_[0] = tempWord.wordStr_;
            tempCandi.strVec_[0] = tempWord.wordStr_;
        }

        tempCandi.editDistVec_[0] = 0;
        tempCandi.regLogProbVec_[0] = tempWord.regLogProb_;
        tempCandi.checkScoreVec_[0] = 0.001;
        tempCandi.checkScore_ = 0.001;
        if (output.size() > 0) {
            if (tempCandi.rmPunctStrVec_[0] == output[0].first) {
                tempCandi.checkScoreVec_[0] = output[0].second + 0.01;
            }
            else {
                tempCandi.checkScoreVec_[0] = output[0].second;
            }
            if (tempCandi.checkScoreVec_[0] <= output[0].second) {
                tempCandi.checkScoreVec_[0] = output[0].second + 0.00001;
            }
        }
     
        double saveScore0 = tempCandi.checkScoreVec_[0];
        tempCandi.checkScoreVec_[0] = 1;
        if (output.size() > 0) {
            if (tempCandi.rmPunctStrVec_[0] == output[0].first) {
                tempCandi.checkScoreVec_[0] += 0.05;
            }
        }
        tempCandi.regLogProbVec_[0] = ((CSeqLEuroWordReg *) recognizer_)->onePrefixPathUpLowerLogProb(\
                tempCandi.strVec_[0], &(tempWord.proMat_[0]),\
                tempWord.proMat_.size(), tempCandi.highLowTypeVec_[0]); 

        for (unsigned int j = 1; j<tempCandi.candiNum_; j++) {
            if (endPunctFlag) {
                tempCandi.rmPunctStrVec_[j] = output[j-1].first;
                tempCandi.strVec_[j] = output[j-1].first;
                tempCandi.strVec_[j] += endCodeStr;
            }
            else {
                tempCandi.rmPunctStrVec_[j] = output[j-1].first;
                tempCandi.strVec_[j] = output[j-1].first;
            }
            tempCandi.regLogProbVec_[j] = ((CSeqLEngWordReg *) recognizer_)->onePrefixPathUpLowerLogProb(\
                    tempCandi.strVec_[j], &(tempWord.proMat_[0]),\
                    tempWord.proMat_.size(), tempCandi.highLowTypeVec_[j]); 

            tempCandi.checkScoreVec_[j] = 1.0 - stringAlignment(tempCandi.strVec_[0], \ 
                    tempCandi.strVec_[j]) * 1.0 / (tempCandi.strVec_[0].size() + 5);
            tempCandi.editDistVec_[j] = stringAlignment(tempCandi.strVec_[0], tempCandi.strVec_[j]);
        }
    }
}

void CSeqLEuroLineBMSSWordDecoder::SeqLEuroHighLowOutput(SSeqLEngLineInfor & line) {
    int upWordNum = 0;
    for (unsigned int i = 0; i < line.wordVec_.size(); i++) {
        SSeqLEngWordInfor & tempWord = line.wordVec_[i];
        SeqLEuroTransformHigh2Low(tempWord.wordStr_);
        if (tempWord.highLowType_ == 2) {
            upWordNum++ ;
        }
    }
    if (line.wordVec_.size() > 0) {
        if (line.wordVec_[0].highLowType_ == 1) {
            SeqLEuroTransformFirstChar2High(line.wordVec_[0].wordStr_);
        }
    }

    if (upWordNum > 0.7 * line.wordVec_.size()) {
        for (unsigned int i = 0; i < line.wordVec_.size(); i++) {
            SSeqLEngWordInfor & tempWord = line.wordVec_[i];
            SeqLEuroTransformLow2High(tempWord.wordStr_);
        }
    }
}

void CSeqLEuroLineBMSSWordDecoder::SeqLEuroPostPuntDigWordText(SSeqLEngLineInfor & line) {
    std::vector<SSeqLEngWordInfor> wordVec;
    for (unsigned int w = 0; w < line.wordVec_.size(); w++) {
        SSeqLEngWordInfor & inputWord = line.wordVec_[w];
        std::vector<std::string> punctStrVec;
        int startIndex = 0;
        int perFlag = -1;
        std::vector<int> unicodeVec;
        SeqLUTF8Str2UnicodeVec(inputWord.wordStr_.c_str(), unicodeVec);
        std::vector<int> tempUnicodeVec;
        tempUnicodeVec.reserve(unicodeVec.size());
        for (unsigned int i = 0; i < unicodeVec.size(); i++) {
            int thisFlag = -1;
            if (!(SeqLEuroUnicodeIsPunct(unicodeVec[i]))) {
                if (SeqLUnicodeIsDigitChar(unicodeVec[i])) {
                   thisFlag = 1; 
                }
                else {
                    thisFlag = 0;
                }
            }
            if (perFlag!=-1 && thisFlag==perFlag) {
                tempUnicodeVec.push_back(unicodeVec[i]);
                continue;
            }

            if (perFlag==1 && thisFlag==0) {   // 16mb.
                int afterEngNum = 0;
                for (unsigned j= i + 1; j < unicodeVec.size(); j++) {
                    if (SeqLUnicodeIsEngChar(unicodeVec[j])) {
                        afterEngNum++;
                    }
                    else {
                        break;
                    }
                }
                if (afterEngNum<=2) {
                    perFlag = 0;
                    tempUnicodeVec.push_back(unicodeVec[i]);
                    continue;
                }
            }

            if (tempUnicodeVec.size()==0) {
                perFlag = thisFlag;
                tempUnicodeVec.push_back(unicodeVec[i]);
                continue;
            }
        
            if (i!=0 && i!=(unicodeVec.size()-1)) {  // a-b
                if (unicodeVec[i] == '-'  \ 
                    && SeqLUnicodeIsEngChar(unicodeVec[i-1]) \
                    && SeqLUnicodeIsEngChar(unicodeVec[i+1])) {
                    tempUnicodeVec.push_back(unicodeVec[i]);
                    continue;
                }
            }
            
            if (i!=0 && i!=(unicodeVec.size()-1)) {  // 1.1
                if (unicodeVec[i] == '.'  \ 
                    && SeqLUnicodeIsDigitChar( unicodeVec[i-1]) \
                    && SeqLUnicodeIsDigitChar( unicodeVec[i+1])) {
                    tempUnicodeVec.push_back(unicodeVec[i]);
                    continue;
                }
            }
            
            if (i!=0) { // -1
                if (unicodeVec[i-1] == '-'  \  
                    && SeqLUnicodeIsDigitChar(unicodeVec[i])) {
                    perFlag = thisFlag;
                    tempUnicodeVec.push_back(unicodeVec[i]);
                    continue;
                }
            }

            perFlag = thisFlag;
            if (tempUnicodeVec.size()>0) {
                char* tempStr =  SeqLUnicodeVec2UTF8Str(tempUnicodeVec);
                std::string tempStr2= tempStr;
                delete [] tempStr;
                punctStrVec.push_back(tempStr2);
                tempUnicodeVec.clear();
            }
            tempUnicodeVec.push_back(unicodeVec[i]);
        }

        if (tempUnicodeVec.size()>0) {
            char* tempStr =  SeqLUnicodeVec2UTF8Str(tempUnicodeVec);
            std::string tempStr2= tempStr;
            delete [] tempStr;
            punctStrVec.push_back(tempStr2);
            tempUnicodeVec.clear();
        }

        int startCharIndex = 0; 
        for (unsigned int i=0; i<punctStrVec.size(); i++) {
            SSeqLEngWordInfor newWord = inputWord;
            newWord.wordStr_ = punctStrVec[i];
            newWord.left_ = inputWord.left_ + \
                            startCharIndex*inputWord.wid_/(inputWord.wordStr_.length()+punctStrVec.size()-1);
            newWord.wid_ = newWord.wordStr_.length()*inputWord.wid_/(inputWord.wordStr_.length() +\
                    punctStrVec.size()-1);
            wordVec.push_back(newWord);
            startCharIndex += (newWord.wordStr_.length()+1);
        }
    }
    line.wordVec_ = wordVec;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
