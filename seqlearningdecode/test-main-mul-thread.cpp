/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
/**
 * @file test-main-mul-thread.cpp
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

#define COMPUT_TIME
//extern char G_thisImageName[256];
#ifdef COMPUT_TIME
    clock_t onlyDecodeClock, onlyRecogClock,onlySegmentClock, onlyRecogPredictClock;
#endif

esp::esp_engsearch * g_pEngSearchRecog;
LangModel::AbstractScoreCal * g_interLm;
CSeqLEngWordRegModel * g_recogModel;

int imageNum =0;
int totalLineNum = 0;

std::string imageDir;
std::string segInforDir;
std::string saveInforDir;
std::string postInforDir;

void *DICore(void *arg);

int main(int argc, char *argv[]) {
    if (argc != 7) {
        std::cout << "./test-main imgDir dectDir beforeInforDir \
            postInforDir threadNum modelname" << std::endl;
        return -1;
    }
  
    imageDir = argv[1];
    segInforDir   = argv[2];
    saveInforDir  = argv[3];
    postInforDir  = argv[4];

    int thread_num = 0;
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

    char esp_model_conf_name[256];
    char inter_lm_trie_name[256];
    char inter_lm_target_name[256];

    snprintf(esp_model_conf_name, 256, "data/model_eng/sc.conf");
    snprintf(inter_lm_trie_name, 256, "data/model_eng/lm_trie_word");
    snprintf(inter_lm_target_name, 256, "data/model_eng/target_word.vocab");

    g_pEngSearchRecog =  new esp::esp_engsearch;
    if (g_pEngSearchRecog->esp_init(esp_model_conf_name) < 0) {
        std::cerr << "Failed to load the spell check module!" << std::endl;
        return 0;
    }

    const LangModel::AbstractScoreCal & interWordLm = \
        LangModel::GetModel(LangModel::CalType_CompactTrie, inter_lm_trie_name, 3,
                            true, true, SEQLENGINTRAGRAMOOPRO, inter_lm_target_name);
    g_interLm = (LangModel::AbstractScoreCal *) &interWordLm;

    std::cout << "Model name...." << argv[6] << std::endl;

    g_recogModel = new CSeqLEngWordRegModel(argv[6], \
          "./data/model_eng/table_seqL_95", 127.5, 48);

#ifdef COMPUT_TIME
    clock_t ct1, ct2;
    ct1 = clock();
    onlyDecodeClock = 0;
    onlyRecogClock = 0;
    onlyRecogPredictClock = 0;
    onlySegmentClock = 0;
#endif

    pthread_t *thp_dicore = (pthread_t *)calloc(thread_num, sizeof(pthread_t));
    for (int i = 1; i < thread_num; i++) {
        pthread_create(&(thp_dicore[i]), NULL, DICore, NULL);
    }

    // Thread 0: the main thread
    DICore(NULL);

    for (int i = 1; i < thread_num; i++) {
        pthread_join(thp_dicore[i], NULL);
    }

    free(thp_dicore);
    thp_dicore = NULL;

#ifdef COMPUT_TIME
    ct2 = clock();
    printf( "CPU time %.3fs for New Decoder of %d imgs,%d lines,noly decoder time %.3fs, only recog time %.3fs,only segment time is %.3f , only predict time is %.3f \n", \
            (float)(ct2-ct1)/CLOCKS_PER_SEC,imageNum ,totalLineNum ,(float)onlyDecodeClock/CLOCKS_PER_SEC,\
            (float)onlyRecogClock/CLOCKS_PER_SEC, (float)onlySegmentClock/CLOCKS_PER_SEC,
            (float)onlyRecogPredictClock/CLOCKS_PER_SEC);
#endif

    LangModel::RemoveModel();
    g_interLm = NULL;
    if (g_pEngSearchRecog) {
        delete g_pEngSearchRecog;
        g_pEngSearchRecog = NULL;
    }
    if (g_recogModel) {
        delete g_recogModel;
        g_recogModel = NULL;
    }

    return 0;
}

void *DICore(void *arg)
{
    CSeqLEngWordSegProcessor seg_processor;
    CSeqLEngWordReg* g_recog_processer = NULL;

    CSeqLEngWordReg recogProcessor(*g_recogModel);
    g_recog_processer = &recogProcessor;

    int round_num = 30;
    //while (1)
    {
        round_num--;
        DIR* dir = NULL;
        struct dirent* ptr = NULL;  
        if ((dir = opendir(imageDir.c_str())) == NULL) {
            std::cerr << "Open dir failed. Dir: " << segInforDir << std::endl;
            return NULL;
        }
        int jump_image_num = 0;
        while (ptr = readdir(dir)) {
            if (ptr->d_name[0] == '.') {
                continue;
            }
            jump_image_num++;
            std::cout << "jump_image_num " << jump_image_num << std::endl;

            std::string read_name = segInforDir;
            read_name += "/";
            read_name += ptr->d_name;
            read_name += ".line.txt";
            std::cout << "Process " << read_name << std::endl;

            time_t ti;
            struct tm *time_ptr;
            ti = time(NULL);
            time_ptr = localtime(&ti);
            printf("\n%d:%d:%d\n", time_ptr->tm_hour, time_ptr->tm_min, time_ptr->tm_sec);

            if (seg_processor.loadFileDetectLineRects(read_name) != 0) {
                std::cerr << "Error load file " << read_name << std::endl;
                continue;
            }

            std::string image_name = imageDir;
            image_name += "/";
            image_name += ptr->d_name;
            IplImage * image = NULL;
            if ((image = cvLoadImage(image_name.c_str(), CV_LOAD_IMAGE_COLOR)) == 0) {
                std::cerr << "Error load Image " << image_name << std::endl;
                continue;
            }
            std::cout << "Funy image" << image_name << "width" << image->width << std::endl;
            imageNum++;

#ifdef COMPUT_TIME
            clock_t loct1 = clock();
#endif

            std::vector<SSeqLEngLineInfor> line_infor_vec;
            std::vector<SSeqLEngLineRect> line_rect_vec;
            if (0) {
                line_rect_vec.resize(1);
                line_rect_vec[0].isBlackWdFlag_ = 1;
                line_rect_vec[0].rectVec_.resize(1);
                line_rect_vec[0].rectVec_[0].left_ = 0;
                line_rect_vec[0].rectVec_[0].wid_ = image->width;
                line_rect_vec[0].rectVec_[0].top_ = 0;
                line_rect_vec[0].rectVec_[0].hei_ = image->height;
                line_infor_vec.resize(line_rect_vec.size());
            } else {
                seg_processor.processOneImage(image, seg_processor.getDetectLineVec());

                seg_processor.wordSegProcessGroupNearby(4);
                line_rect_vec = seg_processor.getSegLineRectVec();
                line_infor_vec.resize(line_rect_vec.size());
            }

#ifdef COMPUT_TIME
            clock_t loct2 = clock();
            onlySegmentClock += loct2 - loct1;
            loct1 = clock();
#endif
            bool quick_recog = false;
            for (unsigned int i = 0; i < line_rect_vec.size(); i++) {
                totalLineNum++;
                SSeqLEngLineInfor & temp_line = line_infor_vec[i]; 
                g_recog_processer->recognize_eng_word_whole_line(image,
                        line_rect_vec[i], temp_line, seg_processor.getSegSpaceConf(i),
                        quick_recog);
            }

#ifdef COMPUT_TIME
            loct2 = clock();
            onlyRecogClock += loct2 - loct1;
            loct1 = clock();
#endif
            std::string save_name = saveInforDir;
            save_name += "/";
            save_name += ptr->d_name;
            save_name += ".txt";

            std::cout << "save Input Lines" << std::endl;
            CSeqLEngSaveImageForWordEst::saveResultImageLineVec(save_name, line_infor_vec);

            std::vector<SSeqLEngLineInfor> out_line_vec;
            for (unsigned int i = 0; i < line_infor_vec.size(); i++) {
                std::cout << "Decode Line " << i << std::endl; 
                CSeqLEngLineDecoder * line_decoder = new CSeqLEngLineBMSSWordDecoder; 
                line_decoder->setWordLmModel(g_pEngSearchRecog, g_interLm);
                line_decoder->setSegSpaceConf(seg_processor.getSegSpaceConf(i));
                SSeqLEngLineInfor out_line = line_decoder->decodeLine(g_recog_processer,
                                                                      line_infor_vec[i]);
                delete line_decoder;
                out_line_vec.push_back(out_line);
            }

#ifdef COMPUT_TIME
            loct2 = clock();
            onlyDecodeClock += loct2 - loct1;
            loct1 = clock();
#endif
            std::string save_post_name = postInforDir;
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
