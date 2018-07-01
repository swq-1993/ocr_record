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

#include "SeqLEngWordSegProcessor.h"


int main(int argc, char * argv[]){
  if(argc != 3){
     std::cout << "./test-main segInforDir saveWordDir " << std::endl;
     return -1;
  }

  std::string segInforDir = argv[1];
  std::string saveInforDir = argv[2];

  CSeqLEngWordSegProcessor segProcessor;
  
  DIR *dir=NULL;
  struct dirent* ptr=NULL;  
  if( (dir = opendir( segInforDir.c_str() )) == NULL ){
        std::cerr << "Open dir failed. Dir: " << segInforDir << std::endl;
        return -1;
  }

  while( ptr = readdir( dir ) ){
     if( ptr->d_name[0] == '.'){
            continue;
     }


     std::string readName = segInforDir;
     readName += "/";
     readName += ptr->d_name;

     if( segProcessor.loadFileWordSegRects(readName)!=0){
        std::cerr <<"Error load file " << readName << std::endl;
     }
     
     segProcessor.wordSegProcessGroupNearby(3);

     std::string saveName = saveInforDir;
     saveName += "/";
     saveName += ptr->d_name;
     //segProcessor.saveFileWordWins( saveName ); 
        segProcessor.save_file_word_wins(saveName); 
  }
  return 0;
}



















/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
