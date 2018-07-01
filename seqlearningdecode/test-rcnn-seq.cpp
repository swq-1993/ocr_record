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
ns_seq_ocr_model::CSeqModel g_seq_model;
nsseqocrchn::CSeqModelCh g_seq_model_ch;

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
    std::vector<rcnnchar::Box> detected_boxes_post;
    if (src_img.rows < 10 || src_img.cols < 10) {
        std::cerr << "The size of image is too small!" << std::endl; 
        return -1;
    }

    //2. run line generation codes
    double dt1 = (double)cv::getTickCount();
    std::vector<linegenerator::TextLineDetectionResult> text_line_detection_results;
    linegenerator::text_detection(src_img, g_cdnn_model, 
        text_line_detection_results, detected_boxes_post, 0);
    double dect_t = ((double)cv::getTickCount() - dt1) / (cv::getTickFrequency());
    if (g_thread_num == 1) {
        g_dect_time += dect_t;
    }

    std::vector<linegenerator::TextPolygon> polygons;
    std::vector<cv::Mat> undistorted_line_imgs;
    std::vector<cv::Mat> undistorted_masks;
    std::vector<cv::Mat> undistorted_coord_x;
    std::vector<cv::Mat> undistorted_coord_y;
    for (unsigned int i_poly = 0; i_poly < text_line_detection_results.size(); ++i_poly){
        polygons.push_back(text_line_detection_results[i_poly].get_text_polygon());
        undistorted_line_imgs.push_back(text_line_detection_results[i_poly].get_line_im());
        undistorted_masks.push_back(text_line_detection_results[i_poly].get_line_mask());
        undistorted_coord_x.push_back(text_line_detection_results[i_poly].get_coord_x());
        undistorted_coord_y.push_back(text_line_detection_results[i_poly].get_coord_y());
    }

    //convert text detection result to the struct which can be used by seq learning
    std::vector<SSeqLESegDecodeSScanWindow> in_line_dect_vec; 
    nsseqocrchn::cvt_det_rec_struct(text_line_detection_results, in_line_dect_vec);

    //sequence learning
    std::vector<SSeqLEngLineInfor> out_line_vec;
    int model_type = 0; 
    bool fast_flag = false;
    int ocr_model_mode = g_ini_file->get_int_value("OCR_MODEL_MODE",\
        IniFile::_s_ini_section_global);
    if (ocr_model_mode == 0) {
        fast_flag = false;
    } else {
        fast_flag = true;
    }

    // 0: use chn and eng model
    // 1: use chn model only
    // 2: use eng model only
    model_type = g_ini_file->get_int_value("CHN_ENG_MODEL",\
        IniFile::_s_ini_section_global);
    std::cout << "model_type = " << model_type << std::endl;
    if (model_type == 0) {
        std::cout << "chn and eng model are both used for output!" << std::endl;
    } else if (model_type == 1) {
        std::cout << "chn model is used only for output!" << std::endl;
    } else {
        std::cout << "eng model is used only for output!" << std::endl;
    }

    dt1 = (double)cv::getTickCount();
    for (int i_line = 0; i_line < in_line_dect_vec.size(); ++i_line) {
        if (g_debug_line_index!=-1 && i_line != g_debug_line_index) {
            text_line_detection_results.erase(text_line_detection_results.begin() 
                + out_line_vec.size());
            continue;
        }
        std::vector<SSeqLESegDecodeSScanWindow> c_in_line_dect_vec; 
        std::vector<SSeqLEngLineInfor> c_out_line_vec;
        c_in_line_dect_vec.push_back(in_line_dect_vec[i_line]);
        IplImage tmp_img = IplImage(undistorted_line_imgs[i_line]);
        IplImage* c_img = &tmp_img;
        // true: call the mul_row segmentation module
        // false: do not call the mul_row segmentation module
        // g_seq_model_ch.set_recg_cand_num(10);
        std::cerr << "recognize image " << file_name 
            << ", the " << i_line << "th line" << std::endl; 
        g_seq_model_ch.set_mul_row_seg_para(false);
        //g_seq_model_ch.set_save_low_prob_flag(false);
        g_seq_model_ch.recognize_image(c_img, c_in_line_dect_vec,
            model_type, fast_flag, c_out_line_vec);
        if (c_out_line_vec.size() <= 0){
            std::cerr << "a detection with no recognition result!" << std::endl; 
                text_line_detection_results.erase(text_line_detection_results.begin() 
                + out_line_vec.size());
        }
        else if (c_out_line_vec.size() == 1){
            out_line_vec.push_back(c_out_line_vec[0]);
        }
        else {
            out_line_vec.push_back(c_out_line_vec[0]);
            for (unsigned int i_rec = 1; i_rec < c_out_line_vec.size(); ++i_rec){
                text_line_detection_results.insert(text_line_detection_results.begin() 
                    + out_line_vec.size(), 
                text_line_detection_results[out_line_vec.size() - 1]);
                out_line_vec.push_back(c_out_line_vec[i_rec]);
                std::cerr << "more than one recognition results are returned!" << std::endl;
            }
        }
    }
    double recg_t = ((double)cv::getTickCount() - dt1) / (cv::getTickFrequency());
    if (g_thread_num == 1) {
        g_recg_time += recg_t;
    }

    nsseqocrchn::linegen_warp_output(text_line_detection_results, out_line_vec);

    //save recognition results
    int save_rst_mode = g_ini_file->get_int_value("SAVE_RST_MODE",
        IniFile::_s_ini_section_global);
    if (save_rst_mode == 0) {
        save_gbk_result(out_line_vec, recog_name.c_str(), 
            const_cast<char*>(file_name.c_str()), src_img.cols, src_img.rows);
    } else {
        save_utf8_result(out_line_vec, recog_name.c_str(), 
            const_cast<char*>(file_name.c_str()), src_img.cols, src_img.rows);
    }

    // save the char-box information and polygon information
    int debug_flag = g_ini_file->get_int_value("DRAW_RCNN_DECT_RST",
        IniFile::_s_ini_section_global);
    if (debug_flag) {
        cv::Mat vis_img = linegenerator::visualize_boxes(src_img, detected_boxes_post);
        cv::imwrite(im_box_name, vis_img);
        cv::Mat poly_img;
        linegenerator::visualize_poly(src_img, polygons, poly_img);
        cv::imwrite(im_poly_name, poly_img);
        nsseqocrchn::merge_line_image(undistorted_line_imgs, merge_img_path);
        nsseqocrchn::save_poly_img(poly_name, file_name, polygons,
            undistorted_line_imgs, undistorted_masks, 
            undistorted_coord_x, undistorted_coord_y,
            src_img);
    }

    return 0;
}

void* core_func(void *argv){
    mallopt(M_MMAP_MAX, 0);
    mallopt(M_TRIM_THRESHOLD, -1);

    bool pressure_flag = false;
    int local_loop_num = 0;
    if (0 == g_loop_num){
        pressure_flag = true;
        local_loop_num = 1;
    } else {
        pressure_flag = false;
        local_loop_num = g_loop_num;
    }

    while (local_loop_num){
        if (!pressure_flag){
            local_loop_num--;
        }

        for (unsigned int i = 0; i < (int)g_img_name_v.size(); ++i){
            g_image_num++;
            // for debug some sample image
            string str_debug_title = string(g_pdebug_title);
            int debug_title_len = str_debug_title.length();
            if (debug_title_len >= 2) {
                int ifind_pos = g_img_name_v[i].find(str_debug_title);
                if (ifind_pos == -1) {
                    continue ;
                }
                printf("debugTitle:%s\t length:%d\n", g_pdebug_title, debug_title_len);
            }
            std::string str_file_name = g_img_name_v[i];

            int ret = 0;
            ret = dect_recg_ocr_rst(str_file_name, g_output_path);
        }
    }
    return NULL;
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
    if (argc < 7){
        std::cerr << "./test-main [detection_model_file] [rec_model_file] [img_path]" 
            << " [res_path] [thread_num]" 
            << " [recg_thresh]" << std::endl;
        return -1;
    }
    char* detection_model_path = argv[1];
    char* recognition_model_path = argv[2];
    g_input_path = std::string(argv[3]);
    g_output_path = std::string(argv[4]);
    int thread_num = atoi(argv[5]);
    float recg_thresh = atof(argv[6]);
    g_loop_num = 1; 
    if (thread_num < 1 || thread_num >= 15) {
        std::cout << "invalid thread num value! range: [1, 15]" << std::endl;
        return -1;
    }
    g_thread_num = thread_num;
    std::cout << "thread num: " << thread_num << std::endl;

    if (cdnnInitModel(detection_model_path, g_cdnn_model, 1, true, true) != 0){
        fprintf(stderr, "model init error.\n");
        return -1;
    }
    int ret_val = nsseqocrchn::init_seq_model(argv[2], 
        g_seq_model, 
        g_seq_model_ch);

    //int ret_val = init_model_paddle_deepcnn(argv[2], 
    //     /*chn_paddle_flag*/ true, 
    //    /*chn_gpu_flag*/ false, 
    //    /*eng_paddle_flag*/ false,
    //    /*eng_gpu_flag*/ false, 
    //    g_seq_model, g_seq_model_ch);
    if (ret_val != 0) {
        fprintf(stderr, "seqocr model init error.\n");
        return -1;
    }
    std::cout << "seq model finished!" << std::endl;
    // initialize the enhace recognition model
    char model_name_ch[256];
    char table_name_ch[256];	
    snprintf(model_name_ch, 256, "./data/chn_data/enhance_model.bin");
    snprintf(table_name_ch, 256, "./data/chn_data/recg_table");
    g_seq_model.init_recog_model_enhance(model_name_ch, table_name_ch);
    g_seq_model_ch.init_enhance_recog_model_row(g_seq_model._m_recog_model_enhance, recg_thresh);


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
    ret_val = linegenerator::proc_output_dir(g_output_path);
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
    if (compute_time == 1) {
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
