/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file SeqLEngPrefixSearch.cpp
 * @author hanjunyu(com@baidu.com)
 * @date 2014/07/08 20:56:54
 * @brief 
 *  
 **/

#include "SeqLEngPrefixSearch.h"





void printPrefixPath( PrefixPath & path){
    std::cout << "prob " << path.pathProb << "\tpath: ";
    for(unsigned int i=0; i<path.path.size(); i++){
      std::cout <<path.path[i] << " ";  
    }
    std::cout << std::endl;
}

PrefixPath onePrefixPathInfor(std::vector<int> & inputPath, float * orgActs, \
         int numClasses, int totalTime, int blank){

    PrefixPath bestPath;
    PrefixPath prePath ;
    int prePathIndex =0;
    
    int maxSearchStep = 1000;   // max search steps
    
    // Initialisation
    float* logActs = new float [numClasses * totalTime];
    for(int i=0; i<numClasses*totalTime; i++){
      logActs[i] = safeLog(orgActs[i]);
    }
   
   /**
      // Printf orgActs
      for(int t=0; t<totalTime; t++){  
        std::cout << t << " ";
        for(int c=0; c<numClasses; c++){
          std::cout << "( " <<c << ", " << orgActs[t*numClasses + c] <<", " << logActs[t*numClasses + c] <<")"<< "\t";
        }
        std::cout << std::endl;
       }


    for (int i = 0; i < totalTime; ++i) {
      int locMax = argMax(orgActs + i * numClasses);
      std::cout << "t " <<i << " Org maxIdx " << locMax << " max_act " << logActs[i*numClasses+locMax] << " exp_act " \
                << safeExp(logActs[i*numClasses+locMax]) << std::endl;
    }
  */
    PrefixPath tempPath;
    tempPath.path.clear();
    tempPath.probClass.resize(totalTime);
    tempPath.probBlank.resize(totalTime);
    tempPath.probClass[0] = safeLog(0);
    tempPath.probBlank[0] = logActs[blank];
    for(int t=1; t<totalTime; t++){
      tempPath.probClass[t] = safeLog(0);
      tempPath.probBlank[t] = logMul(logActs[t*numClasses + blank], tempPath.probBlank[t-1]);
    }
    tempPath.pathProb = tempPath.probBlank[totalTime-1];
    tempPath.extPathProb = safeLog( 1.0 -safeExp(tempPath.pathProb));

    std::vector<PrefixPath> livePathVec;
    livePathVec.push_back(tempPath);
    bestPath = tempPath;
    prePath = tempPath;
    prePathIndex = 0;
    //std::cout << "+++++++++++++++++BEGINE++++++++++++++++++++++numClasses " << numClasses << " totalTime " << totalTime << std::endl;

    // Search the max-best path probility
    bestPath = tempPath;
    prePath = tempPath;
    for(unsigned int i=0; i<inputPath.size(); i++){
        int k = inputPath[i];        
        tempPath.path = prePath.path;
        tempPath.path.push_back(k);
        tempPath.probClass.resize(totalTime);
        tempPath.probBlank.resize(totalTime);
        
        if(prePath.path.size()==0){
          tempPath.probClass[0] = logActs[k];
        }
        else{
          tempPath.probClass[0] = safeLog(0);     
        }

        tempPath.probBlank[0] = safeLog(0);
        float preProb = tempPath.probClass[0];
        for(int t=1; t<totalTime; t++){
          float newProb = prePath.probBlank[t-1];
          if(prePath.path.size()>0){
            if(prePath.path.back() !=k){
              newProb = logAdd(newProb, prePath.probClass[t-1]);
            }
          }
          tempPath.probClass[t] = logMul(logActs[t*numClasses + k] , \
             (logAdd(newProb , tempPath.probClass[t-1])) );
          tempPath.probBlank[t] = logMul(logActs[t*numClasses + blank] , \
             logAdd( tempPath.probClass[t-1], tempPath.probBlank[t-1]) );
          preProb = logAdd(preProb, logMul(logActs[t*numClasses+ k], newProb) );
        }
        tempPath.pathProb = logAdd(tempPath.probClass[totalTime-1], tempPath.probBlank[totalTime-1]);
        tempPath.extPathProb = safeLog(safeExp(preProb)- safeExp(tempPath.pathProb));
      //  std::cout << "Max-best of index " << i << " old_score " << bestPath.pathProb << \
            "new_score " << tempPath.pathProb << std::endl;
        prePath = tempPath;
        bestPath = tempPath;  
        //printPrefixPath( bestPath);
    }
    //std::cout << "tempPath.pathProb " << tempPath.pathProb << " exp_Prob " << safeExp(tempPath.pathProb)<< std::endl;
    delete [] logActs; 
    return tempPath; 
  }
