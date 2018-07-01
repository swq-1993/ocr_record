/*******************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
/**
 * @file test-main.cpp
 * @author hanjunyu(com@baidu.com)
 * xieshufu made the modification@20150911
 * call the api module to run the recognition result
 * @date 2016/09/07 11:09:54
 * @brief 
 *  
 **/
#include <dirent.h>
#include <iostream>
#include <fstream>
#include <cv.h>
#include <highgui.h>
#include "time.h"
#include "dll_main.h"
#include "seq_ocr_model.h"
#include "seq_ocr_chn.h"
#include "ini_file.h"
#include "common_func.h"

const int compute_time = 1;
clock_t g_recog_clock = 0;
int g_image_num = 0;

std::string g_image_dir;
std::string g_row_dect_dir;
std::string g_decode_before_dir;
std::string g_decode_post_dir;

char g_pdebug_title[256];
extern char g_pimage_title[256];
extern char g_psave_dir[256];
extern int g_iline_index;
int g_debug_line_index;
void *DICore(void *arg);

// 0: load the detection result
// 1: the image is normalized to 48 height and input for recognition
int g_dect_mode = 0;

// initialize the seq model
int init_char_model(char *input_chn_model_path);

ns_seq_ocr_model::CSeqModel g_seq_model;
nsseqocrchn::CSeqModelCh g_seq_model_ch;

int main(int argc, char *argv[]) {
    
    if (argc < 6) {
        std::cout << "./test-main imgDir dectDir post_result_dir \
            chn_model_path thread_num" << std::endl;
        return -1;
    }
    char chn_model_path[256];
    char chn_model_fast_path[256];
    g_image_dir = argv[1];
    g_row_dect_dir = argv[2];
    g_decode_post_dir = argv[3];
    snprintf(chn_model_fast_path, 256, "%s", argv[4]);
   
    unsigned int thread_num = 1;
    sscanf(argv[5], "%d", &thread_num);

    int ret = init_char_model(chn_model_fast_path);
    if (ret != 0) {
        std::cout << "chinese single char model initialize failure!" << std::endl;
        return -1;
    }
    snprintf(g_pdebug_title, 256, "%s", "");
    if (argc >= 7) {
        snprintf(g_pdebug_title, 256, "%s", argv[6]);
        printf("Debug Title:%s\n", g_pdebug_title);
    }
    g_debug_line_index = -1;
    if (argc >= 8) {
        g_debug_line_index = atoi(argv[7]);
        printf("Debug line index:%d\n", g_debug_line_index);
    }

    clock_t ct1 = 0;
    clock_t ct2 = 0;

    if (compute_time == 1) {
        ct1 = clock();
        g_recog_clock = 0;
    }
    
    pthread_t *thp_dicore = (pthread_t *)calloc(thread_num, sizeof(pthread_t));
    for (unsigned int i = 0; i < thread_num; i++) {
        pthread_create(&(thp_dicore[i]), NULL, DICore, NULL);
    }	    
    for (unsigned int i = 0; i < thread_num; i++) {
        pthread_join(thp_dicore[i], NULL);
    }

    free(thp_dicore);
    thp_dicore = NULL;
    if (compute_time == 1) {
        ct2 = clock();
        float frecog_time =  (float)g_recog_clock / CLOCKS_PER_SEC;
        float favg_time = frecog_time / g_image_num;
        printf("CPU time %.4fs of %d imgs, average recog time: %.4f s\n", \
            frecog_time, g_image_num, favg_time);
        FILE *flog = fopen("./EvalTime.txt", "at");
        fprintf(flog, "chn_model_path: %s\n", chn_model_path);
        fprintf(flog, "chn_model_fast_path: %s\n", chn_model_fast_path);
        fprintf(flog, "TotalImageNum = %d, total-time:%.4f s average: %.4f s\n\n",\
                g_image_num, frecog_time, favg_time);
        fclose(flog);
    }
}

int loadFileDetectLineRects(std::string fileName,\
        std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec) {

    FILE *fd = fopen(fileName.c_str(), "r");
    if (fd == NULL) 
    {	
        return -1; 
    }

    int read_left = 0;
    int read_right = 0;
    int read_top = 0;
    int read_botom = 0;
    int read_is_black = 0;
    while (1) {
        int temp = fscanf(fd, "%d\t%d\t%d\t%d\t%d\t", &read_left, &read_right, \
                &read_top, &read_botom, &read_is_black);
        if (temp < 5) {
            break;
        }
        // check the validity of the information
        if (read_left < 0 || read_right < 0 || read_top < 0 || read_botom < 0) {
            continue;
        }
        SSeqLESegDecodeSScanWindow temp_line;
        temp_line.left = read_left;
        temp_line.right = read_right;
        temp_line.top  = read_top;
        temp_line.bottom = read_botom;
        temp_line.isBlackWdFlag = read_is_black;
        fscanf(fd, "\n");
        in_line_dect_vec.push_back(temp_line);
    }
    fclose(fd);
    
    return 0; 
}

int load_char_dect_rect(std::string file_name,\
    std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec, \
    int img_w, int img_h) {
    std::ifstream read_file;
    read_file.open(file_name.data(), std::ios::in);

    int ret = 0;
    if (!read_file) {
        ret = -1;
        return ret;
    }
    std::string line;
    //getline(read_file, line);
    //getline(read_file, line);
    //
    int left = 0;
    int top = 0;
    int wid = 0;
    int hei = 0;
    char str_line[1024];
    while (read_file.getline(str_line, 1024)) {
        std::stringstream word(str_line);
        word >> left;
        word >> top;
        word >> wid;
        word >> hei;
        if (left < 0 || top < 0 || wid <= 0 || hei <= 0) {
            continue ;
        }
        SSeqLESegDecodeSScanWindow dect_win;
        dect_win.left = left;
        dect_win.top = top;
        dect_win.right = std::min(left + wid - 1, img_w - 2);
        dect_win.bottom = std::min(top + hei - 1, img_h - 2);
        dect_win.isBlackWdFlag = 1;
        in_line_dect_vec.push_back(dect_win);
    }
    
    return 0; 
}

void *DICore(void *arg) {
   
    DIR *dir = NULL;
    struct dirent* ptr = NULL;  
    if ((dir = opendir(g_image_dir.data())) == NULL) {
        std::cerr << "Open dir failed. Dir: " << g_image_dir << std::endl;
        return NULL;
    }
    int index = 0;
    while (ptr = readdir(dir)) {
        if (ptr->d_name[0] == '.') {
            continue;
        }

        string str_file_name = string(ptr->d_name);
        int itxt_pos = str_file_name.find(".txt");
        if (itxt_pos >= 0) {
            continue;
        }
        
        // for debug some sample image
        string str_debug_title = string(g_pdebug_title);
        int debug_title_len = str_debug_title.length();
        if (debug_title_len >= 2) {
            int ifind_pos = str_file_name.find(str_debug_title);
            if (ifind_pos == -1) {
                continue ;
            }
            printf("debugTitle:%s\t length:%d\n", g_pdebug_title, debug_title_len);
        }
        int jpg_pos = str_file_name.find(".jpg");
        std::string file_title = str_file_name;
        if (jpg_pos > 0) {
            file_title = str_file_name.substr(0, jpg_pos);
        }
        snprintf(g_pimage_title, 256, "%s", file_title.data());
        
        std::string row_dect_path = g_row_dect_dir + "/" + ptr->d_name + ".txt";
        std::cout << "file name: " << str_file_name << std::endl;
        std::string image_name = g_image_dir;
        image_name += "/";
        image_name += ptr->d_name;
        IplImage *image = NULL;
        if ((image = cvLoadImage(image_name.c_str(), CV_LOAD_IMAGE_COLOR)) == 0) {
            std::cerr << "Error load Image " << image_name << std::endl;
            continue;
        }
        
        int img_w = image->width;
        int img_h = image->height;
        std::vector<SSeqLESegDecodeSScanWindow> in_line_dect_vec;
        int ret = load_char_dect_rect(row_dect_path, in_line_dect_vec, img_w, img_h);
        if (ret == -1) {
            std::cout << ptr->d_name << " no dect information!" << std::endl;
            continue ;
        }
        std::vector<SSeqLEngWordInfor> out_word_vec;
                
        clock_t ct1 = 0;
        clock_t ct2 = 0;

        ct1 = clock();
        g_seq_model_ch.recognize_single_char(image, in_line_dect_vec, out_word_vec);
        ct2 = clock();
        g_recog_clock += ct2 - ct1;

        int im_width = image->width;
        int im_height = image->height;
        std::string save_post_name = g_decode_post_dir + "/" + ptr->d_name + ".txt";
        FILE *file_save = fopen(save_post_name.data(), "wt");
        for (int i = 0; i < out_word_vec.size(); i++) {
            SSeqLEngWordInfor word = out_word_vec[i];
            char line_str[256];
            snprintf(line_str, 256, "%d\t%d\t%d\t%d\t%s\t%.4f\n",
                    word.left_, word.top_, word.wid_, word.hei_,
                    word.wordStr_.data(), word.reg_prob);
            fprintf(file_save, line_str);
            std::cout << line_str;
        }
        fclose(file_save);

        if (image) {
            cvReleaseImage(&image);
            image = NULL;
        }

        index++;
        g_image_num++;
    }
    closedir(dir); 

    return 0;
}


// initialize the seq model
int init_char_model(char *input_chn_model_path) {
    char model_dir[256];
    char table_name_ch[256];
    
    snprintf(model_dir, 256, "%s", "./data");
    snprintf(table_name_ch, 256, "%s/chn_data/table_10784", model_dir);
    int ret = 0;

    ret = g_seq_model.init_para();
    if (ret != 0) {
        return -1;
    }
    ret = g_seq_model.init_chn_char_recog_model(input_chn_model_path, table_name_ch);
    if (ret != 0) {
        return -1;
    }
    ret = g_seq_model_ch.init_char_recog_model(g_seq_model._m_recog_model_ch_char);
    if (ret != 0) {
        return -1;
    }

    return ret;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
