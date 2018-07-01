/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file DllMain.h
 * @author xieshufu(com@baidu.com)
 * @date 2015/03/18 14:00:57
 * @brief 
 *  
 **/

#ifndef  APP_SEARCH_VIS_OCR_ALG_SEQLEARNINGDECODE_DLL_MAIN_H
#define  APP_SEARCH_VIS_OCR_ALG_SEQLEARNINGDECODE_DLL_MAIN_H

#include "ini_file.h"

// the adjustable parameter from a initial file
extern IniFile *g_ini_file;

// initialize the parameter file
int init_para_file();
// free the parameter
int free_para_file();

#endif
