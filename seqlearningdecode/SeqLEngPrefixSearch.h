/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file SeqLEngPrefixSearch.h
 * @author hanjunyu(com@baidu.com)
 * @date 2014/07/08 20:53:42
 * @brief 
 *  
 **/




#ifndef  __SEQLENGPREFIXSEARCH_H_
#define  __SEQLENGPREFIXSEARCH_H_

#include <iostream>
#include <vector>
#include <complex>

#ifdef PADDLE_TYPE_DOUBLE
  #define LOG_ZERO -100
  #define LOG_INFINITY 100
  #define EXP_MAX 1.7976e+308
  #define EXP_MIN 2.2250e-308
  #define EXP_LIMIT 709.78
#else
  #define LOG_ZERO -87
  #define LOG_INFINITY 88
  #define EXP_MAX 3.4028e+38
  #define EXP_MIN 1.17549e-38
  #define EXP_LIMIT 88.722
#endif
static inline float safeExp(float x) {
  if (x <= LOG_ZERO) {
    return 0;
  }
  if (x >= EXP_LIMIT) {
    return EXP_MAX;
  }
  return std::exp(x);
}
static inline float safeLog(float x) {
    if (x <= EXP_MIN) {
      return LOG_ZERO;
    }
    return std::log(x);
}
 // x=lna and y=lnb is log scale, ln(a*b)=lna+lnb
static inline float logMul(float x, float y) {
   if (x <= LOG_ZERO || y <= LOG_ZERO) {
    return LOG_ZERO;
   }
   return x + y;
}

// x=lna and y=lnb is log scale, ln(a+b)=lna+ln(1+exp(lnb-lna)), where b < a
static inline float logAdd(float x, float y) {
  if (x <= LOG_ZERO) {
    if (y <= LOG_ZERO) {
      return LOG_ZERO;
    }
      return y;
  }
  if (y <= LOG_ZERO) {
    if (x <= LOG_ZERO) {
      return LOG_ZERO;
    }
    return x;
  }
  if (x < y) {
    float t = y;
    y = x;
    x = t;
  }
  return x + safeLog(1+safeExp(y-x));
}


struct PrefixPath{
    std::vector<int> path;
    std::vector<float> probClass;
    std::vector<float> probBlank;
    float pathProb;
    float extPathProb;
};



PrefixPath onePrefixPathInfor(std::vector<int> & inputPath, float * orgActs, \
         int numClasses, int totalTime, int blank);










#endif  //__SEQLENGPREFIXSEARCH_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
