/**
 * test-rcnn-seq.cpp
 * Author huhan02(huhan02@baidu.com)
 * Create on: 2015-12-15
 *
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 *  
 **/

#include <dirent.h>
#include <string>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sys/stat.h>        
#include <sys/types.h>     
#include <sstream>
#include "malloc.h"
#include "dll_main.h"
#include "seq_ocr_model.h"
#include "seq_ocr_chn.h"
#include "ini_file.h"
#include "common.h"
#include "common_func.h"
#include "proc_func.h"
#include "seq_recg_model.h"

std::vector<std::string> g_img_name_v;
int g_loop_num = 0;
void* g_cdnn_model = NULL;
std::string g_input_path;
std::string g_output_path;

//global variables used by recognition
std::string g_decode_before_dir;
std::string g_decode_post_dir;
extern char g_pimage_title[256];
char g_pdebug_title[256];
int g_debug_line_index;
seq_recg_ns::CRecgModel g_seq_model;

const int compute_time = 1;
float g_recog_clock = 0;
int g_image_num = 0;
int g_thread_num = 0;
float g_dect_time = 0;
float g_recg_time = 0;

// text detection and recognition
int dect_recg_ocr_rst(std::string file_name, std::string save_dir) {
    
    std::string str_file_name = file_name;
    int jpg_pos = str_file_name.find(".jpg");

    std::string file_title = str_file_name;
    if (jpg_pos > 0) {
        file_title = str_file_name.substr(0, jpg_pos);
    }
    snprintf(g_pimage_title, 256, "%s", file_title.data());

    std::string im_inpath = g_input_path + "/" + file_name;
    std::string im_box_name = save_dir 
        + "/detection_result/" + file_title + "_box.jpg"; 
    std::string im_poly_name = save_dir 
        + "/detection_result/" + file_title + "_poly.jpg";
    std::string poly_name = im_poly_name + ".txt";
    std::string merge_img_path = save_dir 
        + "/detection_result/" + file_title + "_poly_cor.jpg"; 
    std::string recog_name = save_dir 
        + "/recog_result/" + file_name + ".txt";

    //1. load image and run fast rcnn algorithm
    cv::Mat src_img = cv::imread(im_inpath);
    //sequence learning
    IplImage p_image = src_img;

    std::vector<seq_recg_ns::DectRect> in_line_dect_vec;
    std::vector<seq_recg_ns::RecgLineInfor> out_line_vec;
    seq_recg_ns::DectRect dect_rect;
    dect_rect.left = 0;
    dect_rect.top = 0;
    dect_rect.right = p_image.width - 1;
    dect_rect.bottom = p_image.height - 1;
    dect_rect.isBlackWdFlag = 1;
    in_line_dect_vec.push_back(dect_rect);

    g_seq_model.recognize_line(&p_image,\
        in_line_dect_vec,\
        out_line_vec);

    FILE *file_save = fopen(recog_name.data(), "wt");
    for (int i = 0; i < out_line_vec.size(); i++) {
        fprintf(file_save, "%d\t%d\t%d\t%d\t%s\t",
            out_line_vec[i].left_, out_line_vec[i].top_,
            out_line_vec[i].wid_, out_line_vec[i].hei_,
            out_line_vec[i].line_str.data());
        std::cout << "[" << out_line_vec[i].left_ << ","
            << out_line_vec[i].top_ << ","
            << out_line_vec[i].wid_ << ","
            << out_line_vec[i].hei_ << ","
            << out_line_vec[i].line_str << "]" << std::endl;
        for (int j = 0; j < out_line_vec[i].word_vec.size(); j++) {
            std::cout << " [" << out_line_vec[i].word_vec[j].left_ << ","
                << out_line_vec[i].word_vec[j].top_ << ","
                << out_line_vec[i].word_vec[j].wid_ << ","
                << out_line_vec[i].word_vec[j].hei_ << ","
                << out_line_vec[i].word_vec[j].wordStr_ << "] ";
        
            fprintf(file_save, "%d\t%d\t%d\t%d\t%s\t%.4f\t",
                out_line_vec[i].word_vec[j].left_, 
                out_line_vec[i].word_vec[j].top_,
                out_line_vec[i].word_vec[j].wid_, 
                out_line_vec[i].word_vec[j].hei_,
                out_line_vec[i].word_vec[j].wordStr_.data(),
                out_line_vec[i].word_vec[j].reg_prob
                );
        }
        fprintf(file_save, "\n");
    }
    fclose(file_save);

    return 0;
}

void* core_func_thread(void *argv){
    mallopt(M_MMAP_MAX, 0);
    mallopt(M_TRIM_THRESHOLD, -1);
    
    int thread_id = *((int *)argv);

    for (int i = 0; i < g_img_name_v.size(); i++) {
        if (i % g_thread_num != thread_id) {
            continue;
        }
        std::string str_file_name = g_img_name_v[i];
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
        int ret = 0;
        ret = dect_recg_ocr_rst(str_file_name, g_output_path);
    }
    return NULL;
}

int main(int argc, char* argv[]){
    if (argc < 6){
        std::cerr << "./test-main [rec_model_file] [img_path]" 
            << " [res_path] [thread_num]" 
            << " [loop_num]" << std::endl;
        return -1;
    }
    char* recognition_model_path = argv[1];
    g_input_path = std::string(argv[2]);
    g_output_path = std::string(argv[3]);
    int thread_num = atoi(argv[4]);
    g_loop_num = atoi(argv[5]);
    if (thread_num < 1 || thread_num > 10) {
        std::cout << "invalid thread num value! range: [1, 10]" << std::endl;
        return -1;
    }
    g_thread_num = thread_num;
    std::cout << "thread num: " << thread_num << std::endl;

    g_seq_model.init_model(recognition_model_path, "./table_recg", 127.5, 48);
    std::cout << "seq model finished!" << std::endl;

    snprintf(g_pdebug_title, 256, "");
    g_debug_line_index = -1;
    snprintf(g_pdebug_title, 256, "%s", "");
    if (argc >= 8) {
        snprintf(g_pdebug_title, 256, "%s", argv[7]);
        printf("Debug Title:%s\n", g_pdebug_title);
    }
    g_debug_line_index = -1;
    if (argc >= 9) {
        g_debug_line_index = atoi(argv[8]);
        printf("Debug line index:%d\n", g_debug_line_index);
    }
    get_img_list(g_input_path, g_img_name_v);

    // create the output directory
    int ret_val = linegenerator::proc_output_dir(g_output_path);
    if (ret_val != 0) {
        fprintf(stderr, "proc output dir error.\n");
        return -1;
    }
    double dt1 = 0;
    double dt2 = 0;

    g_recog_clock = 0;
    dt1 = cv::getTickCount();
    int img_num = g_img_name_v.size();
    if (img_num <= 0) {
        std::cout << "no image files in the test directory!" << std::endl;
        return -1;
    }
    pthread_t *thp_core = (pthread_t *)calloc(thread_num, sizeof(pthread_t));
    
    int *id_args = (int *)calloc(thread_num, sizeof(int));
    int valid_thread_num = 0;
    for (int i = 0; i < g_thread_num; i++){
        id_args[i] = i;
        pthread_create(&(thp_core[i]), NULL, core_func_thread, id_args + i);
    }
    for (int i = 0; i < g_thread_num; i++){
        pthread_join(thp_core[i], NULL);
    }
    if (id_args) {
        free(id_args);
        id_args = NULL;
    }
    free(thp_core);
    thp_core = NULL;

    if (compute_time == 1 && g_thread_num == 1) {
        dt2 = cv::getTickCount();
        g_recog_clock = (dt2 - dt1) / cv::getTickFrequency();
        g_image_num = img_num;
        float focr_time =  (float)g_recog_clock;
        float favg_time = focr_time / g_image_num;
        float avg_dect_time = g_dect_time / g_image_num;
        float avg_recg_time = g_recg_time / g_image_num;
        printf("total time %.4f s of %d imgs, average ocr time: %.4f s\n", \
            focr_time, g_image_num, favg_time);
        printf("avg dect time: %.4f s, avg recg time: %.4f s\n", avg_dect_time, avg_recg_time);
        FILE *flog = fopen("./EvalTime.txt", "at");
        fprintf(flog, "chn_model_path: %s\n", recognition_model_path);
        fprintf(flog, "TotalImageNum = %d, total-time:%.4f s average: %.4f s\n",\
            g_image_num, focr_time, favg_time);
        fprintf(flog, "avg dect time: %.4f s, avg recg time: %.4f s\n\n",\
            avg_dect_time, avg_recg_time);
        fclose(flog);
    }

    if (NULL != g_cdnn_model){
        cdnnReleaseModel(&g_cdnn_model);
        g_cdnn_model = NULL;
    }
    return 0;
}
