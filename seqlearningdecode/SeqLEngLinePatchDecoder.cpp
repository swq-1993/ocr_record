/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file SeqLEngLinePatchDecoder.cpp
 * @author hanjunyu(com@baidu.com)
 * @date 2014/06/18 22:56:48
 * @brief 
 *  
 **/

#include "SeqLEngLinePatchDecoder.h"
#include "EngWordDecode.h"



#include "inheritBdocr.h"
#include "EngCCARegDecode.h"
#include "EngDecodeFeature.h"
#include "TrainPatchDecode.h"
#include "EngDecodeFeature.h"
#include "cdnnWordCut.h"
#include <dirent.h>
LangModel:: AbstractScoreCal* PatchDecodeGlobalInterLmModel=NULL;
LangModel:: AbstractScoreCal* PatchDecodeGlobalCharLmModel=NULL;
CLM * PatchDecodeGlobalIntraLmModel = NULL;
esp_engsearch *G_g_pEngSearchRecog = NULL;
cdnnWordCutModel G_cdnnWordCutModel;
void * G_cdnnModel;
unsigned short * G_cdnnTable;
float * G_cdnnMean;
void * G_cdnnModel_d;
unsigned short * G_cdnnTable_d;
float *G_cdnnMean_d;
#define TOTALCLSNUM 2704
unsigned short g_uEnglishCodeArr[TOTALCLSNUM][2];
int g_nTotalClassNum; 

void printEngCharInfor(SEngCharInfo & engChar){
  std::cout << " ====  " << engChar.codeStr << " Left: "\
      << engChar.left << " Right: " << engChar.right << " Top: " << engChar.top \
      << " Bottom: " << engChar.bottom << std::endl;
}

void printEngWordInfor(SEngWord & engWord )
{
  std::cout << "============  Begin a word ============== " << std::endl;
  std::cout << "Word rect [ l t w h ]:" << engWord.left << "\t" \
      << engWord.top << "\t" << engWord.width << "\t" << engWord.height << "\t" \
      << "Str: " << engWord.wordStr.c_str() << std::endl;
  for(unsigned int i=0; i<engWord.charSet.size(); i++){
    printEngCharInfor(engWord.charSet[i]);
  }
}

void printEngSentInfor(SEngSentence & engSent){
  for(unsigned int i = 0; i<engSent.words.size(); i++){
    printEngWordInfor(engSent.words[i]); 
  }
}

void copySeqLLineInforToSentence( SSeqLEngLineInfor & inputLine, SEngSentence & outSent ){

  outSent.left = inputLine.left_;
  outSent.top  = inputLine.top_;
  outSent.right = inputLine.left_ + inputLine.wid_ - 1;
  outSent.bottom = inputLine.top_ + inputLine.hei_ - 1;
  outSent.width = inputLine.wid_;
  outSent.height = inputLine.hei_;
  outSent.wordNum = inputLine.wordVec_.size();
  outSent.score = 1.0;

  for(unsigned int i=0; i<inputLine.wordVec_.size(); i++){
    SSeqLEngWordInfor & tempSeqLWord = inputLine.wordVec_[i];
    SEngWord tempEngWord;
    tempEngWord.left   = tempSeqLWord.left_;
    tempEngWord.top    = tempSeqLWord.top_;
    tempEngWord.right  = tempSeqLWord.left_ + tempSeqLWord.wid_ - 1;
    tempEngWord.bottom = tempSeqLWord.top_ + tempSeqLWord.hei_ - 1;
    tempEngWord.width  = tempSeqLWord.wid_;
    tempEngWord.height = tempSeqLWord.hei_;
    tempEngWord.lowUpType = 0;
    tempEngWord.candiNum = 0; // it will be reset in EngWordDecode function
    tempEngWord.minTop = tempSeqLWord.top_;
    tempEngWord.avgHei = tempSeqLWord.hei_;
    tempEngWord.isPunc = false;
    tempEngWord.wordStr = tempSeqLWord.wordStr_;
    tempEngWord.checkScore = 1.0;
    tempEngWord.isWordFlag = true;

    unsigned int tempWordLen = tempSeqLWord.wordStr_.length(); 
    for(unsigned int j=0; j<tempWordLen; j++){
      SEngCharInfo tempEngChar;
      tempEngChar.left   = tempSeqLWord.left_ + j*tempSeqLWord.wid_/tempWordLen;
      tempEngChar.top    = tempSeqLWord.top_;
      int tempCharWidth  = tempSeqLWord.wid_ / tempWordLen;
      int tempCharHeight = tempSeqLWord.hei_;
      tempEngChar.right  = tempEngChar.left + tempCharWidth - 1;
      tempEngChar.bottom = tempEngChar.top + tempCharHeight - 1;
      tempEngChar.codeStr[0] = tempSeqLWord.wordStr_.at(j);
      tempEngChar.codeStr[0] = '\0';
      tempEngChar.wCode = 0; // unuseful
      
      tempEngWord.charSet.push_back(tempEngChar);

      SEngDetectWin tempDectWin;
      tempDectWin.left = tempEngChar.left;
      tempDectWin.top = tempEngChar.top;
      tempDectWin.right = tempEngChar.right;
      tempDectWin.bottom = tempEngChar.bottom;
      tempDectWin.rnum = 1;
      tempEngWord.winSet.push_back(tempDectWin);
    }
     outSent.words.push_back(tempEngWord);   
  } 
}

void copySentenceToSeqLLineInfor( SEngSentence & inSent, SSeqLEngLineInfor & outputLine){
  outputLine.left_ = inSent.left;
  outputLine.top_  = inSent.top;
  outputLine.wid_  = inSent.width;
  outputLine.hei_  = inSent.height;
  outputLine.lineStr_ = "";
  outputLine.wordVec_.clear();
  for(unsigned int i=0; i<inSent.words.size(); i++){
    SSeqLEngWordInfor tempWord;
    tempWord.left_ = inSent.words[i].left;
    tempWord.top_ = inSent.words[i].top;
    tempWord.wid_ = inSent.words[i].width;
    tempWord.hei_ = inSent.words[i].height;
    tempWord.wordStr_ = inSent.words[i].wordStr;
    outputLine.lineStr_ += " ";
    outputLine.lineStr_ += tempWord.wordStr_;
    outputLine.wordVec_.push_back(tempWord);
  }
}

SSeqLEngLineInfor CSeqLEngLineADPatchDecoder::decodeLineFunction( SSeqLEngLineInfor & inputLine ){
    std::cout << "Begin the old second word decoder of EngPatchDecoder " << std::endl;

    SEngSentence inSent;

    copySeqLLineInforToSentence( inputLine, inSent );

    SEngSentence outputSent = EngWordDecode( &inSent, espModel_, interWordLm_);
    std::cout<< "***************** inSent *******************" << std::endl;
    printEngSentInfor( inSent);
    std::cout<< "***************** outSent *******************" << std::endl;
    printEngSentInfor( outputSent);
    
    SSeqLEngLineInfor outputLine;
    copySentenceToSeqLLineInfor(outputSent, outputLine);

    return outputLine;
}


















/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
