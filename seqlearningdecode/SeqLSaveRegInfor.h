/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file SeqLSaveRegInfor.h
 * @author hanjunyu(com@baidu.com)
 * @date 2014/06/16 21:49:40
 * @brief 
 *  
 **/




#ifndef  __SEQLSAVEREGINFOR_H_
#define  __SEQLSAVEREGINFOR_H_

#include "SeqLDecodeDef.h"
#include "SeqLEngWordRecog.h"

class CSeqLEngSaveImageForWordEst{
  public:
   static void saveResultOneLine(std::string saveName, SSeqLEngLineInfor& line, std::string format){
       FILE *fpRetFile = NULL;
       fpRetFile = fopen(saveName.c_str(), format.c_str());
       if (fpRetFile == NULL)
       {
           std::cerr << "Can not open the file " << saveName << std::endl;
           return;
       }
	   for( int i=0; i< line.wordVec_.size(); i++ ){
        if(strcmp(line.wordVec_[i].wordStr_.c_str(), "<!>")==0){
           continue;
        }

        int strLen = line.wordVec_[i].wordStr_.size();
        if( strLen >2 && false ){
           if(line.wordVec_[i].wordStr_[strLen-1]=='s'){
                std::string locStr = line.wordVec_[i].wordStr_;
                locStr[strLen-1] = '\0';
		        fprintf( fpRetFile, "%d\t%d\t%d\t%d\t%s\t%f\t\n", line.wordVec_[i].left_, line.wordVec_[i].top_, \ 
                    line.wordVec_[i].wid_ , line.wordVec_[i].hei_, \
                    locStr.c_str(), line.wordVec_[i].regLogProb_ );
		        fprintf( fpRetFile, "%d\t%d\t%d\t%d\t%s\t%f\t\n", line.wordVec_[i].left_, line.wordVec_[i].top_, \ 
                    line.wordVec_[i].wid_ , line.wordVec_[i].hei_, \
                    "s", line.wordVec_[i].regLogProb_ );
           }
        }
		fprintf( fpRetFile, "%d\t%d\t%d\t%d\t%s\t%f\t\n", line.wordVec_[i].left_, line.wordVec_[i].top_, \ 
                line.wordVec_[i].wid_ , line.wordVec_[i].hei_, \
                line.wordVec_[i].wordStr_.c_str(), line.wordVec_[i].regLogProb_ );
       }
	
      fclose(fpRetFile);
      fpRetFile = NULL;
      return;
}
   static void saveResultImageLineVec(std::string saveName, std::vector<SSeqLEngLineInfor> & lineVec ){
      for(int i=0; i<lineVec.size(); i++){
        if(i==0){
           saveResultOneLine(saveName, lineVec[i], "w"); 
        }
        else{
           saveResultOneLine(saveName, lineVec[i], "a"); 
        }
      }  
   }
           
};











#endif  //__SEQLSAVEREGINFOR_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
