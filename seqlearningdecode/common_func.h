/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file common_func.h
 * @author xieshufu(com@baidu.com)
 * @date 2015/03/18 14:00:57
 * @brief 
 *  
 **/

#ifndef  APP_SEARCH_VIS_OCR_ALG_SEQLEARNINGDECODE_COMMON_FUNC_H
#define  APP_SEARCH_VIS_OCR_ALG_SEQLEARNINGDECODE_COMMON_FUNC_H

#include <string>
#include <cv.h>
#include <highgui.h>
#include "SeqLDecodeDef.h"
#include "dll_main.h"
std::string seq_lunicode_vec_2_utf8_str(std::vector<int>& vec);
void seq_lutf8_str_2_unicode_vec(const char* ps, std::vector<int>& vec);

void draw_rectangle(unsigned char *im, int w, int h, int left, int right,\
                    int top, int bottom, int ext, \
                    unsigned char r, unsigned char g, unsigned char b);
int save_color_data(unsigned char *im, int w, int h, char *filepath);

bool check_punc(int code);
int check_num_str(std::string strLabel, bool &bNum);
int check_eng_str(std::string strLabel, bool &bEng, bool &bHighLowType);
int check_chn(std::string character, bool &bchn);

template <class T>
void qswap_(T &a, T &b) {
    T temp = a;
    a = b;
    b = temp;
}

template <class T>
void quicksort(int *index, T *values, int start, int end) {
    if (start >= end - 1) {
        return;
    }

    T &pivot = values[index[(start + end - 1) / 2]];

    int lo = start;
    int hi = end;

    for (;;) {
            while (lo < hi && values[index[lo]] < pivot) {
                lo++;
        }

        while (lo < hi && values[index[hi - 1]] >= pivot) {
            hi--;
        }

        if (lo == hi || lo == hi - 1) {
            break;
        }

        qswap_(index[lo], index[hi - 1]);
        lo++;
        hi--;
    }

    int split1 = lo;

    hi = end;

    for (;;) {
        while (lo < hi && values[index[lo]] == pivot) {
            lo++;
        }

        while (lo <hi && values[index[hi - 1]]>pivot) {
            hi--;
        }

        if (lo == hi || lo == hi - 1) {
            break;
        }

        qswap_(index[lo], index[hi - 1]);
        lo++;
        hi--;
    }

    int split2 = lo;
    quicksort(index, values, start, split1);
    quicksort(index, values, split2, end);
}

template <class T>
void quicksort(int *index, T *values, int nsize) {
    int ii = 0;

    for (ii = 0; ii < nsize; ii++) {
        index[ii] = ii;
    }

    quicksort(index, values, 0, nsize);
}
void large_value_index(float *pVector, int iDim, int iSelNum, int *pSelIndex);

// find the k largest number in the array
int large_value_quick_index(float *pVector, int iDim, int iSelNum, int *pSelIndex);

// rotate the image with one angle
IplImage* rotate_image(IplImage* src, int angle, bool clockwise);

char* utf8_to_ansi(const std::string from);
void save_utf8_result(std::vector<SSeqLEngLineInfor> out_line_vec,\
    const char* savename, char *imgname, int imWidth, int imHeight);
void save_gbk_result(std::vector<SSeqLEngLineInfor> out_line_vec,\
    const char* savename, char *imgname, int imWidth, int imHeight);

void save_utf8_result_prob(std::vector<SSeqLEngLineInfor> out_line_vec,\
    const char* savename, char *imgname, int imWidth, int imHeight);

void save_utf8_result_mul_cand(std::vector<SSeqLEngLineInfor> out_line_vec,\
    const char* savename, char *imgname, int imWidth, int imHeight);

void save_utf8_result_mul_cand_menu(std::vector<SSeqLEngLineInfor> out_line_vec,\
    const char* savename, char *imgname, int imWidth, int imHeight);

void save_gbk_result_mul_cand(std::vector<SSeqLEngLineInfor> out_line_vec,\
    const char* savename, char *imgname, int imWidth, int imHeight);

int get_img_list(std::string input_dir, std::vector<std::string> &img_name_v);

void save_utf8_result_batch(std::vector<SSeqLEngLineInfor> out_line_vec,\
    std::vector<std::string> img_name_v,
    const char* savename);

void post_proc_line_recog_result(std::vector<SSeqLEngLineInfor> &out_line_vec);

void merge_two_line(SSeqLEngLineInfor& line_a, SSeqLEngLineInfor& line_b);

#endif  //__COMMON_FUNC_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
