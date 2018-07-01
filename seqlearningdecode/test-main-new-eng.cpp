/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
/**
 * @file test-main.cpp
 * @author hanjunyu(com@baidu.com)
 * @date 2014/06/17 11:09:54
 * @brief 
 *  
 **/

#include <dirent.h>
#include <iostream>
#include <cv.h>
#include <highgui.h>
#include "SeqLLoadRegInfor.h"
#include "SeqLSaveRegInfor.h"
#include "SeqLEngLineDecodeHead.h"
#include "SeqLEngLineBMSSWordDecoder.h"
#include "SeqLEngWordSegProcessor.h"
#include "SeqLEngWordRecog.h"
#include "SeqLEuroWordRecog.h"
#include "SeqLEuroLineBMSSWordDecoder.h"
#include "esc_searchapi.h"
#include "DictCalFactory.h"
#include "time.h"
#include "dll_main.h"
#include "seq_ocr_model_eng.h"
#include "seq_ocr_eng.h"

int g_image_num = 0;
int g_total_line_num = 0;

std::string g_image_dir;
std::string g_seg_infor_dir;
std::string g_post_infor_dir;
bool g_eng_recog_fast_flag = false;

seq_ocr_eng::CSeqModel g_seq_model;
seq_ocr_eng::CSeqModelEng g_seq_model_eng;

void *di_core(void *arg);

int main(int argc, char *argv[]) {
    if (argc != 8) {
        std::cout << "./test-main imgDir dectDir postdecode_res_dir\
            threadNum modelname fast_modelname fast_mode(0:slow 1:fast)" << std::endl;
        return -1;
    }
  
    std::cout << "Model name...." << argv[5] << std::endl;
    std::cout << "Fast Model name...." << argv[6] << std::endl;

    g_image_dir = argv[1];
    g_seg_infor_dir   = argv[2];
    g_post_infor_dir  = argv[3];
  
    int thread_num = 0;
    sscanf(argv[4], "%d", &thread_num);
    int recog_mode = 0;
    recog_mode = atoi(argv[7]);
    if (recog_mode != 0 && recog_mode != 1) {
        std::cout << "please set the correct processing mode: 0-slow 1-fast" << std::endl;
        return -1;
    }

    char tabel_name[256];
    char esp_model_conf_name[256];
    char inter_lm_trie_name[256];
    char inter_lm_target_name[256];

    snprintf(tabel_name, 256, "data/assist_data/table_seqL_95");
    snprintf(esp_model_conf_name, 256, "data/assist_data/sc.conf");
    snprintf(inter_lm_trie_name, 256, "data/assist_data/lm_trie_word");
    snprintf(inter_lm_target_name, 256, "data/assist_data/target_word.vocab");

    g_seq_model.init(argv[5], argv[6], esp_model_conf_name, inter_lm_trie_name,
            inter_lm_target_name, tabel_name);

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

    clock_t c1 = clock();
    
    pthread_t *thp_dicore = (pthread_t *)calloc(thread_num, sizeof(pthread_t));
    for (int i = 0; i < thread_num; i++)
    {
        pthread_create(&(thp_dicore[i]), NULL, di_core, NULL);
    }
        
    for (int i = 0; i < thread_num; i++)
    {
        pthread_join(thp_dicore[i], NULL);
    }

    free(thp_dicore);
    thp_dicore = NULL;

    clock_t c2 = clock();
    printf("Total recognize time: %.3f for %d images and %d lines\n",
            (float)(c2 - c1) / CLOCKS_PER_SEC, g_image_num, g_total_line_num);

}

void *di_core(void *arg)
{
    int round_num = 1;
    CSeqLEngWordSegProcessor seg_processor;
    while (1) {

        if (round_num < 1) {
            break;
        }
        round_num--;

        DIR *dir = NULL;
        struct dirent* ptr = NULL;  
        if ((dir = opendir(g_image_dir.c_str())) == NULL) {
            std::cerr << "Open dir failed. Dir: " << g_seg_infor_dir << std::endl;
            return NULL;
        }
        int jump_image_num = 0;
        while (ptr = readdir(dir)) {
            if (ptr->d_name[0] == '.') {
                continue;
            }
            jump_image_num++;
            std::cout << "jump_image_num " << jump_image_num << std::endl;

            std::string read_name = g_seg_infor_dir;
            read_name += "/";
            read_name += ptr->d_name;
            read_name += ".line.txt";
            std::cout << "Process " << read_name << std::endl;

            time_t ti;
            struct tm *time_ptr;
            ti = time(NULL);
            time_ptr = localtime(&ti);
            printf("\n%d:%d:%d\n", time_ptr->tm_hour, time_ptr->tm_min,
                    time_ptr->tm_sec);

            if (seg_processor.loadFileDetectLineRects(read_name) != 0) {
                std::cerr << "Error load file " << read_name << std::endl;
                continue;
            }
     
     
            std::string image_name = g_image_dir;
            image_name += "/";
            image_name += ptr->d_name;
            IplImage * image = NULL;
            if ((image = cvLoadImage(image_name.c_str(), CV_LOAD_IMAGE_COLOR)) == 0) {
                std::cerr << "Error load Image " << image_name << std::endl;
                continue;
            }
            std::cout << "Funy image" << image_name << "width"
                << image->width << std::endl;
            g_image_num++;

            std::vector<SSeqLEngLineInfor> out_line_vec;
            //The main recog function
            g_seq_model_eng.recognize_image(image,
                    seg_processor.getDetectLineVec(), g_eng_recog_fast_flag, out_line_vec);

            g_total_line_num += out_line_vec.size();

            std::string save_post_name = g_post_infor_dir;
            save_post_name += "/";
            save_post_name += ptr->d_name;
            save_post_name += ".txt";
            CSeqLEngSaveImageForWordEst::saveResultImageLineVec(save_post_name, out_line_vec);

            cvReleaseImage(&image);
            image = NULL;
        }

        closedir(dir); 

    }
    return 0;
}






/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
