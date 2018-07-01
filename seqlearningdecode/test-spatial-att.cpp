/***************************************************************************
 * 
 * Copyright (c) 2017 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file test-spatial-att.cpp
 * @author sunyipeng(com@baidu.com)
 * @date 2017/11/17 14:45:55
 * @brief 
 *  
 **/

#include <dirent.h>
#include <iostream>
#include <cv.h>
#include <highgui.h>
#include "seq_ocr_spatial_att.h"
#include "seql_asia_att_recog.h"
#include "DictCalFactory.h"
#include "time.h"
#include "dll_main.h"

int g_image_num = 0;
int g_total_line_num = 0;

std::string g_image_dir;
std::string g_out_infor_dir;
std::string g_model_dir;

seq_ocr_spatial_att::CSeqModelSpatialAtt g_model_spatial_att;

void *di_core(void *arg);

int main(int argc, char *argv[]) {
    if (argc != 5) {
        std::cout << "./test-main img_dir res_dir model_dir thread_num" << std::endl;
        return -1;
    }
  
    std::cout << "Model name...." << argv[4] << std::endl;

    g_image_dir = argv[1];
    g_out_infor_dir   = argv[2];
    g_model_dir = argv[3]; 
    
    int thread_num = 0;
    sscanf(argv[4], "%d", &thread_num);
    
    char table_name[256];
    snprintf(table_name, 256, "data/spatial_att_model/table_10784");
    
    int ret = init_para_file();
    if (ret != 0) {
        std::cerr << "could not initlize the parameter file!" << std::endl;
        return -1;
    }
    std::cout << "Param. init. success." << std::endl; 
    
    // reg model init.
    std::cout << "RegModel allocate start." << std::endl;
    CSeqLAsiaAttRegModel * p_model_spatial_att = new CSeqLAsiaAttRegModel(argv[3],
            table_name, true, false, 127.5, 120, true);
    p_model_spatial_att->set_decode_mode(true);
    std::cout << "RegModel allocate success." << std::endl;
    
    // recog. model initialization
    g_model_spatial_att.init_model_spatial_att(p_model_spatial_att);
    std::cout << "Model initializaton success." << std::endl; 

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
    
    // free reg model
    if (p_model_spatial_att != NULL) {
        delete p_model_spatial_att;
        std::cout << "RegModel free success." << std::endl;
    }
    p_model_spatial_att = NULL;

    clock_t c2 = clock();
    printf("Total recognize time: %.3f for %d images and %d spatial\n",
            (float)(c2 - c1) / CLOCKS_PER_SEC, g_image_num, g_total_line_num);

}

void *di_core(void *arg)
{
    int round_num = 1;
    
    while (1) {

        if (round_num < 1) {
            break;
        }
        round_num--;

        DIR *dir = NULL;
        struct dirent* ptr = NULL;  
        if ((dir = opendir(g_image_dir.c_str())) == NULL) {
            std::cerr << "Open dir failed. Dir: " << g_image_dir << std::endl;
            return NULL;
        }
        int jump_image_num = 0;
        while (ptr = readdir(dir)) {
            if (ptr->d_name[0] == '.') {
                continue;
            }
            jump_image_num++;
            std::cout << "jump_image_num " << jump_image_num << std::endl;
            
            //std::string read_name = g_seg_infor_dir;
            //read_name += "/";
            //read_name += ptr->d_name;
            //read_name += ".line.txt";
            //std::cout << "Process " << read_name << std::endl;

            time_t ti;
            struct tm *time_ptr;
            ti = time(NULL);
            time_ptr = localtime(&ti);
            printf("\n%d:%d:%d\n", time_ptr->tm_hour, time_ptr->tm_min,
                    time_ptr->tm_sec);

            std::string image_name = g_image_dir;
            image_name += "/";
            image_name += ptr->d_name;
            IplImage * image = NULL;
            if ((image = cvLoadImage(image_name.c_str(), CV_LOAD_IMAGE_COLOR)) == 0) {
                std::cerr << "Error load Image " << image_name << std::endl;
                continue;
            }
            std::cout << "Test image: " << image_name 
                      << "\twidth=" << image->width << " height=" << image->height << std::endl;
            g_image_num++;

            SSeqLEngLineInfor out_line_vec;
            
            //The main recog function
            g_model_spatial_att.recognize_image_spatial_att(image, out_line_vec);

            g_total_line_num += 1;

            //std::string save_post_name = g_post_infor_dir;
            //save_post_name += "/";
            //save_post_name += ptr->d_name;
            //save_post_name += ".txt";
            
            cvReleaseImage(&image);
            image = NULL;
        }

        closedir(dir); 

    }
    return 0;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
