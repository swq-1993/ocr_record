/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file test-main.cpp
 * @author hanjunyu(com@baidu.com)
 * @date 2014/06/17 11:09:54
 * @brief 
 *  
 **/

#include <dirent.h>
#include <iostream>
#include <cv.h>
#include <highgui.h>
#include "SeqLLoadRegInfor.h"
#include "SeqLSaveRegInfor.h"
#include "SeqLEngLineDecodeHead.h"
#include "SeqLEngLineBMSSWordDecoder.h"
#include "SeqLEngWordSegProcessor.h"
#include "SeqLEngWordRecog.h"
#include "esc_searchapi.h"
#include "DictCalFactory.h"

extern char G_thisImageName[256];
#ifdef COMPUT_TIME
    clock_t onlyDecodeClock, onlyRecogClock,onlySegmentClock, onlyRecogPredictClock;
#endif


int main(int argc, char *argv[] ){
  if(argc != 5){
     std::cout << "./test-main imgDir dectDir beforeInforDir postInforDir" << std::endl;
     return -1;
  }
  
  std::string imageDir = argv[1];
  std::string segInforDir   = argv[2];
  std::string saveInforDir  = argv[3];
  std::string postInforDir  = argv[4];
  
  char espModelConfName[256];
  char interLmTrieName[256];
  char interLmTargetName[256];

  sprintf( espModelConfName, "data/assist_data/sc.conf" );
  sprintf( interLmTrieName, "data/assist_data/lm_trie_word" );
  sprintf( interLmTargetName, "data/assist_data/target_word.vocab" );
  
  esp::esp_engsearch * g_pEngSearchRecog =  new esp::esp_engsearch;
  if( g_pEngSearchRecog->esp_init( espModelConfName ) < 0 )
  {
     std::cerr << "Failed to load the spell check module!" << std::endl;
     return 0;
  }
  
  const LangModel::AbstractScoreCal & interWordLm = \
            LangModel::GetModel( LangModel::CalType_CompactTrie, interLmTrieName, 3, \
            true, true, SEQLENGINTRAGRAMOOPRO,  interLmTargetName );

   
  CSeqLEngWordSegProcessor segProcessor;

//  CSeqLEngWordReg recogProcessor("./data/assist_data/seqL.model.bin", \
         "./data/assist_data/table_seqL_94",112.5, 48);
  CSeqLEngWordRegModel recogModel("./data/assist_data/model_seqL_94.bin", \
          "./data/assist_data/table_seqL_94",112.5, 48);
  CSeqLEngWordReg recogProcessor(recogModel);



#ifdef COMPUT_TIME
    clock_t ct1, ct2;
    ct1 = clock();
    onlyDecodeClock = 0;
    onlyRecogClock = 0;
    onlyRecogPredictClock = 0;
    onlySegmentClock = 0;
#endif
    
    int imageNum =0;
    int totalLineNum = 0;
  DIR *dir=NULL;
  struct dirent* ptr=NULL;  
  if( (dir = opendir( imageDir.c_str() )) == NULL ){
        std::cerr << "Open dir failed. Dir: " << segInforDir << std::endl;
        return -1;
  }

  while( ptr = readdir( dir ) ){
     if( ptr->d_name[0] == '.'){
            continue;
     }
     std::cout << "Image: " << ptr->d_name << std::endl;
     strcpy(G_thisImageName, ptr->d_name);

     std::string readName = segInforDir;
     readName += "/";
     readName += ptr->d_name;
     readName += ".line.txt";
     std::cout << "Process " << readName << std::endl;
     if(segProcessor.loadFileDetectLineRects(readName)!=0 ){
//     if( segProcessor.loadFileWordSegRects(readName)!=0){
        std::cerr <<"Error load file " << readName << std::endl;
        continue;
     }
     
     
     std::string imageName = imageDir;
     imageName += "/";
     imageName += ptr->d_name;
     IplImage * image = NULL;
     if(( image = cvLoadImage( imageName.c_str(), CV_LOAD_IMAGE_COLOR )) ==0 ){
         std::cerr << "Error load Image " << imageName << std::endl;
         continue;
     }
     imageNum++;

#ifdef COMPUT_TIME
    clock_t loct1, loct2;
    loct1 = clock();
#endif
    
     std::vector<SSeqLEngLineInfor> lineInforVec;
     std::vector<SSeqLEngLineRect> lineRectVec;
     if(0){
        lineRectVec.resize(1);
        lineRectVec[0].isBlackWdFlag_ = 1;
        lineRectVec[0].rectVec_.resize(1);
        lineRectVec[0].rectVec_[0].left_ = 0;
        lineRectVec[0].rectVec_[0].wid_ = image->width;
        lineRectVec[0].rectVec_[0].top_ = 0;
        lineRectVec[0].rectVec_[0].hei_ = image->height;
        lineInforVec.resize(lineRectVec.size());
     }
     else{
         segProcessor.processOneImage(image, segProcessor.getDetectLineVec());

        //segProcessor.wordSegProcessGroupNearby( 4 );
        segProcessor.wordSegProcessGroupNearby( 4 );
        lineRectVec = segProcessor.getSegLineRectVec();
        lineInforVec.resize(lineRectVec.size());
    }

#ifdef COMPUT_TIME
    loct2 = clock();
    onlySegmentClock += loct2 - loct1;
    loct1 = clock();
#endif
     for(unsigned int i=0; i<lineRectVec.size(); i++){
         totalLineNum ++;
         SSeqLEngLineInfor & tempLine = lineInforVec[i]; 
         recogProcessor.recognizeWordLineRect(image, lineRectVec[i], tempLine);
     }

#ifdef COMPUT_TIME
    loct2 = clock();
    onlyRecogClock += loct2 - loct1;
    loct1 = clock();
#endif
     std::string saveName = saveInforDir;
     saveName += "/";
     saveName += ptr->d_name;
     saveName += ".txt";
     
     std::cout << "save Input Lines" << std::endl;
     CSeqLEngSaveImageForWordEst::saveResultImageLineVec(saveName, lineInforVec);
    
     std::vector<SSeqLEngLineInfor> outLineVec;
     for(unsigned int i=0; i<lineInforVec.size(); i++ ){
       std::cout << "Decode Line " << i << std::endl; 
       CSeqLEngLineDecoder * lineDecoder = new CSeqLEngLineBMSSWordDecoder; 
       lineDecoder->setWordLmModel( g_pEngSearchRecog, (LangModel:: AbstractScoreCal*) &interWordLm);
       lineDecoder->setSegSpaceConf( segProcessor.getSegSpaceConf(i));
       SSeqLEngLineInfor outLine = lineDecoder->decodeLine( &recogProcessor, lineInforVec[i]);
       delete lineDecoder;
       outLineVec.push_back(outLine);
     }
     
#ifdef COMPUT_TIME
    loct2 = clock();
    onlyDecodeClock += loct2 - loct1;
    loct1 = clock();
#endif
     std::string savePostName = postInforDir;
     savePostName += "/";
     savePostName += ptr->d_name;
     savePostName += ".txt";
     CSeqLEngSaveImageForWordEst::saveResultImageLineVec(savePostName, outLineVec);

     cvReleaseImage(&image);
  }

#ifdef COMPUT_TIME
    ct2 = clock();
    printf( "CPU time %.3fs for New Decoder of %d imgs,%d lines,noly decoder time %.3fs, only recog time %.3fs,only segment time is %.3f , only predict time is %.3f \n", \
            (float)(ct2-ct1)/CLOCKS_PER_SEC,imageNum ,totalLineNum ,(float)onlyDecodeClock/CLOCKS_PER_SEC,\
            (float)onlyRecogClock/CLOCKS_PER_SEC, (float)onlySegmentClock/CLOCKS_PER_SEC,
            (float)onlyRecogPredictClock/CLOCKS_PER_SEC);
#endif


  
  closedir(dir); 
  LangModel::RemoveModel();
  if( g_pEngSearchRecog )
  {
      delete g_pEngSearchRecog;
      g_pEngSearchRecog = NULL;
  }
  return 0;

}



/*

int main(int argc, char * argv[]){
  if(argc != 4){
     std::cout << "./test-main regInforDir saveInforDir postInforDir" << std::endl;
     return -1;
  }

  std::string regInforDir = argv[1];
  std::string saveInforDir = argv[2];
  std::string postInforDir = argv[3];

  char espModelConfName[256];
  char interLmTrieName[256];
  char interLmTargetName[256];

  sprintf( espModelConfName, "data/assist_data/sc.conf" );
  sprintf( interLmTrieName, "data/assist_data/lm_trie_word" );
  sprintf( interLmTargetName, "data/assist_data/target_word.vocab" );
  
  esp::esp_engsearch * g_pEngSearchRecog =  new esp::esp_engsearch;
  if( g_pEngSearchRecog->esp_init( espModelConfName ) < 0 )
  {
     std::cerr << "Failed to load the spell check module!" << std::endl;
     return 0;
  }
  
  const LangModel::AbstractScoreCal & interWordLm = \
            LangModel::GetModel( LangModel::CalType_CompactTrie, interLmTrieName, 3, \
            true, true, SEQLENGINTRAGRAMOOPRO,  interLmTargetName );

  //     CSeqLEngLineDecoder * lineDecoder = new CSeqLEngLineDecoder; 
   //CSeqLEngLineDecoder * lineDecoder = new CSeqLEngLineADPatchDecoder; 

		const char* sent ="enough";
        double lmScoreN1 = 0;
        LangModel::CalIterWrapper r_n1(LangModel::CalType_CompactTrie) ;
        r_n1 = interWordLm.BeginCal( lmScoreN1 );
        r_n1 = interWordLm.NextCal( r_n1, &( sent), size_t(1), lmScoreN1 );
	    std::cout << sent <<" " << lmScoreN1 << std::endl;
        
		const char* sent2 ="eno ug h";
        lmScoreN1 = 0;
        r_n1 = interWordLm.BeginCal( lmScoreN1 );
        r_n1 = interWordLm.NextCal( r_n1, &( sent2 ), size_t(1), lmScoreN1 );
	    std::cout << sent2 <<" " << lmScoreN1 << std::endl;
       
        const char* sent3="eno";
        const char* sent4="ug";
        const char* sent5="h";
        lmScoreN1 = 0;
        r_n1 = interWordLm.BeginCal( lmScoreN1 );
        r_n1 = interWordLm.NextCal( r_n1, &( sent3 ), size_t(1), lmScoreN1 );
	    std::cout << sent3 <<" " << lmScoreN1 << std::endl;
        r_n1 = interWordLm.NextCal( r_n1, &( sent4 ), size_t(1), lmScoreN1 );
	    std::cout << sent4 <<" " << lmScoreN1 << std::endl;
        r_n1 = interWordLm.NextCal( r_n1, &( sent5 ), size_t(1), lmScoreN1 );
	    std::cout << sent5 <<" " << lmScoreN1 << std::endl;





  //CSeqLEngWordRegLoader reader;
  CSeqLEngWordCutRegLoader reader;
  
  DIR *dir=NULL;
  struct dirent* ptr=NULL;  
  if( (dir = opendir( regInforDir.c_str() )) == NULL ){
        std::cerr << "Open dir failed. Dir: " << regInforDir << std::endl;
        return -1;
  }

  while( ptr = readdir( dir ) ){
     if( ptr->d_name[0] == '.'){
            continue;
     }


     std::string readName = regInforDir;
     readName += "/";
     readName += ptr->d_name;

     if(reader.loadFile(readName)!=0){
        std::cerr <<"Error load file " << readName << std::endl;
     }
  
     std::cout << reader.getFileName() << " Line Num: " << reader.getLineNum() << std::endl;
     std::vector<SSeqLEngLineInfor> lineVec;
     for(int i=0; i<reader.getLineNum(); i++){
        printASeqLLine( reader.getLineInfor(i));
        lineVec.push_back(reader.getLineInfor(i));
     }

     std::string saveName = saveInforDir;
     saveName += "/";
     saveName += ptr->d_name;
     
     std::cout << "save Input Lines" << std::endl;
     CSeqLEngSaveImageForWordEst::saveResultImageLineVec(saveName, lineVec);
    
     std::vector<SSeqLEngLineInfor> outLineVec;
     for(unsigned int i=0; i<lineVec.size(); i++ ){
       std::cout << "Decode Line " << i << std::endl; 
       
       CSeqLEngLineDecoder * lineDecoder = new CSeqLEngLineBMSSWordDecoder; 
       lineDecoder->setWordLmModel( g_pEngSearchRecog, (LangModel:: AbstractScoreCal*) &interWordLm);
       
       SSeqLEngLineInfor outLine = lineDecoder->decodeLine(lineVec[i]);
       
       delete lineDecoder;

       outLineVec.push_back(outLine);
     }
     
     std::string savePostName = postInforDir;
     savePostName += "/";
     savePostName += ptr->d_name;
     CSeqLEngSaveImageForWordEst::saveResultImageLineVec(savePostName, outLineVec);
     
  }

  closedir(dir); 
  LangModel::RemoveModel();
  if( g_pEngSearchRecog )
  {
      delete g_pEngSearchRecog;
      g_pEngSearchRecog = NULL;
  }
  return 0;
}

*/

















/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
