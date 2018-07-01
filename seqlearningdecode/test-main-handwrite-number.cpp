/**
 * test-main-mul-thread-chn.cpp
 *
 * Author: hanjunyu(hanjunyu@baidu.com)
 *         xieshufu made the modification@20150911
 *         call the api module to run the recognition result
 * Create on: 2014-06-17
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
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
static bool s_use_the_same_model = true;

std::string g_image_dir;
std::string g_row_dect_dir;
std::string g_decode_before_dir;
std::string g_decode_post_dir;

char g_pdebug_title[256];
extern char g_pimage_title[256];
extern char g_psave_dir[256];
extern int g_iline_index;
int g_debug_line_index;
void *dicore(void *arg);

void save_utf8_mul_cand_rst(std::vector<SSeqLEngLineInfor> out_line_vec,\
    const char* savename, char *imgname, int imWidth, int imHeight);

// 0: load the detection result
// 1: the image is normalized to 48 height and input for recognition
int g_dect_mode = 0;

// initialize the seq model
int init_seq_model(char *input_chn_model_path);

ns_seq_ocr_model::CSeqModel g_seq_model;
nsseqocrchn::CSeqModelCh g_seq_model_ch;

int main(int argc, char *argv[]) {
    std::cout << "Run Information: " << argv[0] << std::endl;
    if (argc > 2) {
        std::cout << "image_dir.................." << argv[1] << std::endl;
    }
    if (argc > 3) {
        std::cout << "dect_dir..................." << argv[2] << std::endl;
    }
    if (argc > 4) {
        std::cout << "result_dir................." << argv[3] << std::endl;
    }
    if (argc > 5) {
        std::cout << "chn_model_path............." << argv[4] << std::endl;
    }
    if (argc > 6) {
        std::cout << "thread_num................." << argv[5] << std::endl;
    }
    std::cout << std::endl;

    if (argc < 6) {
        std::cout << "Usage: " << argv[0] << " "
                  << "image_dir dect_dir result_dir chn_model_path thread_num"
                  << std::endl;
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
    thread_num = (thread_num > 1) ? thread_num : 1;

#ifdef SEQL_USE_PADDLE
    if (thread_num > 1) {
        std::cout << "multi-thread is not supported in PaddlePaddle, "
                  << "use one thread instead"
                  << std::endl;
        thread_num = 1;
    }
#endif /* SEQL_USE_PADDLE */

    int ret = init_seq_model(chn_model_fast_path);
    if (ret != 0) {
        std::cout << "chinese seqocr model initialize failure!" << std::endl;
        return -1;
    }
    g_dect_mode = g_ini_file->get_int_value("IMG_DECT_MODE",\
            IniFile::_s_ini_section_global);
    if (g_dect_mode != 0 && g_dect_mode != 1) {
        std::cout << "detection mode must be 0 or 1!" << std::endl;
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
    for (unsigned int i = 1; i < thread_num; i++) {
        pthread_create(&(thp_dicore[i]), NULL, dicore, NULL);
    }

    // Thread 0: the main thread
    dicore(NULL);

    for (unsigned int i = 1; i < thread_num; i++) {
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

int loadfiledetectlinerects(std::string fileName,\
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

void *dicore(void *arg) {
   
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
        
        std::string row_dect_path = g_row_dect_dir;
        row_dect_path += "/";
        row_dect_path += ptr->d_name;
        row_dect_path += ".line.txt";
        
        std::cout << "file name: " << str_file_name << std::endl;
        std::string image_name = g_image_dir;
        image_name += "/";
        image_name += ptr->d_name;
        IplImage *image = NULL;
        if ((image = cvLoadImage(image_name.c_str(), CV_LOAD_IMAGE_COLOR)) == 0) {
            std::cerr << "Error load Image " << image_name << std::endl;
            continue;
        }
        
        std::vector<SSeqLESegDecodeSScanWindow> in_line_dect_vec;
        if (g_dect_mode == 0) {
            int ret = loadfiledetectlinerects(row_dect_path, in_line_dect_vec);
            if (ret == -1) {
                std::cout << ptr->d_name << " no dect information!" << std::endl;
                continue ;
            }
        } else if (g_dect_mode == 1){
            SSeqLESegDecodeSScanWindow temp_line;
            temp_line.left = 0;
            temp_line.right = image->width - 1;
            temp_line.top  = 0;
            temp_line.bottom = image->height - 1;
            temp_line.isBlackWdFlag = 1;
            in_line_dect_vec.push_back(temp_line);
        } 
        std::vector<SSeqLEngLineInfor> out_line_vec;
                
        clock_t ct1 = 0;
        clock_t ct2 = 0;

        ct1 = clock();
        int model_type = 0; 
        bool fast_flag = false;
        
        // 0: use chn and eng model
        // 1: use chn model only
        model_type = g_ini_file->get_int_value("CHN_ENG_MODEL",\
            IniFile::_s_ini_section_global);
        if (model_type == 0) {
            std::cout << "chn and eng model are both used!" << std::endl;
        } else {
            std::cout << "chn model is used only!" << std::endl;
        }
        int ocr_model_mode = g_ini_file->get_int_value("OCR_MODEL_MODE",\
            IniFile::_s_ini_section_global);
        
        if (ocr_model_mode == 0) {
            fast_flag = false;
        } else {
            fast_flag = true;
        }
        g_seq_model_ch.set_debug_line_index(g_debug_line_index);
        g_seq_model_ch.set_mul_row_seg_para(false);
        g_seq_model_ch.set_remove_head_end_noise(false);
        g_seq_model_ch.set_recg_cand_num(10);
        g_seq_model_ch.recognize_image_hw_num(image, in_line_dect_vec,
                model_type, fast_flag, out_line_vec);
        ct2 = clock();
        g_recog_clock += ct2 - ct1;

        int im_width = image->width;
        int im_height = image->height;
        std::string save_post_name = g_decode_post_dir;
        save_post_name += "/";
        save_post_name += ptr->d_name;
        save_post_name += ".txt";

        int save_rst_mode = g_ini_file->get_int_value("SAVE_RST_MODE",
            IniFile::_s_ini_section_global);
        if (save_rst_mode == 0) {
            std::cout << "result GBK mode" << std::endl;
            save_gbk_result(out_line_vec, save_post_name.data(),\
                ptr->d_name, im_width, im_height);
        } else {
            std::cout << "result UTF8 mode" << std::endl;
            save_utf8_result(out_line_vec, save_post_name.data(),\
                ptr->d_name, im_width, im_height);
        }

        if (image) {
            cvReleaseImage(&image);
            image = NULL;
        }

        index++;
        g_image_num++;
    }
    closedir(dir); 
    //}

    return 0;
}


// initialize the seq model
int init_seq_model(char *input_chn_model_path) {
    char chn_model_path[256];
    char chn_model_fast_path[256];

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
    snprintf(chn_model_fast_path, 256, "%s", input_chn_model_path);
    
    if (!s_use_the_same_model) {
        // snprintf(chn_model_fast_path, 256, "%s/chn_data/gatedrnn_model_small.bin", model_dir);
        snprintf(chn_model_path, 256, "%s/chn_data/gatedrnn_model_small.bin", model_dir);
    } else {
        snprintf(chn_model_fast_path, 256, "%s", input_chn_model_path);
        snprintf(chn_model_path, 256, "%s", input_chn_model_path);
    }
    snprintf(inter_lm_target_name_ch, 256, "%s/chn_data/word_target.vocab", model_dir);
    snprintf(inter_lm_trie_name_ch, 256, "%s/chn_data/word_lm_trie", model_dir);
    snprintf(esp_model_conf_name_ch, 256, "%s/chn_data/sc.conf", model_dir);
    snprintf(table_name_ch, 256, "%s/chn_data/table_10784", model_dir);
    
    if (!s_use_the_same_model) {
        snprintf(model_name_en, 256, "%s/model_eng/gru_model_fast.bin", model_dir);
    } else {
        snprintf(model_name_en, 256, "%s", input_chn_model_path);
    }
    snprintf(inter_lm_target_name_en, 256, "%s/model_eng/target_word.vocab", model_dir);
    snprintf(inter_lm_trie_name_en, 256, "%s/model_eng/lm_trie_word", model_dir);
    snprintf(esp_model_conf_name_en, 256, "%s/model_eng/sc.conf", model_dir);
    snprintf(table_name_en, 256, "%s/model_eng/table_seqL_95", model_dir);

    if (!s_use_the_same_model) {
        snprintf(chn_model_wm_path, 256, "%s/wm_data/model_wm.bin", model_dir);
    } else {
        snprintf(chn_model_wm_path, 256, "%s", input_chn_model_path);
    }

    int ret = 0;
    ret = g_seq_model.init(chn_model_path, chn_model_fast_path,
        esp_model_conf_name_ch, inter_lm_trie_name_ch,
        inter_lm_target_name_ch, table_name_ch, model_name_en, esp_model_conf_name_en,
        inter_lm_trie_name_en, inter_lm_target_name_en, table_name_en,
        chn_model_wm_path);
    if (ret != 0) {
        return -1;
    }
    ret = g_seq_model_ch.init_model(g_seq_model._m_eng_search_recog_en,
            g_seq_model._m_inter_lm_en, 
            g_seq_model._m_recog_model_en, g_seq_model._m_eng_search_recog_ch,
            g_seq_model._m_inter_lm_ch, 
            g_seq_model._m_recog_model_ch_fast);
    if (ret != 0) {
        return -1;
    }
    
    // initialize the column chinese recognition model
    char col_chn_model_path[256];
    if (!s_use_the_same_model) {
        snprintf(col_chn_model_path, 256, "%s/chn_data/col_gru_model.bin", model_dir);
    } else {
        snprintf(col_chn_model_path, 256, "%s", input_chn_model_path);
    }
    std::cout << "col_chn_model_path: " << col_chn_model_path << std::endl;
    ret = g_seq_model.init_col_chn_recog_model(col_chn_model_path, table_name_ch);
    if (ret != 0) {
        return -1;
    }
    ret = g_seq_model_ch.init_col_recog_model(g_seq_model._m_recog_model_col_chn);
    if (ret != 0) {
        return -1;
    }

    // initialize the char recognition model
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

struct cand_word_info {
    int word_index;
    double reg_log_prob;
    std::string word_str; 
};
bool compare_word(cand_word_info word1, cand_word_info word2) {
    return word1.reg_log_prob > word2.reg_log_prob;
}

void save_utf8_mul_cand_rst(std::vector<SSeqLEngLineInfor> out_line_vec,\
    const char* savename, char *imgname, int imWidth, int imHeight) {
    FILE *fp_ret_file = NULL;
    fp_ret_file = fopen(savename, "wt");
    if (fp_ret_file == NULL)  {
        printf("Can not open the file %s\n", savename);
        return;
    }
    fprintf(fp_ret_file, "%s\t%d\t%d\t\n", imgname, imWidth, imHeight);
 
    int max_str_cand_num = 5;
    unsigned int out_line_vec_size = out_line_vec.size();
    for (unsigned int i = 0; i < out_line_vec.size(); i++) {
        std::vector<SSeqLEngWordInfor> &word_vec = out_line_vec[i].wordVec_;
        int word_vec_size = word_vec.size();
        std::string max_str = "";
        double max_prob = 0;
        int total_cand_num = 0;
        std::vector<cand_word_info> cand_vec;
        std::vector<std::string> max_prob_str_vec;

        for (int j = 0; j < word_vec.size(); j++) {
            max_str += word_vec[j].wordStr_;
            max_prob_str_vec.push_back(word_vec[j].wordStr_);
            max_prob += word_vec[j].regLogProb_;
            
            double recg_prob = pow(10, word_vec[j].regLogProb_);
            std::cout << "[" << j << " " << word_vec[j].wordStr_ << " " << recg_prob << "] ";

            int cand_num = word_vec[j].cand_word_vec.size();
            total_cand_num += cand_num;
            for (int cand_idx = 0; cand_idx < cand_num; cand_idx++) {
                cand_word_info cand_info;
                cand_info.word_index = j;
                cand_info.reg_log_prob = word_vec[j].cand_word_vec[cand_idx].regLogProb_;
                cand_info.word_str = word_vec[j].cand_word_vec[cand_idx].wordStr_;
                cand_vec.push_back(cand_info);
            
                double recg_prob1 = pow(10, cand_info.reg_log_prob);
                std::cout << "[" << j << " " << cand_info.word_str << " " << recg_prob1 << "] ";
            }

        }
        std::cout << std::endl;
        fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t%.4f\n",\
            out_line_vec[i].left_, out_line_vec[i].top_,\
            out_line_vec[i].wid_, out_line_vec[i].hei_, 
            max_str.data(), max_prob);
        
        printf("%d\t%d\t%d\t%d\t%s\t%.4f\n",\
            out_line_vec[i].left_, out_line_vec[i].top_,\
            out_line_vec[i].wid_, out_line_vec[i].hei_, 
            max_str.data(), max_prob);

        if (max_str_cand_num <= 1) {
            continue;
        }
        sort(cand_vec.begin(), cand_vec.end(), compare_word);
        for (int j = 0; j < cand_vec.size(); j++) {
            std::vector<std::string> temp_max_vec = max_prob_str_vec;
            cand_word_info cand_info = cand_vec[j];
            int word_index = cand_info.word_index;
            temp_max_vec[word_index] = cand_info.word_str;

            std::string line_str = "";
            for (int k = 0; k < temp_max_vec.size(); k++) {
                line_str += temp_max_vec[k];
            }
            double total_prob = max_prob - word_vec[word_index].regLogProb_ 
                + cand_info.reg_log_prob;
            fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t%.4f\n",\
                out_line_vec[i].left_, out_line_vec[i].top_,\
                out_line_vec[i].wid_, out_line_vec[i].hei_, 
                line_str.data(), total_prob);
            
            printf("%d\t%d\t%d\t%d\t%s\t%.4f\n",\
                out_line_vec[i].left_, out_line_vec[i].top_,\
                out_line_vec[i].wid_, out_line_vec[i].hei_, 
                line_str.data(), total_prob);
        }
        
    }
    fclose(fp_ret_file);

    return;
}
