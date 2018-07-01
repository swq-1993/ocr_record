/*******************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
/**
 * @file test-paddle-batch.cpp
 * xieshufu made the modification@20150911
 * call the api module to run the recognition result
 * @date 2017/06/13 11:09:54
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
#include "seq_ocr_chn_batch.h"
#include "ini_file.h"
#include "common_func.h"
#include "proc_func.h"

const int compute_time = 1;
clock_t g_recog_clock = 0;
int g_image_num = 0;
double g_recg_time = 0;
double g_dect_time = 0;

std::string g_image_dir;
std::string g_result_path;
std::string g_output_path;
std::string g_list_path; 

extern char g_pimage_title[256];
extern char g_psave_dir[256];
int g_batch_size;
std::vector<std::string> g_img_vec;
std::vector<int> g_idx_vec;
std::vector<std::string> g_gt_vec;
std::vector<int> g_wid_vec;
std::vector<int> g_hei_vec;

int get_sort_img_list(std::string list_path, 
    std::vector<std::string> &img_vec,
    std::vector<int> &idx_vec);

void *paddle_recog(void *arg);
int test_paddle_recog(int argc, char *argv[]);
CSeqLEngWordRegModel *g_recg_model = NULL;
CSeqLAsiaWordReg *g_recg_module = NULL;

int main(int argc, char *argv[]) {
    // only recognition
    return test_paddle_recog(argc, argv);
}

void *paddle_recog(void *arg) {
   
    int index = 0;
    int total_img_num = g_img_vec.size();
    int batch_num = total_img_num / g_batch_size;
    if (total_img_num % g_batch_size != 0) {
        batch_num += 1;
    }
    std::cout << "total image num: " << total_img_num << std::endl;
    std::cout << "batch size: " << g_batch_size << " batch num: " << batch_num << std::endl;
        
    int correct_num = 0;
    FILE *file_summary = NULL;
    FILE *file_save = NULL;
    std::string rst_path = g_result_path;
    file_save = fopen((char*)rst_path.data(), "at");
    std::string rst_summary_path = g_result_path + "_summary.txt";
    file_summary = fopen(rst_summary_path.data(), "at");
    for (int batch_idx = 0; batch_idx < batch_num; batch_idx++) {
        std::vector<st_batch_img> vec_batch_info;
        std::vector<int> vec_wid;
        std::vector<int> vec_hei;
        std::vector<std::string> vec_file_name;
        std::vector<std::string> vec_gt;
        //std::cout << "batch_idx: " << batch_idx << "/" << batch_num << std::endl;
        for (int j = 0; j < g_batch_size; j++) {
            int img_idx = batch_idx * g_batch_size + j;
            if (img_idx >= total_img_num) {
                break;
            }

            st_batch_img batch_img;
            int index = g_idx_vec[img_idx];
            std::string image_name = g_img_vec[index];
            batch_img.img = cvLoadImage(image_name.c_str(), CV_LOAD_IMAGE_COLOR);
            if (batch_img.img == NULL) {
                std::cout << "could not load image! " << image_name << std::endl;
                continue;
            }
            batch_img.bg_flag = 1;
            vec_batch_info.push_back(batch_img);
            vec_file_name.push_back(g_img_vec[index]);
            vec_gt.push_back(g_gt_vec[index]);
            vec_wid.push_back(g_wid_vec[index]);
            vec_hei.push_back(g_hei_vec[index]);
        }
        if (vec_batch_info.size() <= 0) {
            break;
        }
        
        std::vector<SSeqLEngLineInfor> out_batch_vec;      
        g_recg_module->recognize_row_img_batch(vec_batch_info,
            out_batch_vec);
        
        for (unsigned int i = 0; i < out_batch_vec.size(); i++) {
            std::vector<SSeqLEngWordInfor>& word_vec = out_batch_vec[i].wordVec_;
            std::string rst_str = "";
            int word_vec_len = word_vec.size();
            if (word_vec_len > 0) {
                SSeqLEngWordInfor &word = word_vec[0];
                char tmp_str[256];
                for (int j = 0; j < word.pathVec_.size(); j++) {
                    if (j == word.pathVec_.size() - 1) {
                        snprintf(tmp_str, 256, "%d", word.pathVec_[j]);
                    } else {
                        snprintf(tmp_str, 256, "%d,", word.pathVec_[j]);
                    }
                    rst_str += std::string(tmp_str);
                }
            }
            int flag = 0;
            if (rst_str == vec_gt[i]) {
                flag = 1;
                correct_num++;
            } else {
                fprintf(file_save, "%d %d %s %s %s %d\n", vec_wid[i], vec_hei[i],
                    vec_file_name[i].data(), vec_gt[i].data(), rst_str.data(), flag);
            }
        }

        for (int j = 0; j < vec_batch_info.size(); j++) {
            cvReleaseImage(&vec_batch_info[j].img);
            vec_batch_info[j].img = NULL;
        }
    }
    float correct_rate = (float)correct_num / total_img_num;
    fprintf(file_summary, "%s\t%d\t%d\t%.4f\n", 
        g_list_path.data(), total_img_num, correct_num, correct_rate);
    fclose(file_save);
    fclose(file_summary);
    std::cout << "batch_size:" << g_batch_size << " total_num: " << total_img_num
        << " correct_rate: " << correct_rate << " finished!" << std::endl;
    
    return 0;
}

int test_paddle_recog(int argc, char *argv[]) {
    
    if (argc < 5) {
        std::cout << "./test-main [train_list_path] [result_path] \
            [model_path] [batch_size]" << std::endl;
        return -1;
    }
    char recg_model_path[256];
    char table_name[256];
    g_list_path = argv[1];
    g_result_path = argv[2];
    snprintf(recg_model_path, 256, "%s", argv[3]);
    snprintf(table_name, 256, "./data/recg_table");
   
    unsigned int thread_num = 1;
    g_batch_size = atoi(argv[4]);
    if (g_batch_size < 1 || g_batch_size > 100) {
        std::cout << "batch_size should be in [1, 100]!" << std::endl;
        return -1;
    }
    bool paddle_flag = true;
    bool gpu_flag = true;
    
    init_para_file();
    g_recg_model = new CSeqLEngWordRegModel(\
        recg_model_path, table_name, 
        paddle_flag, gpu_flag, 127.5, 48, true);
    if (g_recg_model == NULL) {
        std::cout << "recg model initialize failure!" << std::endl;
        return -1;
    }
    g_recg_model->set_batch_size(g_batch_size);
    g_recg_module = new CSeqLAsiaWordReg(*g_recg_model);
    std::cout << "recog model initialized ok!" << std::endl;

    int ret =  get_sort_img_list(g_list_path, 
        g_img_vec, g_idx_vec);
    g_image_num = g_img_vec.size();

    double dt1 = 0;
    double dt2 = 0;

    dt1 = cv::getTickCount();
    paddle_recog(NULL);

    if (compute_time == 1) {
        dt2 = cv::getTickCount();
        double frecog_time = (dt2 - dt1) / cv::getTickFrequency();
        double favg_time = frecog_time / g_image_num;        
        printf("CPU time %.4fs of %d imgs, average recog time: %.4f s\n", \
            frecog_time, g_image_num, favg_time);
        FILE *flog = fopen("./EvalTime.txt", "at");
        fprintf(flog, "recg_model_path: %s\n", recg_model_path);
        fprintf(flog, "batch_size: %d\n", g_batch_size);
        fprintf(flog, "TotalImageNum = %d, total-time:%.4f s average: %.4f s\n\n",\
                g_image_num, frecog_time, favg_time);
        fclose(flog);
    }
    if (g_recg_model) {
        delete g_recg_model;
        g_recg_model = NULL;
    }
    if (g_recg_module) {
        delete g_recg_module;
        g_recg_module = NULL;
    }

    return 0;
}

void split_str(const std::string& src, std::string sep_char, 
    std::vector<std::string>& vec_str)  
{  
    int sep_char_len = sep_char.size();
    int src_len = src.size();
    int last_pos = 0;
    int index = -1;  
    while (-1 != (index = src.find(sep_char, last_pos)))  
    {  
        vec_str.push_back(src.substr(last_pos, index - last_pos));  
        last_pos = index + sep_char_len;  
    }  
    std::string last_str = src.substr(last_pos, src_len - last_pos);
    if (!last_str.empty()) {
        vec_str.push_back(last_str);
    }
    return ;
}  

int get_sort_img_list(std::string list_path, 
    std::vector<std::string> &img_vec,
    std::vector<int> &idx_vec) {
    
    int ret_val = 0;
    std::ifstream read_file;
    read_file.open(list_path.c_str());
    if (!read_file) {
        std::cout << "could not open the list path!" << list_path << std::endl;        
        return -1; 
    }

    std::vector<std::string> label_vec;
    std::string line;
    std::string sep_char = " ";
    while (std::getline(read_file, line)) {
        std::vector<std::string> vec_str;
        split_str(line, sep_char, vec_str); 
        if (vec_str.size() != 4) {
            std::cout << "invalid input list format!" << std::endl;
            break;
        }
        int wid = 0;
        int hei = 0;
        std:string img_path;
        std::string label_str;
        wid = atoi(vec_str[0].data());
        hei = atoi(vec_str[1].data());
        img_path = vec_str[2];
        label_str = vec_str[3];
        // check the validity of the information
        if (wid < 0 || hei < 0) {
            std::cout << "invalid string!" << std::endl;
            continue;
        }
        g_wid_vec.push_back(wid);
        g_hei_vec.push_back(hei);
        g_img_vec.push_back(img_path);
        g_gt_vec.push_back(label_str);
    }
    read_file.close();

    int img_num = img_vec.size();
    g_idx_vec.resize(img_num);
    quicksort(&g_idx_vec[0], &g_wid_vec[0], img_num);
    std::string sort_list_path = list_path + ".sort";
    FILE *file_save = fopen((char*)sort_list_path.data(), "wt");
    for (int i = 0; i < img_num; i++) {
        int sort_idx = idx_vec[i];
        fprintf(file_save, "%d %d %s %s\n", g_wid_vec[sort_idx],
            g_hei_vec[sort_idx], g_img_vec[sort_idx].data(),
            g_gt_vec[sort_idx].data());
    }
    fclose(file_save);
    return ret_val;
}
/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
