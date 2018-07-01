/***************************************************************************
 *
 * Copyright (c) 2017 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/
/**
 * @file test-v3-recg-orient.cpp
 * @author xieshufu(com@baidu.com)
 * @date 2017/03/02 16:23:13
 * @brief
 *
 **/
#include <sstream>
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
#include "fcn_detector.h"
#include "caffe_interface.h"

#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
std::vector<std::string> g_img_name_v;
FCNDetector *g_fcn_model = NULL;
std::string g_dect_dir = "";

std::string g_input_path;
std::string g_output_path;
std::string g_m_img_char_model_type;

//global variables used by recognition
std::string g_decode_post_dir;
extern char g_pimage_title[256];
char g_pdebug_title[256];
int g_debug_line_index;
ns_seq_ocr_model::CSeqModel g_seq_model;
nsseqocrchn::CSeqModelChBatch g_seq_model_ch_batch;

const int compute_time = 1;
float g_recog_clock = 0;
int g_image_num = 0;
float g_dect_time = 0;
float g_recg_time = 0;

int init_caffe_global_model();
int init_fcn_detector_general_model(const char *model_dir, FCNDetector* &dect_model);
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
    std::string box_name = im_box_name + ".txt";
    std::string im_poly_name = save_dir
        + "/detection_result/" + file_title + "_poly.jpg";
    std::string poly_name = im_poly_name + ".txt";
    std::string merge_img_path = save_dir
        + "/detection_result/" + file_title + "_poly_cor.jpg";
    std::string recog_name = save_dir
        + "/recog_result/" + file_name + ".txt";

    std::cout << file_name << std::endl;
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
    rcnn_loc._m_img_char_model_type = g_m_img_char_model_type;
    rcnn_loc.init_model_v3(g_fcn_model, g_seq_model._m_recog_model_attr);

    std::string lang_type = "CHN_ENG";
    IplImage tmp_img = src_img;
    int ret = rcnn_loc.locate(&tmp_img, lang_type, g_dect_dir);
    if (ret != 0) {
        std::cerr << "locate image text lines error!" << std::endl;
        return -1;
    }
    nsseqocrchn::loc_result_t* loc_result = rcnn_loc.get_loc_result();
    double dect_t = ((double)cv::getTickCount() - dt1) / (cv::getTickFrequency());
    g_dect_time += dect_t;

    // draw the detection result
    int debug_flag = g_ini_file->get_int_value("DRAW_RCNN_DECT_RST",
        IniFile::_s_ini_section_global);
    if (debug_flag) {
        std::vector<linegenerator::TextLineDetectionResult> text_lines;
        rcnn_loc.get_org_line_result(text_lines);

        std::vector<rcnnchar::Box> detected_boxes_post;
        rcnn_loc.get_org_char_result(detected_boxes_post);
        cv::Mat vis_img = linegenerator::visualize_boxes(src_img, detected_boxes_post);
        cv::imwrite(im_box_name, vis_img);

        std::vector<linegenerator::TextPolygon> polygons;
        for (unsigned int i_poly = 0; i_poly < text_lines.size(); ++i_poly){
            polygons.push_back(text_lines[i_poly].get_text_polygon());
        }
        cv::Mat poly_img;
        linegenerator::visualize_poly(src_img, polygons, poly_img);
        cv::imwrite(im_poly_name, poly_img);

        rcnnchar::save_detected_result(box_name, detected_boxes_post);
    }

    //3.get the line image batch of the detection window
    //
    dt1 = (double)cv::getTickCount();
    const std::vector<nsseqocrchn::loc_region_t>& regions = loc_result->regions;
    std::vector<st_batch_img> vec_batch_info;
    for (int i = 0; i < (int)regions.size(); i++)
    {
        st_batch_img batch_img;
        IplImage tmp_img = IplImage(*regions[i].img_region);
        batch_img.img = cvCloneImage(&tmp_img);
        //sunwanqi debug
        /*char adr[128]={0};
        sprintf(adr, "./line_image/input_line%d.jpg",i);
        cvSaveImage(adr,&tmp_img);*/
        batch_img.bg_flag = regions[i].b_bg_white;
        vec_batch_info.push_back(batch_img);
    }
    //std::cout<<vec_batch_info.size()<<endl;
    // 0:chn and eng model; 1:chn model only; 2: eng model only
    int model_type = 0;
    bool fast_flag = false;

    // Note: in the service setting: false;
    // for data mining: true
    //g_seq_model_ch.set_save_low_prob_flag(true);
    //
    /*设置这些参数是具体干什么的？？？*/
    g_seq_model_ch_batch.set_debug_line_index(g_debug_line_index);
    g_seq_model_ch_batch.set_mul_row_seg_para(false);
    g_seq_model_ch_batch.set_recg_cand_num(1);
    g_seq_model_ch_batch.set_formulate_output_flag(true);
    //g_seq_model_ch_batch.set_resnet_strategy_flag(true);

    std::vector<SSeqLEngLineInfor> out_batch_vec;
    g_seq_model_ch_batch.recognize_image_batch(vec_batch_info,
          model_type, fast_flag, out_batch_vec);
    // free memory
    for (int i = 0; i < vec_batch_info.size(); i++) {
        cvReleaseImage(&vec_batch_info[i].img);
        vec_batch_info[i].img = NULL;
    }
    for(unsigned int i = 0; i < out_batch_vec.size(); ++i)
          std::cout<<"out_batch_vec"<<i<<":"<<out_batch_vec[i].lineStr_<<std::endl;
    nsseqocrchn::recg_result_t recg_result;
    nsseqocrchn::CSeqOCRConvert::convert_output(out_batch_vec, recg_result, true);
    ret = rcnn_loc.post_act(&recg_result);

    if (g_m_img_char_model_type == "vgg_3scale_0.125x")
    {
        recg_result.filter_word_with_low_prob(0.44);
    }
    else if (g_m_img_char_model_type == "vgg_1scale_1x")
    {
        recg_result.filter_word_with_low_prob(0.48);
    }
    else
    {
        fprintf(stderr, "the specified model name doesn't exist!");
        return -1;
    }

    //recg_result.print_info(true);
    double recg_t = ((double)cv::getTickCount() - dt1) / (cv::getTickFrequency());
    g_recg_time += recg_t;

    //recg_result.save_to_file(recog_name.data());
    recg_result.save_to_file_eval(recog_name.data());
    //save recognition results
    //save_utf8_result(out_line_vec, recog_name.c_str(),
    //    const_cast<char*>(file_name.c_str()), src_img.cols, src_img.rows);
    std::cout << "recognize finished!" << std::endl;

    return 0;
}

void* core_func_thread(void *argv){
    mallopt(M_MMAP_MAX, 0);
    mallopt(M_TRIM_THRESHOLD, -1);

    for (int i = 0; i < g_img_name_v.size(); i++) {
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
            << " [res_path]"
            << " [char_model_type: vgg_3scale_0.125x / vgg_1scale_1x]"
            << " [dect_dir:0-without dir dect/1-with dir dect]" << std::endl;
        return -1;
    }
    char* detection_model_path = argv[1];
    char* recognition_model_path = argv[2];
    g_input_path = std::string(argv[3]);
    g_output_path = std::string(argv[4]);
    g_m_img_char_model_type = std::string(argv[5]);
    int dect_dir = atoi(argv[6]);
    g_dect_dir = "v3_dir";
    if (dect_dir == 0) {
        g_dect_dir = "v3";
    } else {
        g_dect_dir = "v3_dir";
    }

    int ret_val = 0;
    init_caffe_global_model();
    ret_val = init_fcn_detector_general_model(detection_model_path, g_fcn_model);
    if (ret_val != 0){
        std::cout << "fcn model init error." << std::endl;
        return -1;
    }

    bool chn_paddle_flag = true;
    bool chn_gpu_flag = true;
    bool eng_paddle_flag = false;
    bool eng_gpu_flag = false;
    ret_val = nsseqocrchn::init_model_paddle_deepcnn(recognition_model_path,
        chn_paddle_flag, chn_gpu_flag,
        eng_paddle_flag, eng_gpu_flag,
        g_seq_model, g_seq_model_ch_batch);
    if (ret_val != 0) {
        std::cout << "seqocr gpu model init error." << std::endl;
        return -1;
    }
    std::cout << "seq gpu model init ok!" << std::endl;

    ret_val = g_seq_model.init_ocr_attr_model();
    if (ret_val != 0) {
        std::cout << "ocr_attr model init error" << std::endl;
        return -1;
    }
    std::cout << "ocr_attr model init ok!" << std::endl;

    snprintf(g_pdebug_title, 256, "");
    g_debug_line_index = -1;
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

    core_func_thread(NULL);

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

    if (NULL != g_fcn_model){
        delete g_fcn_model;
        g_fcn_model = NULL;
    }
    return 0;
}

int init_fcn_detector_general_model(const char *model_dir, FCNDetector* &dect_model)
{
    char model_name_fcn_detector_general_proto[256];
    char model_name_fcn_detector_general_weight[256];
    char model_name_fcn_detector_extra_info[256];
    if (g_m_img_char_model_type == "vgg_3scale_0.125x")
    {
        snprintf(model_name_fcn_detector_general_proto,
            256, "%s/fcn_data/fcn_deploy.prototxt", model_dir);
        snprintf(model_name_fcn_detector_general_weight,
            256, "%s/fcn_data/fcn_weight.caffemodel", model_dir);
        snprintf(model_name_fcn_detector_extra_info,
            256, "%s/fcn_data/fcn_extra_info.prototxt", model_dir);
    }
    else if (g_m_img_char_model_type == "vgg_1scale_1x")
    {
        snprintf(model_name_fcn_detector_general_proto,
            256, "%s/fcn_data/fcn_deploy_vgg9conv.prototxt", model_dir);
        snprintf(model_name_fcn_detector_general_weight,
            256, "%s/fcn_data/fcn_weight_vgg9conv.caffemodel", model_dir);
        snprintf(model_name_fcn_detector_extra_info,
            256, "%s/fcn_data/fcn_extra_info_vgg9conv.prototxt", model_dir);
    }
    else
    {
        fprintf(stderr, "the specified model name doesn't exist!");
        return -1;
    }
    
    dect_model = new FCNDetector();

    int ret = 0;
    std::string str_model_name_fcn_detector_general_proto
                = std::string(model_name_fcn_detector_general_proto);
    std::string str_model_name_fcn_detector_general_weight
                = std::string(model_name_fcn_detector_general_weight);

    int file_descriptor = open(model_name_fcn_detector_extra_info, O_RDONLY);
    if (file_descriptor < 0)
    {
        fprintf(stderr, "can not open extra info: %s", model_name_fcn_detector_extra_info);
        return -1;
    }

    rcnnchar::DenseBoxParam dense_box_param;
    google::protobuf::io::FileInputStream file_input(file_descriptor);
    file_input.SetCloseOnDelete(true);
    if (!google::protobuf::TextFormat::Parse(&file_input, &dense_box_param))
    {
        fprintf(stderr, "Fail to parse extra info file!\n");
        return -1;
    }

    ret = dect_model->init(str_model_name_fcn_detector_general_proto,
                                        str_model_name_fcn_detector_general_weight,
                                        dense_box_param);
    if (0 != ret)
    {
        return -1;
    }

    return ret;

}
int init_caffe_global_model() {

    char* device_str = std::getenv("CAFFE_PREDICT_DEVICEID");
    if (device_str == NULL || strlen(device_str) == 0)
    {
        std::cout << "setting caffe to CPU model" << std::endl;
        return 0;
    }

    int ret = 0;
    int device_id = atoi(std::getenv("CAFFE_PREDICT_DEVICEID"));
    if (device_id < 0)
    {
        std::cout << "setting caffe to CPU model" << std::endl;
    }
    else
    {
        int argc_tmp = 1;
        char* argv_tmp[] = {""};
        vis::initcaffeglobal(argc_tmp, argv_tmp, device_id);
        std::cout << "setting caffe to GPU: " << device_id << std::endl;
    }
    return ret;
}


/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
