/*******************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
/**
 * @file test-row.cpp
 * @author hanjunyu(com@baidu.com)
 * xieshufu made the modification@20150911
 * call the api module to run the recognition result
 * @date 2014/06/17 11:09:54
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
std::string g_row_dect_dir;
std::string g_output_path;
std::vector<string> g_img_name_v;

char g_pdebug_title[256];
extern char g_pimage_title[256];
extern char g_psave_dir[256];
extern int g_iline_index;
int g_debug_line_index;
int g_batch_size;
void *row_recog(void *arg);
void *dect_recog(void *arg);
int dect_recg_image(std::string file_name, std::string save_dir);

void* g_cdnn_model = NULL;
ns_seq_ocr_model::CSeqModel g_seq_model;
nsseqocrchn::CSeqModelChBatch g_seq_model_ch_batch;

int test_row_recog(int argc, char *argv[]);
int test_dect_recog(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    
    // only recognition
    return test_row_recog(argc, argv);
    // detection + recognition
    //return test_dect_recog(argc, argv);
}

void *row_recog(void *arg) {
   
    int index = 0;
    int total_img_num = g_img_name_v.size();
    int batch_num = total_img_num / g_batch_size;
    if (total_img_num % g_batch_size != 0) {
        batch_num += 1;
    }
    std::cout << "total image num: " << total_img_num << std::endl;
    std::cout << "batch size: " << g_batch_size << " batch num: " << batch_num << std::endl;
    int model_type = g_ini_file->get_int_value("CHN_ENG_MODEL",\
        IniFile::_s_ini_section_global);
    if (model_type == 0) {
        std::cout << "chn and eng model are both used!" << std::endl;
    } else {
        std::cout << "chn model is used only!" << std::endl;
    }
        
    std::vector<SSeqLEngLineInfor> out_line_vec;      
    for (int batch_idx = 0; batch_idx < batch_num; batch_idx++) {
        std::vector<st_batch_img> vec_batch_info;
        std::vector<std::string> vec_file_name;
        std::cout << "batch_idx: " << batch_idx << "/" << batch_num << std::endl << std::endl;
        for (int j = 0; j < g_batch_size; j++) {
            int img_idx = batch_idx * g_batch_size + j;
            if (img_idx >= total_img_num) {
                break;
            }

            st_batch_img batch_img;
            std::string image_name = g_image_dir + "/" + g_img_name_v[img_idx];
            batch_img.img = cvLoadImage(image_name.c_str(), CV_LOAD_IMAGE_COLOR);
            if (batch_img.img == NULL) {
                std::cout << "could not load image! " << image_name << std::endl;
                continue;
            }
            batch_img.bg_flag = 1;
            vec_batch_info.push_back(batch_img);
            vec_file_name.push_back(g_img_name_v[img_idx]);
        }
        if (vec_batch_info.size() <= 0) {
            break;
        }
        
        bool fast_flag = true;
        g_seq_model_ch_batch.set_debug_line_index(g_debug_line_index);
        g_seq_model_ch_batch.set_mul_row_seg_para(false);

        std::vector<SSeqLEngLineInfor> out_batch_vec;      
        g_seq_model_ch_batch.recognize_image_batch(vec_batch_info,
            model_type, fast_flag, out_batch_vec);
        for (int j = 0; j < out_batch_vec.size(); j++) {
            out_line_vec.push_back(out_batch_vec[j]);
        }
        
        for (unsigned int i = 0; i < out_batch_vec.size(); i++) {
            printf("%s: [%d, %d, %d,%d] %s\n",\
                vec_file_name[i].data(),
                out_batch_vec[i].left_, out_batch_vec[i].top_,\
                out_batch_vec[i].wid_, out_batch_vec[i].hei_, out_batch_vec[i].lineStr_.data());
        }

        for (int j = 0; j < vec_batch_info.size(); j++) {
            cvReleaseImage(&vec_batch_info[j].img);
            vec_batch_info[j].img = NULL;
        }
    }
    std::cout << "result UTF8 mode" << std::endl;
    std::cout << "save result ..." << std::endl;
    char save_path[256];
    snprintf(save_path, 256, "%s/ocr_recg_result.txt", g_output_path.data());
    save_utf8_result_batch(out_line_vec, g_img_name_v, save_path);
    std::cout << "finished!" << std::endl;
    
    return 0;
}

int test_row_recog(int argc, char *argv[]) {
    
    if (argc < 6) {
        std::cout << "./test-main [imgDir] [dect_dir] [result_dir] \
            [model_path] [line_num]" << std::endl;
        return -1;
    }
    char chn_model_path[256];
    char chn_model_fast_path[256];
    g_image_dir = argv[1];
    g_row_dect_dir = argv[2];
    g_output_path = argv[3];
    snprintf(chn_model_fast_path, 256, "%s", argv[4]);
   
    unsigned int thread_num = 1;
    g_batch_size = atoi(argv[5]);
    if (g_batch_size < 1 || g_batch_size > 100) {
        std::cout << "batch_size should be in [1, 100]!" << std::endl;
        return -1;
    }
    bool chn_paddle_flag = true;
    bool chn_gpu_flag = true;
    bool eng_paddle_flag = false;
    bool eng_gpu_flag = false;
    int ret = nsseqocrchn::init_model_paddle_deepcnn(chn_model_fast_path, 
        chn_paddle_flag, chn_gpu_flag, eng_paddle_flag, eng_gpu_flag,
        g_seq_model, g_seq_model_ch_batch);
    if (ret != 0) {
        std::cout << "chinese seqocr model initialize failure!" << std::endl;
        return -1;
    }
    std::cout << "all recog model initialized ok!" << std::endl;

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
    
    get_img_list(g_image_dir, g_img_name_v);
    g_image_num = g_img_name_v.size();

    double dt1 = 0;
    double dt2 = 0;

    dt1 = cv::getTickCount();
    // Thread 0: the main thread
    row_recog(NULL);

    if (compute_time == 1) {
        dt2 = cv::getTickCount();
        double frecog_time = (dt2 - dt1) / cv::getTickFrequency();
        double favg_time = frecog_time / g_image_num;        
        printf("CPU time %.4fs of %d imgs, average recog time: %.4f s\n", \
            frecog_time, g_image_num, favg_time);
        FILE *flog = fopen("./EvalTime.txt", "at");
        fprintf(flog, "chn_model_fast_path: %s\n", chn_model_fast_path);
        fprintf(flog, "TotalImageNum = %d, total-time:%.4f s average: %.4f s\n\n",\
                g_image_num, frecog_time, favg_time);
        fclose(flog);
    }

    return 0;
}

int test_dect_recog(int argc, char *argv[]) {
    if (argc < 5) {
        std::cerr << "./test-main [detection_model_file] [rec_model_file] [img_path]" 
            << " [res_path] " << std::endl;
        return -1;
    }
    char* detection_model_path = argv[1];
    char* recognition_model_path = argv[2];
    g_image_dir = argv[3];
    g_output_path = argv[4];
    
    if (cdnnInitModel(detection_model_path, g_cdnn_model, 1, true, true) != 0){
        fprintf(stderr, "model init error.\n");
        return -1;
    }
   
    bool chn_paddle_flag = true;
    bool chn_gpu_flag = true;
    bool eng_paddle_flag = false;
    bool eng_gpu_flag = false;
    int ret = nsseqocrchn::init_model_paddle_deepcnn(recognition_model_path, 
        chn_paddle_flag, chn_gpu_flag,
        eng_paddle_flag, eng_gpu_flag,
        g_seq_model, g_seq_model_ch_batch);
    if (ret != 0) {
        std::cout << "chinese seqocr model initialize failure!" << std::endl;
        return -1;
    }
    std::cout << "detection and recog model initialized ok!" << std::endl;

    snprintf(g_pdebug_title, 256, "%s", "");
    if (argc >= 6) {
        snprintf(g_pdebug_title, 256, "%s", argv[5]);
        printf("Debug Title:%s\n", g_pdebug_title);
    }
    g_debug_line_index = -1;
    if (argc >= 7) {
        g_debug_line_index = atoi(argv[6]);
        printf("Debug line index:%d\n", g_debug_line_index);
    }
    
    get_img_list(g_image_dir, g_img_name_v);
    // create the output directory
    ret = nsseqocrchn::proc_output_dir(g_output_path);
    if (ret != 0) {
        fprintf(stderr, "proc output dir error.\n");
        return -1;
    }
    g_image_num = g_img_name_v.size();

    double dt1 = 0;
    double dt2 = 0;

    dt1 = cv::getTickCount();
    // Thread 0: the main thread
    dect_recog(NULL);

    if (compute_time == 1) {
        dt2 = cv::getTickCount();
        double frecog_time = (dt2 - dt1) / cv::getTickFrequency();
        double favg_time = frecog_time / g_image_num;        
        printf("CPU time %.4fs of %d imgs, average recog time: %.4f s\n", \
            frecog_time, g_image_num, favg_time);
        FILE *flog = fopen("./EvalTime.txt", "at");
        fprintf(flog, "chn_model_fast_path: %s\n", recognition_model_path);
        fprintf(flog, "TotalImageNum = %d, total-time:%.4f s average: %.4f s\n\n",\
                g_image_num, frecog_time, favg_time);
        fclose(flog);
    }
    if (NULL != g_cdnn_model){
        cdnnReleaseModel(&g_cdnn_model);
        g_cdnn_model = NULL;
    }

    return 0;
}

void *dect_recog(void *arg) {

    for (unsigned int i = 0; i < (int)g_img_name_v.size(); ++i){
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
        ret = dect_recg_image(str_file_name, g_output_path);
    }
    return NULL;
}

int dect_recg_image(std::string file_name, std::string save_dir) {
    
    std::string str_file_name = file_name;
    int jpg_pos = str_file_name.find(".jpg");
    std::cout << "image: " << file_name << std::endl;

    std::string file_title = str_file_name;
    if (jpg_pos > 0) {
        file_title = str_file_name.substr(0, jpg_pos);
    }
    snprintf(g_pimage_title, 256, "%s", file_title.data());

    std::string im_inpath = g_image_dir + "/" + file_name;
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
    g_dect_time += dect_t;

    std::vector<linegenerator::TextPolygon> polygons;
    std::vector<cv::Mat> undistorted_line_imgs;
    std::vector<cv::Mat> undistorted_masks;
    std::vector<cv::Mat> undistorted_coord_x;
    std::vector<cv::Mat> undistorted_coord_y;

    int line_num = text_line_detection_results.size();
    if (line_num <= 0) {
        std::cout << "no detection result!" << std::endl;
        return -1;
    }
    for (unsigned int i_poly = 0; i_poly < line_num; ++i_poly){
        polygons.push_back(text_line_detection_results[i_poly].get_text_polygon());
        undistorted_line_imgs.push_back(text_line_detection_results[i_poly].get_line_im());
        undistorted_masks.push_back(text_line_detection_results[i_poly].get_line_mask());
        undistorted_coord_x.push_back(text_line_detection_results[i_poly].get_coord_x());
        undistorted_coord_y.push_back(text_line_detection_results[i_poly].get_coord_y());
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

    // get the line image batch of the detection window
    dt1 = (double)cv::getTickCount();
    std::vector<st_batch_img> vec_batch_info;
    std::vector<linegenerator::TextLineDetectionResult> detection_lines;
    for (int i_line = 0; i_line < line_num; ++i_line) {
        st_batch_img batch_img;
        IplImage tmp_img = IplImage(undistorted_line_imgs[i_line]);
    
        if (tmp_img.width < SEQL_MIN_IMAGE_WIDTH \
            || tmp_img.height < SEQL_MIN_IMAGE_HEIGHT \
            || tmp_img.width > SEQL_MAX_IMAGE_WIDTH\
            || tmp_img.height > SEQL_MAX_IMAGE_HEIGHT) {
            continue ;
        }
        batch_img.img = cvCloneImage(&tmp_img);
        batch_img.bg_flag = text_line_detection_results[i_line].get_black_flag();
        vec_batch_info.push_back(batch_img);
        detection_lines.push_back(text_line_detection_results[i_line]);
    }

    //sequence learning
    int model_type = 0; 
    bool fast_flag = true;
    // 0: use chn and eng model
    // 1: use chn model only
    // 2: use eng model only
    model_type = g_ini_file->get_int_value("CHN_ENG_MODEL",\
        IniFile::_s_ini_section_global);
    if (model_type == 0) {
        std::cout << "chn and eng model are both used for output!" << std::endl;
    } else if (model_type == 1) {
        std::cout << "chn model is used only for output!" << std::endl;
    } else {
        std::cout << "eng model is used only for output!" << std::endl;
    }
        
    // g_seq_model_ch.set_recg_cand_num(10);
    g_seq_model_ch_batch.set_debug_line_index(g_debug_line_index);
    g_seq_model_ch_batch.set_mul_row_seg_para(false);
    g_seq_model_ch_batch.set_recg_cand_num(5);

    std::vector<SSeqLEngLineInfor> out_batch_vec;      
    g_seq_model_ch_batch.recognize_image_batch(vec_batch_info,
          model_type, fast_flag, out_batch_vec);
    nsseqocrchn::linegen_warp_output(detection_lines, out_batch_vec);
    double recg_t = ((double)cv::getTickCount() - dt1) / (cv::getTickFrequency());
    g_recg_time += recg_t;

    //save recognition results utf8-mode
    save_utf8_result(out_batch_vec, recog_name.c_str(), 
        const_cast<char*>(file_name.c_str()), src_img.cols, src_img.rows);

    for (int i_line = 0; i_line < vec_batch_info.size(); ++i_line) {
        cvReleaseImage(&vec_batch_info[i_line].img);
        vec_batch_info[i_line].img = NULL;
    }

    return 0;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
