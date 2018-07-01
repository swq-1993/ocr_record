/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file test-main-eng.cpp
 * @author wangyuzhuo(com@baidu.com)
 * @date 2015/05/17 14:32:26
 * @brief test eng line detection and recognition 
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
#include <sys/stat.h>        //mkdir()
#include <sys/types.h>       //mkdir()
#include <sstream>
#include "malloc.h"
#include "text_detection.h"
#include "dll_main.h"
#include "SeqLSaveRegInfor.h" 
#include "ini_file.h"
#include "seq_ocr_model_eng.h"
#include "seq_ocr_eng.h"
#include "common.h"
#include "proc_func.h"
#include "time.h"
#include "common_func.h"

std::vector<std::string> g_img_name_v;
int g_loop_num = 0;
int g_thread_num = 1;
void* g_cdnn_model = NULL;
std::string g_input_path;
std::string g_output_path;

//global variables used by eng recognition
bool g_eng_recog_fast_flag = false;
seq_ocr_eng::CSeqModel g_seq_model;
seq_ocr_eng::CSeqModelEng g_seq_model_eng;

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
        
    int thread_id = *((int *)argv);
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

            if (i % g_thread_num != thread_id)
            {
                continue;
            }

            std::string im_inpath = g_input_path + "/" + g_img_name_v[i];
            std::string im_box_name = g_output_path + "/detection_result/box_" + g_img_name_v[i];
            std::string box_name = im_box_name + ".txt";
            std::string im_poly_name = g_output_path + "/detection_result/poly_" + g_img_name_v[i];
            std::string poly_name = im_poly_name + ".txt";

            std::string recog_name = g_output_path + "/recog_result/" + g_img_name_v[i] + ".txt";
            //1. load image and run fast rcnn algorithm
            //cv::Mat src_img = cv::imread(im_inpath, CV_LOAD_IMAGE_COLOR);
            cv::Mat src_img = cv::imread(im_inpath);
            std::vector<rcnnchar::Box> detected_boxes_post;
            if (src_img.rows < 10 || src_img.cols < 10) {
                std::cerr << "The size of image is too small!" << std::endl; 
                continue;
            }
            std::cerr << "Image name: " << g_img_name_v[i] << ", width = " 
                    << src_img.cols << ", height = " << src_img.rows << std::endl;
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
            for (int i_line = 0; i_line < in_line_dect_vec.size(); ++i_line){
                std::vector<SSeqLESegDecodeSScanWindow> c_in_line_dect_vec; 
                std::vector<SSeqLEngLineInfor> c_out_line_vec;
                c_in_line_dect_vec.push_back(in_line_dect_vec[i_line]);
                IplImage tmp_img = IplImage(undistorted_line_imgs[i_line]);
                IplImage* c_img = &tmp_img;
                g_seq_model_eng.recognize_image(c_img, c_in_line_dect_vec,
                        g_eng_recog_fast_flag, c_out_line_vec);
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

            nsseqocrchn::linegen_warp_output(text_line_detection_results, 
                out_line_vec);
            //save recognition results
            CSeqLEngSaveImageForWordEst::saveResultImageLineVec(recog_name, out_line_vec);
            //save_utf8_result(out_line_vec, recog_name.c_str(), 
            //    (char*)im_inpath.data(), src_img.cols, src_img.rows);

            cv::Mat vis_img = linegenerator::visualize_boxes(src_img, detected_boxes_post);
            cv::imwrite(im_box_name, vis_img);
            cv::Mat poly_img;
            linegenerator::visualize_poly(src_img, 
                polygons, poly_img);
            cv::imwrite(im_poly_name, poly_img);
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
            std::string str_file_name = g_img_name_v[i];
            std::string filename_nopost = str_file_name.replace(
                str_file_name.end()-4, 
                str_file_name.end(), "");
            
            //save line information
            for (unsigned int i_line = 0; 
                i_line < undistorted_line_imgs.size(); ++i_line){
                std::vector<float> xcoord;
                std::vector<float> ycoord;
                for (unsigned int i_pts = 0; 
                    i_pts < polygons[i_line].get_polygon().size(); 
                    ++i_pts){
                    xcoord.push_back(polygons[i_line].get_polygon()[i_pts].x);
                    ycoord.push_back(polygons[i_line].get_polygon()[i_pts].y);
                }

                int minx = std::min(std::max((int)(*min_element(xcoord.begin(), 
                        xcoord.end())), 0), src_img.cols-1);
                int miny = std::min(std::max((int)*min_element(ycoord.begin(), 
                        ycoord.end()), 0), src_img.rows - 1);
                int maxx = std::min(std::max((int)(*max_element(xcoord.begin(), 
                        xcoord.end())), 0), src_img.cols - 1);
                int maxy = std::min(std::max((int)(*max_element(ycoord.begin(), 
                        ycoord.end())), 0), src_img.rows - 1);
                std::stringstream ss_t;
                ss_t << "_" << minx << "_" << miny << "_" << maxx << "_" << maxy << ".png";
                std::string outname_line = g_output_path + "/line_im/im_" + 
                    filename_nopost + ss_t.str();
                cv::imwrite(outname_line, undistorted_line_imgs[i_line]);
                std::string outname_mask = g_output_path + "/line_mask/mask_" + 
                    filename_nopost + ss_t.str();
                cv::imwrite(outname_mask, undistorted_masks[i_line]);
                std::string outname_coordx = g_output_path + "/coord_x/coordx_" + 
                    filename_nopost + ss_t.str();
                cv::imwrite(outname_coordx, undistorted_coord_x[i_line]);
                std::string outname_coordy = g_output_path + "/coord_y/coordy_" + 
                    filename_nopost + ss_t.str();
                cv::imwrite(outname_coordy, undistorted_coord_y[i_line]);
            }
        }
    }
    return NULL;
}

int main(int argc, char* argv[]){
    if (argc != 9){
        std::cerr << "./test-main-new detection_mode rec_model fast_model"
            << "fast_mode(0:slow 1:fast) img_dir res_dir thread_num" 
            << "loop_num" << std::endl;
        return -1;
    }
    char* detection_model_path = argv[1];
    char* recognition_model_path = argv[2];
    char* fast_recognition_model_path = argv[3];
    int recog_mode = atoi(argv[4]);
    g_input_path = std::string(argv[5]);
    g_output_path = std::string(argv[6]);
    g_thread_num = atoi(argv[7]);
    g_loop_num = atoi(argv[8]);

    if (recog_mode != 0 && recog_mode != 1) {
        std::cout << "please set the correct processing mode: 0-slow 1-fast" << std::endl;
        return -1;
    }

    if (cdnnInitModel(detection_model_path, g_cdnn_model, 1, true, true) != 0){
        fprintf(stderr, "model init error.\n");
        return -1;
    }

    char model_dir[256];
    char esp_model_conf_name_en[256];
    char inter_lm_trie_name_en[256];
    char inter_lm_target_name_en[256];
    char table_name_en[256];
    
    snprintf(model_dir, 256, "%s", "./data");
    snprintf(inter_lm_target_name_en, 256, "%s/model_eng/target_word.vocab", model_dir);
    snprintf(inter_lm_trie_name_en, 256, "%s/model_eng/lm_trie_word", model_dir);
    snprintf(esp_model_conf_name_en, 256, "%s/model_eng/sc.conf", model_dir);
    snprintf(table_name_en, 256, "%s/model_eng/table_seqL_95", model_dir);

    g_seq_model.init(recognition_model_path, fast_recognition_model_path,
                     esp_model_conf_name_en, inter_lm_trie_name_en,
                     inter_lm_target_name_en, table_name_en);
    if (recog_mode == 0) {
        //slow mode 
        //initialize with the slow model
        g_eng_recog_fast_flag = false;
        g_seq_model_eng.init_model(g_seq_model._m_eng_search_recog_eng,
            g_seq_model._m_inter_lm_eng, g_seq_model._m_recog_model_eng); 
    } else { 
        // fast mode
        // initialize with the fast model
        g_eng_recog_fast_flag = true;
        g_seq_model_eng.init_model(g_seq_model._m_eng_search_recog_eng,
            g_seq_model._m_inter_lm_eng, g_seq_model._m_recog_model_eng_fast); 
    }
    g_seq_model_eng.set_acc_eng_highlowtype_flag(false);
   // snprintf(g_pdebug_title, 256, "");
   // g_debug_line_index = -1;

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
    int ret_val = linegenerator::proc_output_dir(g_output_path);
    if (ret_val != 0) {
        fprintf(stderr, "proc output dir error.\n");
        return -1;
    }

    clock_t c1 = clock();

    //std::cerr << "thread num = " << thread_num << std::endl; 
    pthread_t *thp_core = (pthread_t *)calloc(g_thread_num, sizeof(pthread_t));
    int *id_args = (int *)calloc(g_thread_num, sizeof(int));
    for (int i = 0; i < g_thread_num; i++){
        id_args[i] = i;
        pthread_create(&(thp_core[i]), NULL, core_func, id_args + i);
    }
    for (int i = 0; i < g_thread_num; i++){
        pthread_join(thp_core[i], NULL);
    }

    free(thp_core);
    thp_core = NULL;

    if (NULL != g_cdnn_model){
        cdnnReleaseModel(&g_cdnn_model);
        g_cdnn_model = NULL;
    }
    clock_t c2 = clock();
    printf("Total recognize time: %.3f\n", (float)(c2 - c1) / CLOCKS_PER_SEC);
    free(id_args);
    id_args = NULL;
    return 0;
}


