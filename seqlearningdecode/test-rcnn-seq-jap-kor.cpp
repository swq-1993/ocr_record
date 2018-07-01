/**
 * test-rcnn-seq-jap-kor.cpp
 * Author: huhan02(huhan02@baidu.com)
 * Create on: 2015-12-15
 *
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **/

#include <dirent.h>
#include "common_utils.h"
#include "basic_elements.h"
#include "fast_rcnn.h"
#include "line_generator.h"
#include <string>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sys/stat.h>        
#include <sys/types.h>     
#include <sstream>
#include "malloc.h"
#include "text_detection.h"
#include "dll_main.h"
#include "seq_ocr_model.h"
#include "seq_ocr_chn.h"
#include "ini_file.h"
#include "common.h"
#include "common_func.h"

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
clock_t g_recog_clock = 0;
int g_image_num = 0;
// 0: chinese
// 1: Japanese
// 2: English
int g_lang_category = 0;
int linegen_warp_output(
    std::vector<linegenerator::TextLineDetectionResult>& text_detection_results, 
    std::vector<SSeqLEngLineInfor>& cvt_result);

// initialize the seq model
int init_seq_model(char *recog_model_path);
void merge_line_image(std::vector<cv::Mat> &undistorted_line_imgs, std::string save_merge_path);
void save_poly_img(std::string poly_name, std::string image_name,
    std::vector<linegenerator::TextPolygon> &polygons,
    std::vector<cv::Mat> &undistorted_line_imgs,
    std::vector<cv::Mat> &undistorted_masks,
    std::vector<cv::Mat> &undistorted_coord_x,
    std::vector<cv::Mat> &undistorted_coord_y,
    cv::Mat &src_img);

int cvt_det_rec_struct(std::vector<linegenerator::TextLineDetectionResult>& text_lines, 
        std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec)
{
    in_line_dect_vec.clear();
    for (unsigned int i = 0; i < text_lines.size(); ++i){
    
        SSeqLESegDecodeSScanWindow temp_line;
        temp_line.left = 0;
        temp_line.right = text_lines[i].get_line_im().cols - 1;
        temp_line.top  = 0;
        temp_line.bottom = text_lines[i].get_line_im().rows - 1;
        temp_line.isBlackWdFlag = text_lines[i].get_black_flag();
        in_line_dect_vec.push_back(temp_line);
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

        for (unsigned int i = 0; i < g_img_name_v.size(); ++i){
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
            int jpg_pos = str_file_name.find(".jpg");

            std::string file_title = str_file_name;
            if (jpg_pos > 0) {
                file_title = str_file_name.substr(0, jpg_pos);
            }
            snprintf(g_pimage_title, 256, "%s", file_title.data());

            std::string im_inpath = g_input_path + "/" + g_img_name_v[i];
            std::string im_box_name = g_output_path 
                + "/detection_result/" + file_title + "_box.jpg"; 
            std::string im_poly_name = g_output_path 
                + "/detection_result/" + file_title + "_poly.jpg";
            std::string poly_name = im_poly_name + ".txt";
            std::string merge_img_path = g_output_path 
                + "/detection_result/" + file_title + "_poly_cor.jpg"; 
            std::string recog_name = g_output_path 
                + "/recog_result/" + g_img_name_v[i] + ".txt";

            //1. load image and run fast rcnn algorithm
            cv::Mat src_img = cv::imread(im_inpath);
            std::vector<rcnnchar::Box> detected_boxes_post;
            if (src_img.rows < 10 || src_img.cols < 10) {
                std::cerr << "The size of image is too small!" << std::endl; 
                continue;
            }
            //2. run line generation codes
            double t = (double)cv::getTickCount();
            std::vector<linegenerator::TextLineDetectionResult> text_line_detection_results;
            linegenerator::text_detection(src_img, g_cdnn_model, 
                text_line_detection_results, detected_boxes_post, 0);
            t = ((double)cv::getTickCount() - t) 
                / (cv::getTickFrequency());
            std::cerr << "line generation time is " << t << std::endl;
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
            cvt_det_rec_struct(text_line_detection_results, in_line_dect_vec);

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
        
            // 0: use jap/kor and eng model
            // 1: use jap/kor model only
            model_type = g_ini_file->get_int_value("CHN_ENG_MODEL",\
                IniFile::_s_ini_section_global);
            std::cout << "model_type = " << model_type << std::endl;
            std::string lang_str = "";
            if (g_lang_category == 1) {
                lang_str = "Japanese";
            } else {
                lang_str = "Korean";
            }
            if (model_type == 0) {
                std::cout << lang_str << " and eng model are both used for output!" << std::endl;
            } else if (model_type == 1) {
                std::cout << lang_str << " model is used only for output!" << std::endl;
            } else {
                std::cout << "eng model is used only for output!" << std::endl;
            }
            clock_t ct1 = 0;
            clock_t ct2 = 0;

            ct1 = clock();
            for (int i_line = 0; i_line < in_line_dect_vec.size(); ++i_line){
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
                g_seq_model_ch.set_mul_row_seg_para(false);
                g_seq_model_ch.recognize_image(c_img, c_in_line_dect_vec,
                        model_type, fast_flag, c_out_line_vec);
                std::cerr << "recognize image " << g_img_name_v[i] 
                    << ", the " << i_line << "th line" << std::endl; 
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

            //linegenerator::warp_output(text_line_detection_results, 
            //    out_line_vec);
            linegen_warp_output(text_line_detection_results, 
                out_line_vec);
        
            ct2 = clock();
            g_recog_clock += ct2 - ct1;
            //save recognition results
            int save_rst_mode = g_ini_file->get_int_value("SAVE_RST_MODE",
                IniFile::_s_ini_section_global);
            if (save_rst_mode == 0) {
                save_gbk_result(out_line_vec, recog_name.c_str(), 
                    const_cast<char*>(g_img_name_v[i].c_str()), src_img.cols, src_img.rows);
            } else {
                save_utf8_result(out_line_vec, recog_name.c_str(), 
                    const_cast<char*>(g_img_name_v[i].c_str()), src_img.cols, src_img.rows);
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
                merge_line_image(undistorted_line_imgs, merge_img_path);
    
                save_poly_img(poly_name, g_img_name_v[i], polygons,
                    undistorted_line_imgs, undistorted_masks, 
                    undistorted_coord_x, undistorted_coord_y,
                    src_img);
            }

        }
    }
    return NULL;
}

int main(int argc, char* argv[]){
    if (argc < 7){
        std::cerr << "./test-main [detection_model_file] [rec_model_file] [img_path]" 
            << " [res_path] [thread_num]" 
            << " [loop_num]" << std::endl;
        return -1;
    }
    char* detection_model_path = argv[1];
    char* recognition_model_path = argv[2];
    g_input_path = std::string(argv[3]);
    g_output_path = std::string(argv[4]);
    int thread_num = atoi(argv[5]);
    g_loop_num = atoi(argv[6]);

    if (cdnnInitModel(detection_model_path, g_cdnn_model, 1, true, true) != 0){
        fprintf(stderr, "model init error.\n");
        return -1;
    }
    int ret_val = init_seq_model(argv[2]);
    if (ret_val != 0) {
        fprintf(stderr, "seqocr model init error.\n");
        return -1;
    }
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

    DIR *dir = NULL;
    struct dirent* ptr = NULL;
    if ((dir = opendir(g_input_path.c_str())) == NULL) {
        std::cerr << "Open image dir failed. Dir: " << g_input_path.c_str() << std::endl;
        return -1;
    }
    while (ptr = readdir(dir)) {
        if (ptr->d_name[0] == '.') {
            continue;
        }
        std::string str_file_name = std::string(ptr->d_name);
        if (str_file_name.length() < 5){
            std::cerr << "Wrong image file format: " << str_file_name << std::endl;
            continue;
        }
        else{
            std::string post_str = str_file_name.substr(str_file_name.length() - 4, 4);
            if (post_str.compare(".jpg") != 0 && post_str.compare(".png") != 0){
                std::cerr << "Wrong image file format: " << str_file_name << std::endl;
                continue;
            }
        }
        g_img_name_v.push_back(str_file_name);
    }
    closedir(dir);

    // create the output directory
    ret_val = linegenerator::proc_output_dir(g_output_path);
    if (ret_val != 0) {
        fprintf(stderr, "proc output dir error.\n");
        return -1;
    }
    if (compute_time == 1) {
        g_recog_clock = 0;
    }

    //std::cerr << "thread num = " << thread_num << std::endl; 
    pthread_t *thp_core = (pthread_t *)calloc(thread_num, sizeof(pthread_t));
    for (int i = 0; i < thread_num; i++){
        pthread_create(&(thp_core[i]), NULL, core_func, NULL);
    }
    for (int i = 0; i < thread_num; i++){
        pthread_join(thp_core[i], NULL);
    }

    free(thp_core);
    thp_core = NULL;
    if (compute_time == 1) {
        float frecog_time =  (float)g_recog_clock / CLOCKS_PER_SEC;
        float favg_time = frecog_time / g_image_num;
        printf("CPU time %.4fs of %d imgs, average recog time: %.4f s\n", \
            frecog_time, g_image_num, favg_time);
        FILE *flog = fopen("./EvalTime.txt", "at");
        fprintf(flog, "chn_model_path: %s\n", recognition_model_path);
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

// initialize the seq model
int init_seq_model(char *recog_model_path) {

    int ret = 0;
    char model_dir[256];
    ret = g_seq_model.init_para();
    if (ret != 0) {
        return ret;
    }
    
    g_lang_category = g_ini_file->get_int_value("RECOG_LANG", IniFile::_s_ini_section_global);
    if (g_lang_category != 1 && g_lang_category != 2) {
        std::cout << "lang_category should be 1(Japanese) or 2(Korean)!" << std::endl;
        return -1;
    }

    snprintf(model_dir, 256, "%s", "./data");
    char model_name_en[256];
    char esp_model_conf_name_en[256];
    char inter_lm_trie_name_en[256];
    char inter_lm_target_name_en[256];
    char table_name_en[256];
    snprintf(model_name_en, 256, "%s/model_eng/gru_model_fast.bin", model_dir);
    snprintf(inter_lm_target_name_en, 256, "%s/model_eng/target_word.vocab", model_dir);
    snprintf(inter_lm_trie_name_en, 256, "%s/model_eng/lm_trie_word", model_dir);
    snprintf(esp_model_conf_name_en, 256, "%s/model_eng/sc.conf", model_dir);
    snprintf(table_name_en, 256, "%s/model_eng/table_seqL_95", model_dir);

    ret = g_seq_model.init_eng_recog_model(model_name_en, esp_model_conf_name_en, \
        inter_lm_trie_name_en, inter_lm_target_name_en, table_name_en);
    if (ret != 0) {
        return ret;
    }
    if (g_lang_category == 1) {
        // initialize the japanese ocr model
        char table_name_jap[256];
        char model_name_jap[256];
        char model_name_col_jap[256];
        snprintf(table_name_jap, 256, "%s/jap_data/table_jap_8330", model_dir);
        snprintf(model_name_jap, 256, "%s", recog_model_path);
        snprintf(model_name_col_jap, 256, "%s/jap_data/col_jap_gru_model.bin", model_dir);
        ret = g_seq_model.init_jap_recog_model(model_name_jap, model_name_col_jap, table_name_jap);
        if (ret != 0) {
            return -1;
        }
        // initialize the recognition model
        ret = g_seq_model_ch.init_model(g_seq_model._m_eng_search_recog_en,
            g_seq_model._m_inter_lm_en, 
            g_seq_model._m_recog_model_en, NULL, NULL, 
            g_seq_model._m_recog_model_jap);
        if (ret != 0) {
            return ret;
        }
        // initialize the column recognition model
        ret = g_seq_model_ch.init_col_recog_model(g_seq_model._m_recog_model_col_jap);
        if (ret != 0) {
            return -1;
        }
    
    } else if (g_lang_category == 2) {
        // initialize the korean ocr model 
        char table_name_kor[256];
        char model_name_kor[256];
        snprintf(table_name_kor, 256, "%s/kor_data/table_kor_5322", model_dir);
        snprintf(model_name_kor, 256, "%s", recog_model_path);
        ret = g_seq_model.init_kor_recog_model(model_name_kor, table_name_kor);
        if (ret != 0) {
            return -1;
        }
        ret = g_seq_model_ch.init_model(g_seq_model._m_eng_search_recog_en,
            g_seq_model._m_inter_lm_en, 
            g_seq_model._m_recog_model_en, NULL, NULL, 
            g_seq_model._m_recog_model_kor);
        if (ret != 0) {
            return ret;
        }
    }

    return ret;
}


// merge the line image 
void merge_line_image(std::vector<cv::Mat> &undistorted_line_imgs, std::string save_merge_path) {

    IplImage *merge_img = NULL;
    int merge_w = 0;
    int merge_h = 0;
    int nchannels = 0;
    for (unsigned int i_line = 0; 
        i_line < undistorted_line_imgs.size(); ++i_line) {
        IplImage tmp_img = IplImage(undistorted_line_imgs[i_line]);
        IplImage* c_img = &tmp_img;
        int img_w = c_img->width;
        int img_h = c_img->height;
        if (merge_w < img_w) {
            merge_w = img_w;
        }
        merge_h += img_h;        

        if (i_line == 0) {
            nchannels = c_img->nChannels;
        }
    }
    merge_img = cvCreateImage(cvSize(merge_w, merge_h), IPL_DEPTH_8U, nchannels);
    int width_step = merge_img->widthStep;
    memset(merge_img->imageData, 255, width_step * merge_h);

    int start_y = 0;
    for (int i = 0; i < undistorted_line_imgs.size(); i++) {
        IplImage tmp_img = IplImage(undistorted_line_imgs[i]);
        IplImage* c_img = &tmp_img;
        int img_h = c_img->height;
        for (int k = 0; k < img_h; k++) {
            memcpy(merge_img->imageData + (start_y + k) * merge_img->widthStep,
                c_img->imageData + k * c_img->widthStep, c_img->widthStep);
        }
        start_y += img_h;
    }
    cvSaveImage(save_merge_path.data(), merge_img);
    cvReleaseImage(&merge_img);

    return ;
}
// save the polygon and other information 
void save_poly_img(std::string poly_name, std::string image_name,
    std::vector<linegenerator::TextPolygon> &polygons,
    std::vector<cv::Mat> &undistorted_line_imgs,
    std::vector<cv::Mat> &undistorted_masks,
    std::vector<cv::Mat> &undistorted_coord_x,
    std::vector<cv::Mat> &undistorted_coord_y,
    cv::Mat &src_img) {
   
    std::ofstream ofile;
    ofile.open(poly_name.c_str());
    for (unsigned int j = 0; j < polygons.size(); ++j){
        for (unsigned int k = 0; k < 
            polygons[j].get_polygon().size(); ++k){
            ofile << polygons[j].get_polygon()[k].x 
            << " " << polygons[j].get_polygon()[k].y 
            << " ";
        }
        ofile << std::endl;
    }
    ofile.close();
    std::string str_file_name = image_name;
    std::string filename_nopost = str_file_name.replace(str_file_name.end()-4, 
        str_file_name.end(), "");

    return ;
}

void get_trans_coords(cv::Rect& rect, cv::Mat& coord_x, cv::Mat& coord_y, 
        cv::Point& min_coord, cv::Point& max_coord)
{
    int width = coord_x.cols;
    int height = coord_x.rows;
    rect.x = std::min(std::max(rect.x, 0), width - 1);
    rect.width = std::max(std::min(rect.width, width - 1), 0);
    rect.y = std::min(std::max(rect.y, 0), height - 1);
    rect.height = std::max(std::min(rect.height, height - 1), 0);
    std::vector<int> xs;
    std::vector<int> ys;
    for (int i = rect.x; i <= rect.width; ++i){
        xs.push_back(coord_x.at<unsigned short>(rect.y, i));
        xs.push_back(coord_x.at<unsigned short>(rect.height, i));
        ys.push_back(coord_y.at<unsigned short>(rect.y, i));
        ys.push_back(coord_y.at<unsigned short>(rect.height, i));
    }
    for (int i = rect.y + 1; i <= rect.height - 1; ++i){
        xs.push_back(coord_x.at<unsigned short>(i, rect.x));
        xs.push_back(coord_x.at<unsigned short>(i, rect.width));
        ys.push_back(coord_y.at<unsigned short>(i, rect.x));
        ys.push_back(coord_y.at<unsigned short>(i, rect.width));
    }
    min_coord.x = *std::min_element(xs.begin(), xs.end());
    min_coord.y = *std::min_element(ys.begin(), ys.end());
    max_coord.x = *std::max_element(xs.begin(), xs.end());
    max_coord.y = *std::max_element(ys.begin(), ys.end());
}

int linegen_warp_output(
    std::vector<linegenerator::TextLineDetectionResult>& text_detection_results, 
    std::vector<SSeqLEngLineInfor>& cvt_result)
{
    for (unsigned int i_line = 0; i_line < text_detection_results.size(); ++i_line){
        cv::Point min_coord;
        cv::Point max_coord;
        cv::Mat line_im = text_detection_results[i_line].get_line_im();
        cv::Mat line_mask = text_detection_results[i_line].get_line_mask();
        cv::Mat coord_x = text_detection_results[i_line].get_coord_x();
        cv::Mat coord_y = text_detection_results[i_line].get_coord_y();
        cv::Rect rect;
        rect.x = 0;
        rect.y = 0;
        rect.width = line_im.cols - 1;
        rect.height = line_im.rows - 1;
        get_trans_coords(rect, coord_x, coord_y, min_coord, max_coord);
        for (unsigned int k = 0; k < cvt_result[i_line].wordVec_.size(); ++k){
            SSeqLEngWordInfor word = cvt_result[i_line].wordVec_[k];
            cv::Rect tmp_rect;
            cv::Point char_min_coord;
            cv::Point char_max_coord;
            tmp_rect.x = rect.x + cvt_result[i_line].wordVec_[k].left_;
            tmp_rect.y = rect.y + cvt_result[i_line].wordVec_[k].top_;
            tmp_rect.width = tmp_rect.x + cvt_result[i_line].wordVec_[k].wid_ - 1;
            tmp_rect.height = tmp_rect.y + cvt_result[i_line].wordVec_[k].hei_ - 1;
            get_trans_coords(tmp_rect, coord_x, coord_y, char_min_coord, char_max_coord);
            cvt_result[i_line].wordVec_[k].left_ = char_min_coord.x;
            cvt_result[i_line].wordVec_[k].top_ = char_min_coord.y;
            cvt_result[i_line].wordVec_[k].wid_ = char_max_coord.x - char_min_coord.x + 1;
            cvt_result[i_line].wordVec_[k].hei_ = char_max_coord.y - char_min_coord.y + 1;
            if (word.left_add_space_) {
                for (unsigned int j = 0; j < word.charVec_.size(); ++j){
                    SSeqLEngCharInfor char_info = word.charVec_[j];
                    cv::Rect tmp_rect_char;
                    cv::Point tmp_char_min_pts;
                    cv::Point tmp_char_max_pts;
                    tmp_rect_char.x = rect.x + char_info.left_;
                    tmp_rect_char.y = rect.y + char_info.top_;
                    tmp_rect_char.width = tmp_rect_char.x + char_info.wid_ - 1;
                    tmp_rect_char.height = tmp_rect_char.y + char_info.hei_ - 1;
                    get_trans_coords(tmp_rect_char, coord_x, coord_y, 
                            tmp_char_min_pts, tmp_char_max_pts);
                    cvt_result[i_line].wordVec_[k].charVec_[j].left_ = tmp_char_min_pts.x;
                    cvt_result[i_line].wordVec_[k].charVec_[j].top_ = tmp_char_min_pts.y;
                    cvt_result[i_line].wordVec_[k].charVec_[j].wid_ 
                        = tmp_char_max_pts.x - tmp_char_min_pts.x + 1;
                    cvt_result[i_line].wordVec_[k].charVec_[j].hei_ 
                        = tmp_char_max_pts.y - tmp_char_min_pts.y + 1;
                }
            }
            // for candidate word
            for (int j = 0; j < word.cand_word_vec.size(); j++) {
                cvt_result[i_line].wordVec_[k].cand_word_vec[j].left_ 
                    = cvt_result[i_line].wordVec_[k].left_;
                cvt_result[i_line].wordVec_[k].cand_word_vec[j].top_
                    = cvt_result[i_line].wordVec_[k].top_;
                cvt_result[i_line].wordVec_[k].cand_word_vec[j].wid_ 
                    = cvt_result[i_line].wordVec_[k].wid_;
                cvt_result[i_line].wordVec_[k].cand_word_vec[j].hei_ 
                    = cvt_result[i_line].wordVec_[k].hei_;
            }
        }
        cvt_result[i_line].left_ = min_coord.x;
        cvt_result[i_line].top_ = min_coord.y;
        cvt_result[i_line].wid_ = max_coord.x - min_coord.x + 1;
        cvt_result[i_line].hei_ = max_coord.y - min_coord.y + 1;
    }
    return 0;
}

