/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file SeqLLoadRegInfor.h
 * @author hanjunyu(com@baidu.com)
 * @date 2014/06/16 21:23:50
 * @brief 
 *  
 **/




#ifndef  __SEQLLOADREGINFOR_H_
#define  __SEQLLOADREGINFOR_H_

#include "SeqLDecodeDef.h"


class CSeqLEngWordRegLoader{
    
    private:

       bool sameLineWordCouple(const SSeqLEngWordInfor & w1, const SSeqLEngWordInfor &w2){
         int w2Cy= w2.top_ + 0.5*w2.hei_;
         if( w2Cy>w1.top_ && (w2Cy < w1.top_+w1.hei_)){
           return true;
         }
         else{
           return false;
         }
       }
       void ExtractInLines();


       std::string fileName_;
       std::vector<SSeqLEngWordInfor> inWordVec_;
       std::vector<SSeqLEngLineInfor> inLineVec_;

    public:
       int loadFile(std::string fileName);
       std::string getFileName(){
         return fileName_;
       }
       int getLineNum(){
         return inLineVec_.size();
       }
       SSeqLEngLineInfor & getLineInfor(int index){
         assert(index < inLineVec_.size());
         return inLineVec_[index];
       }
    
};

 void printASeqLWord( SSeqLEngWordInfor & tempWord){
 
    std::cout << tempWord.left_ << " " << tempWord.top_ << " " \
               << tempWord.wid_ << " " << tempWord.hei_ << " " \
               << tempWord.wordStr_ << std::endl;
 }

 void printASeqLLine( SSeqLEngLineInfor & tempLine){
    std::cout << tempLine.left_ << " " << tempLine.top_ << " " \
               << tempLine.wid_ << " " << tempLine.hei_ << " " \
               << tempLine.lineStr_ << std::endl;
 }

bool leftSeqLWordFirst(const SSeqLEngWordInfor & w1,const SSeqLEngWordInfor& w2){
          return   w1.left_ < w2.left_;   
} 
void CSeqLEngWordRegLoader::ExtractInLines(){

    std::vector<SSeqLEngWordInfor> inWordVecCopy = inWordVec_;
    sort(inWordVecCopy.begin(), inWordVecCopy.end(), leftSeqLWordFirst);
    std::cout << "=========SORTED WORD OUTPUT=============" << std::endl;
    std::vector<SSeqLEngWordInfor>::iterator iter_loc = inWordVecCopy.begin();
    for(iter_loc; iter_loc!=inWordVecCopy.end(); iter_loc++){
      printASeqLWord(*iter_loc);
    }
    
    while(true){
        if(inWordVecCopy.size()<1){
           break;
        }
        
        SSeqLEngLineInfor tempLine;

        std::vector<SSeqLEngWordInfor>::iterator iter = inWordVecCopy.begin();
        tempLine.wordVec_.push_back(*iter);
        tempLine.left_ = iter->left_;
        tempLine.top_ = iter->top_;
        tempLine.wid_ = iter->wid_;
        tempLine.hei_ = iter->hei_;
        tempLine.lineStr_ = iter->wordStr_;

        iter=inWordVecCopy.erase(iter);
        while(true){
          if(iter==inWordVecCopy.end()){
            break;
          }
          if( sameLineWordCouple(tempLine.wordVec_.back(), *iter)){
            tempLine.wordVec_.push_back(*iter);
            int tempRight = std::max(tempLine.left_+tempLine.wid_, iter->left_+iter->wid_);
            int tempBottom = std::max(tempLine.top_ + tempLine.hei_, iter->top_+iter->hei_);
            tempLine.left_ = std::min(tempLine.left_, iter->left_);
            tempLine.top_ = std::min(tempLine.top_, iter->top_);
            tempLine.wid_ = tempRight - tempLine.left_;
            tempLine.hei_ = tempBottom - tempLine.top_;
            tempLine.lineStr_ += " ";
            tempLine.lineStr_ += iter->wordStr_;
            iter= inWordVecCopy.erase(iter);
          }
          else
          {
            iter++;
          }
        }
        inLineVec_.push_back(tempLine);
    }

    return;
}

int CSeqLEngWordRegLoader::loadFile(std::string fileName){
         
         FILE * fd = fopen(fileName.c_str(), "r");
         if(fd==NULL){
          return -1;
         }
         fileName_ = fileName;
         inWordVec_.clear();
         inLineVec_.clear();
         
         while(true){
           int left, top, wid, hei;
           char str[100];
           int num = fscanf(fd,"%d\t%d\t%d\t%d\t%s\t", &left, \
                   &top, &wid, &hei, str);
           if(num < 1){
               break;
           }
           
           SSeqLEngWordInfor tempWord;
           tempWord.left_ = left;
           tempWord.top_ = top;
           tempWord.wid_ = wid;
           tempWord.hei_ = hei;
           tempWord.wordStr_ = str;
           printASeqLWord(tempWord);
           inWordVec_.push_back(tempWord);

         }
         fclose(fd);

         ExtractInLines();
         return 0;

}




class CSeqLEngWordCutRegLoader{
    
    private:

       std::string fileName_;
       std::vector<SSeqLEngLineInfor> inLineVec_;

    public:
       int loadFile(std::string fileName);
       std::string getFileName(){
         return fileName_;
       }
       int getLineNum(){
         return inLineVec_.size();
       }
       SSeqLEngLineInfor & getLineInfor(int index){
         assert(index < inLineVec_.size());
         return inLineVec_[index];
       }
    
};


int CSeqLEngWordCutRegLoader::loadFile(std::string fileName){
         
         FILE * fd = fopen(fileName.c_str(), "r");
         if(fd==NULL){
          return -1;
         }
         fileName_ = fileName;
         inLineVec_.clear();
         
         while(true){
           int lineIndex, left, top, wid, hei;
           char str[100];
           int num = fscanf(fd,"%d\t%d\t%d\t%d\t%d\t%s\t",&lineIndex, &left, \
                   &top, &wid, &hei, str);
           if(num < 1){
               break;
           }
           if(inLineVec_.size()<= lineIndex){
             inLineVec_.resize(lineIndex+1);
           } 
           SSeqLEngWordInfor tempWord;
           tempWord.left_ = left;
           tempWord.top_ = top;
           tempWord.wid_ = wid;
           tempWord.hei_ = hei;
           tempWord.wordStr_ = str;
           printASeqLWord(tempWord);
           inLineVec_[lineIndex].wordVec_.push_back(tempWord);
         }
         fclose(fd);
        
         for( unsigned int i=0; i<inLineVec_.size(); i++){
            sort(inLineVec_[i].wordVec_.begin(), inLineVec_[i].wordVec_.end(), leftSeqLWordFirst);
            
            SSeqLEngLineInfor & tempLine = inLineVec_[i];
            if(tempLine.wordVec_.size() <1 ){
                tempLine.left_ = -1;
                tempLine.top_  = -1;
                tempLine.wid_  = 0;
                tempLine.hei_  = 0;
            }
            else
            {
                tempLine.left_ = tempLine.wordVec_[0].left_;
                tempLine.top_ = tempLine.wordVec_[0].top_;
                tempLine.wid_ = tempLine.wordVec_[0].wid_;
                tempLine.hei_ = tempLine.wordVec_[0].hei_;
                int lineRight = tempLine.left_ + tempLine.wid_;
                int lineBottom = tempLine.top_ + tempLine.hei_;
                for( unsigned int j=0; j<tempLine.wordVec_.size(); j++){
                   SSeqLEngWordInfor & tempWord = tempLine.wordVec_[j];
                   lineRight = std::max(lineRight, tempWord.left_ + tempWord.wid_);
                   lineBottom = std::max(lineBottom, tempWord.top_ + tempWord.hei_);
                   tempLine.left_ = std::min(tempLine.left_, tempWord.left_);
                   tempLine.top_  = std::min(tempLine.top_,  tempWord.top_);
                   tempLine.wid_  = lineRight - tempLine.left_;
                   tempLine.hei_  = lineBottom - tempLine.top_;
                }
            }
         }
        
         return 0;
}



#endif  //__SEQLLOADREGINFOR_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
