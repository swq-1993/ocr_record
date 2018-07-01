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
#include "./waimai_ocr/seq_ocr_chn_wm.h"
#include "SeqLDecodeDef.h"

const int compute_time = 1;
clock_t g_recog_clock = 0;
int g_image_num = 0;

std::string g_image_dir;
std::string g_row_dect_dir;
std::string g_decode_before_dir;
std::string g_decode_post_dir;

char g_pdebug_title[256];
extern int g_iline_index;
int g_idebug_index;
extern char g_pimage_title[256];
void *DICore(void *arg);

void saveUtf8OcrResult4EvalProcess(std::vector<SSeqLEngLineInfor> out_line_vec,\
    const char* savename, char *imgname, int imWidth, int imHeight);

ns_seq_ocr_model::CSeqModel g_seq_model;
nsseqocrchn_wm::CSeqModelCh g_seq_model_ch;

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
    snprintf(chn_model_path, 256, "%s", argv[4]);
    snprintf(chn_model_fast_path, 256, "%s", argv[4]);
   
    unsigned int thread_num = 1;
    sscanf(argv[5], "%d", &thread_num);

    char model_dir[256];
    char esp_model_conf_name_ch[256];
    char inter_lm_target_name_ch[256];
    char inter_lm_trie_name_ch[256];
    char table_name_ch[256];
    char chn_model_wm_path[256];
    
    char model_name_en[256];
    char esp_model_conf_name_en[256];
    char inter_lm_trie_name_en[256];
    char inter_lm_target_name_en[256];
    char table_name_en[256];
    
    snprintf(model_dir, 256, "%s", "./data");
    snprintf(inter_lm_target_name_ch, 256, "%s/chn_data/word_target.vocab", model_dir);
    snprintf(inter_lm_trie_name_ch, 256, "%s/chn_data/word_lm_trie", model_dir);
    snprintf(esp_model_conf_name_ch, 256, "%s/chn_data/sc.conf", model_dir);
    snprintf(table_name_ch, 256, "%s/chn_data/table_9054", model_dir);
    
    snprintf(model_name_en, 256, "%s/model_eng/small_gated.bin", model_dir);
    snprintf(inter_lm_target_name_en, 256, "%s/model_eng/target_word.vocab", model_dir);
    snprintf(inter_lm_trie_name_en, 256, "%s/model_eng/lm_trie_word", model_dir);
    snprintf(esp_model_conf_name_en, 256, "%s/model_eng/sc.conf", model_dir);
    snprintf(table_name_en, 256, "%s/model_eng/table_seqL_95", model_dir);

    snprintf(chn_model_wm_path, 256, "%s/wm_data/model_wm.bin", model_dir);

    g_seq_model.init(chn_model_path, chn_model_fast_path,
        esp_model_conf_name_ch, inter_lm_trie_name_ch,
        inter_lm_target_name_ch, table_name_ch, model_name_en, esp_model_conf_name_en,
        inter_lm_trie_name_en, inter_lm_target_name_en, table_name_en, \
        chn_model_wm_path);
    g_seq_model_ch.init_model(g_seq_model._m_eng_search_recog_en,
            g_seq_model._m_inter_lm_en, 
            g_seq_model._m_recog_model_en, g_seq_model._m_eng_search_recog_ch,
            g_seq_model._m_inter_lm_ch, 
            g_seq_model._m_recog_model_ch_wm);

    snprintf(g_pdebug_title, 256, "");
    if (argc >= 7) {
        snprintf(g_pdebug_title, 256, "%s", argv[6]);
        printf("Debug Title:%s\n", g_pdebug_title);
    }
    g_idebug_index = -1;
    if (argc >= 8) {
        g_idebug_index = atoi(argv[7]);
    }
    printf("Debug line index:%d\n", g_idebug_index);

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
        fprintf(flog, "TotalImageNum = %d, total-time:%.4f s average: %.4f s\n\n",\
                g_image_num, frecog_time, favg_time);
        fclose(flog);
    }
}

int loadFileDetectLineRects(std::string fileName,\
        std::vector<ns_seqocr_seg_fun_wm::SSeqLESegDecodeSScanWindow> &in_line_dect_vec) {

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
    int block_num = 0;
    while (1)
    {

        int temp = fscanf(fd, "%d\t%d\t%d\t%d\t%d\t%d\t", &read_left, &read_right, \
                &read_top, &read_botom, &read_is_black, &block_num);
        //std::cout << "read temp " << temp << std::endl;
        if (temp < 6) {
            break;
        }
        
        // check the validity of the information
        if (read_left < 0 || read_right < 0 || read_top < 0 || read_botom < 0) {
            continue;
        }
        ns_seqocr_seg_fun_wm::SSeqLESegDecodeSScanWindow temp_line;
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

void *DICore(void *arg) {
   
    //while (1) { 
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
       
        int jpg_pos = str_file_name.find(".jpg"); 
        string str_file_title = str_file_name.substr(0, jpg_pos);
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
        
        snprintf(g_pimage_title, 256, "%s", str_file_title.data());
        std::string row_dect_path = g_row_dect_dir;
        row_dect_path += "/";
        row_dect_path += ptr->d_name;
        row_dect_path += ".line.txt";
        
        std::vector<ns_seqocr_seg_fun_wm::SSeqLESegDecodeSScanWindow> in_line_dect_vec;
        int ret = loadFileDetectLineRects(row_dect_path, in_line_dect_vec);
        if (ret == -1) {
            std::cout << ptr->d_name << " no dect information!" << std::endl;
            continue ;
        }

        std::cout << "file name: " << str_file_name << std::endl;
        std::string image_name = g_image_dir;
        image_name += "/";
        image_name += ptr->d_name;
        IplImage *image = NULL;
        if ((image = cvLoadImage(image_name.c_str(), CV_LOAD_IMAGE_COLOR)) == 0) {
            std::cerr << "Error load Image " << image_name << std::endl;
            continue;
        }

        std::vector<SSeqLEngLineInfor> out_line_vec;
        int im_width = image->width;
        int im_height = image->height;
        int resize_width = im_width;
        int resize_height = im_height;
        int max_wh = std::max(im_width, im_height);
        float fratio = 1.0;
        IplImage *resize_img = NULL;
        if (max_wh > SEQL_MAX_IMAGE_WIDTH) {
            fratio = (float)SEQL_MAX_IMAGE_WIDTH / max_wh;
            resize_width = (int)(im_width * fratio);
            resize_height = (int)(im_height * fratio);
        }
        resize_img = cvCreateImage(cvSize(resize_width, resize_height),
                image->depth, image->nChannels);
        cvResize(image, resize_img, CV_INTER_CUBIC);
        for (int i = 0; i < in_line_dect_vec.size(); i++) {
            in_line_dect_vec[i].left = (int)(in_line_dect_vec[i].left * fratio);
            in_line_dect_vec[i].right = (int)(in_line_dect_vec[i].right * fratio);
            in_line_dect_vec[i].top = (int)(in_line_dect_vec[i].top * fratio);
            in_line_dect_vec[i].bottom = (int)(in_line_dect_vec[i].bottom * fratio);
        }
                
        clock_t ct1 = 0;
        clock_t ct2 = 0;

        ct1 = clock();
        
        // only recognize the row with Chinese ocr engine
        int model_type = 1; 
        g_seq_model_ch.set_debug_line_index(g_idebug_index);
        g_seq_model_ch.recognize_image(resize_img, in_line_dect_vec, model_type, out_line_vec);
        ct2 = clock();
        g_recog_clock += ct2 - ct1;
    
        for (unsigned int i = 0; i < out_line_vec.size(); i++) {
            SSeqLEngLineInfor& out_line = out_line_vec[i];
            out_line.left_ = (int)(out_line.left_ / fratio);
            out_line.top_ = (int)(out_line.top_ / fratio);
            out_line.wid_ = (int)(out_line.wid_ / fratio);
            out_line.hei_ = (int)(out_line.hei_ / fratio);

            for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
                SSeqLEngWordInfor& word = out_line_vec[i].wordVec_[j];
                if (!word.left_add_space_) {
                    SSeqLEngWordInfor & word = out_line_vec[i].wordVec_[j];
                    word.left_ = (int)(word.left_ / fratio);
                    word.top_ = (int)(word.top_ / fratio);
                    word.hei_ = (int)(word.hei_ / fratio);
                    word.wid_ = (int)(word.wid_ / fratio);
                } else {
                    for (int k = 0; k < word.charVec_.size(); k++) {
                        SSeqLEngCharInfor &char_info = word.charVec_[k];
                        char_info.left_ = (int)(char_info.left_ / fratio);
                        char_info.top_ = (int)(char_info.top_ / fratio);
                        char_info.wid_ = (int)(char_info.wid_ / fratio);
                        char_info.hei_ = (int)(char_info.hei_ / fratio);
                    }
                }
            }
        }

        std::string save_post_name = g_decode_post_dir;
        save_post_name += "/";
        save_post_name += ptr->d_name;
        save_post_name += ".txt";
        saveUtf8OcrResult4EvalProcess(out_line_vec, save_post_name.data(),\
                ptr->d_name, im_width, im_height);

        if (image) {
            cvReleaseImage(&image);
            image = NULL;
        }
        if (resize_img) {
            cvReleaseImage(&resize_img);
            resize_img = NULL;
        }

        index++;
        g_image_num++;
    }
    closedir(dir);
    //} 

    return 0;
}

void saveUtf8OcrResult4EvalProcess(std::vector<SSeqLEngLineInfor> out_line_vec,\
    const char* savename, char *imgname, int imWidth, int imHeight)  {

    FILE *fp_ret_file = NULL;
    fp_ret_file = fopen(savename, "wt");
    if (fp_ret_file == NULL)  {
        printf("Can not open the file %s\n", savename);
        return;
    }
    fprintf(fp_ret_file, "%s\t%d\t%d\t\n", imgname, imWidth, imHeight);
 
    // choose the saving format for English row
    // 1: the same format as Chinese, each character is preserved e.g., "c h i n e s e"
    // 0: the English format and each word is preserved e.g., "chinese"
    int en_line_format = g_ini_file->get_int_value("EN_LINE_FORMAT",\
            IniFile::_s_ini_section_global);
    unsigned int out_line_vec_size = out_line_vec.size();
    
    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        printf("%d-row recognition result: [%d, %d, %d,%d] %s\n",\
                i, out_line_vec[i].left_, out_line_vec[i].top_,\
                out_line_vec[i].wid_, out_line_vec[i].hei_, out_line_vec[i].lineStr_.data());
    }

    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        std::string str_line_rst = "";
        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            str_line_rst += out_line_vec[i].wordVec_[j].wordStr_;
        }
        out_line_vec[i].lineStr_ = str_line_rst;
    }
    
    for (unsigned int i = 0; i < out_line_vec.size(); i++) {
        if (out_line_vec[i].lineStr_.length() <= 0) {
            continue ;
        }
        fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t",\
                out_line_vec[i].left_, out_line_vec[i].top_,\
                out_line_vec[i].wid_, out_line_vec[i].hei_, out_line_vec[i].lineStr_.data());

        if (en_line_format == 0) {
            for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
                SSeqLEngWordInfor word = out_line_vec[i].wordVec_[j];
                fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t",\
                        word.left_, word.top_, word.wid_, word.hei_, word.wordStr_.data());
            }
        } else if (en_line_format == 1) {
            for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
                SSeqLEngWordInfor word = out_line_vec[i].wordVec_[j];
                char *pword_str = NULL;

                //std::cout << word.wordStr_ << " " << std::endl;
                if (!word.left_add_space_) {
                    fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t",\
                            word.left_, word.top_, word.wid_, word.hei_, word.wordStr_.data());
                } else {
                    for (int k = 0; k < word.charVec_.size(); k++) {
                        SSeqLEngCharInfor char_info = word.charVec_[k];
                        fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t",
                          char_info.left_, char_info.top_,
                          char_info.wid_, char_info.hei_, char_info.charStr_);
                    }
                }
            }

        }
        fprintf(fp_ret_file, "\n");
    }
    fclose(fp_ret_file);

    return;
}


/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
