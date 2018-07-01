/***************************************************************************
 *
 * Copyright (c) 2017 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/



/**
 * @file test-recg-orient.cpp
 * @author xieshufu(com@baidu.com)
 * @date 2017/03/02 16:23:13
 * @brief
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
#include "ocr_orientation_seq.h"
#include "locrecg.h"
#include "seq_ocr_chn_convert.h"
#include "rcnn_loc.h"

std::vector<std::string> g_img_name_v;
std::string g_dect_dir = "";
void* g_cdnn_model = NULL;
std::string g_input_path;
std::string g_output_path;

//global variables used by recognition
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
    nsseqocrchn::CRcnnLoc rcnn_loc;
    rcnn_loc.init(g_cdnn_model, g_seq_model._m_recog_model_attr);

    std::string lang_type = "CHN_ENG";
    IplImage tmp_img = src_img;
    int ret = rcnn_loc.locate(&tmp_img, lang_type, g_dect_dir);
    if (ret != 0) {
        std::cerr << "locate image text lines error!" << std::endl;
        return -1;
    }
    nsseqocrchn::loc_result_t* loc_result = rcnn_loc.get_loc_result();
    double dect_t = ((double)cv::getTickCount() - dt1) / (cv::getTickFrequency());
    if (g_thread_num == 1) {
        g_dect_time += dect_t;
    }

    int debug_flag = g_ini_file->get_int_value("DRAW_RCNN_DECT_RST",
        IniFile::_s_ini_section_global);
    if (debug_flag) {
        std::vector<linegenerator::TextLineDetectionResult> text_lines;
        rcnn_loc.get_org_line_result(text_lines);
        std::vector<linegenerator::TextPolygon> polygons;
        for (unsigned int i_poly = 0; i_poly < text_lines.size(); ++i_poly){
            polygons.push_back(text_lines[i_poly].get_text_polygon());
        }
        cv::Mat poly_img;
        linegenerator::visualize_poly(src_img, polygons, poly_img);
        cv::imwrite(im_poly_name, poly_img);
    }

    //convert text detection result to the struct which can be used by seq learning
    std::vector<SSeqLESegDecodeSScanWindow> in_line_dect_vec;
    nsseqocrchn::CSeqOCRConvert::convert_input(*loc_result, in_line_dect_vec);

    //sequence learning
    std::vector<SSeqLEngLineInfor> out_line_vec;
    int model_type = 0;
    bool fast_flag = false;

    // 0: use chn and eng model
    // 1: use chn model only
    // 2: use eng model only
    //model_type = 1;
    model_type = g_ini_file->get_int_value("CHN_ENG_MODEL",\
        IniFile::_s_ini_section_global);
    dt1 = (double)cv::getTickCount();

    std::vector<SSeqLEngLineInfor> recg_rst_vec;
    for (int i_line = 0; i_line < in_line_dect_vec.size(); ++i_line) {
        if (g_debug_line_index!=-1 && i_line != g_debug_line_index) {
            continue;
        }
        std::vector<SSeqLESegDecodeSScanWindow> c_in_line_dect_vec;
        std::vector<SSeqLEngLineInfor> c_out_line_vec;
        c_in_line_dect_vec.push_back(in_line_dect_vec[i_line]);
        IplImage c_img(*(loc_result->regions[i_line].img_region));
        // true: call the mul_row segmentation module
        // false: do not call the mul_row segmentation module
        // g_seq_model_ch.set_recg_cand_num(10);
        g_seq_model_ch.set_mul_row_seg_para(false);
        // Note: in the service setting: false;
        // for data mining: true
        //g_seq_model_ch.set_save_low_prob_flag(true);

        g_seq_model_ch.set_formulate_output_flag(true);

        ////liushanshan number_recog
        //g_seq_model_ch.set_save_number_flag(true);
        ////general: set_time_step_number(3); number or eng: set_time_step_number(6)
        //g_seq_model_ch.set_time_step_number(6);
        g_seq_model_ch.recognize_image(&c_img, c_in_line_dect_vec,
            model_type, fast_flag, c_out_line_vec, true);
        int out_vec_size = c_out_line_vec.size();
        if (out_vec_size <= 0) {
            SSeqLEngLineInfor tmp_line;
            recg_rst_vec.push_back(tmp_line);
        } else {
            for (int j = 0; j < out_vec_size; j++) {
                recg_rst_vec.push_back(c_out_line_vec[j]);
            }
        }
        std::cerr << "recognize image " << file_name
            << ", the " << i_line << "th line" << std::endl;
    }

    nsseqocrchn::recg_result_t recg_result;
    nsseqocrchn::CSeqOCRConvert::convert_output(recg_rst_vec, recg_result, true);
    ret = rcnn_loc.post_act(&recg_result);
    //recg_result.print_info(true);
    double recg_t = ((double)cv::getTickCount() - dt1) / (cv::getTickFrequency());
    if (g_thread_num == 1) {
        g_recg_time += recg_t;
    }

    //recg_result.save_to_file(recog_name.data());
    recg_result.save_to_file_eval(recog_name.data());
    //save recognition results
    //save_utf8_result(out_line_vec, recog_name.c_str(),
    //    const_cast<char*>(file_name.c_str()), src_img.cols, src_img.rows);

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
    if (argc < 7){
        std::cerr << "./test-main [detection_model_file] [rec_model_file] [img_path]"
            << " [res_path] [thread_num]"
            << " [dect_dir:0-without dir dect/1-with dir dect]" << std::endl;
        return -1;
    }
    char* detection_model_path = argv[1];
    char* recognition_model_path = argv[2];
    g_input_path = std::string(argv[3]);
    g_output_path = std::string(argv[4]);
    int thread_num = atoi(argv[5]);
    int dect_dir = atoi(argv[6]);
    g_dect_dir = "v2_dir";
    if (dect_dir == 0) {
        g_dect_dir = "v2";
    } else {
        g_dect_dir = "v2_dir";
    }
    if (thread_num < 1 || thread_num > 10) {
        std::cout << "invalid thread num value! range: [1, 10]" << std::endl;
        return -1;
    }
    g_thread_num = thread_num;
    std::cout << "thread num: " << thread_num << std::endl;

    if (cdnnInitModel(detection_model_path, g_cdnn_model, 1, true, true) != 0){
        fprintf(stderr, "model init error.\n");
        return -1;
    }
    int ret_val = nsseqocrchn::init_seq_model(argv[2], g_seq_model, g_seq_model_ch);
    if (ret_val != 0) {
        fprintf(stderr, "seqocr model init error.\n");
        return -1;
    }
    std::cout << "seq model init ok!" << std::endl;

    ret_val = g_seq_model.init_ocr_attr_model();
    if (ret_val != 0) {
        std::cout << "ocr_attr model init error" << std::endl;
        return -1;
    }
    std::cout << "ocr_attr model init ok!" << std::endl;

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

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
