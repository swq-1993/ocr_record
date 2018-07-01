/***************************************************************************
 * 
 * Copyright (c) 2017 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
/**
 * @file proc_func.h
 * @author xieshufu(com@baidu.com)
 * @date 2017/01/19 13:31:40
 * @brief 
 *  
 **/

#ifndef  NSSEQOCRCHN_PROC_FUNC_H
#define  NSSEQOCRCHN_PROC_FUNC_H

#include <vector>
#include "common_utils.h"
#include "basic_elements.h"
#include "fast_rcnn.h"
#include "line_generator.h"
#include "text_detection.h"
#include "seq_ocr_model.h"
#include "seq_ocr_chn.h"
#include "seq_ocr_chn_batch.h"

namespace nsseqocrchn
{
// function for text-detection
int linegen_warp_output(
    std::vector<linegenerator::TextLineDetectionResult>& text_detection_results, 
    std::vector<SSeqLEngLineInfor>& cvt_result);

void cal_un_distorted_box(const std::vector<rcnnchar::Box> detected_boxes,
    const cv::Mat coord_x, const cv::Mat coord_y,
    std::vector<rcnnchar::Box> &un_distorted_boxes);

void merge_line_image(std::vector<cv::Mat> &undistorted_line_imgs, std::string save_merge_path);

int cvt_det_rec_struct(std::vector<linegenerator::TextLineDetectionResult>& text_lines, 
        std::vector<SSeqLESegDecodeSScanWindow> &in_line_dect_vec);

// save the polygon and other information 
void save_poly_img(std::string poly_name, std::string image_name,
    std::vector<linegenerator::TextPolygon> &polygons,
    std::vector<cv::Mat> &undistorted_line_imgs,
    std::vector<cv::Mat> &undistorted_masks,
    std::vector<cv::Mat> &undistorted_coord_x,
    std::vector<cv::Mat> &undistorted_coord_y,
    cv::Mat &src_img);

// initialize the seq model
// default: deepcnn
int init_seq_model(char *input_chn_model_path,
    ns_seq_ocr_model::CSeqModel &seq_model,  
    nsseqocrchn::CSeqModelCh &seq_model_ch
    );

// initialize the seq model with paddle/deepcnn mode
int init_model_paddle_deepcnn(char *input_chn_model_path, 
    bool chn_paddle_flag, bool chn_gpu_flag,
    bool eng_paddle_flag, bool eng_gpu_flag,
    ns_seq_ocr_model::CSeqModel &seq_model,  
    nsseqocrchn::CSeqModelChBatch &seq_model_ch);

// initialize the seq model with paddle/deepcnn mode
int init_model_paddle_deepcnn_eng_gpu(char *input_chn_model_path, 
    bool chn_paddle_flag, bool chn_gpu_flag,
    bool eng_paddle_flag, bool eng_gpu_flag,
    ns_seq_ocr_model::CSeqModel &seq_model,  
    nsseqocrchn::CSeqModelChBatch &seq_model_ch,
    char* eng_model_path);

// initialize the seq model with paddle/deepcnn mode
int init_model_paddle_deepcnn(char *input_chn_model_path, 
    bool chn_paddle_flag, bool chn_gpu_flag,
    bool eng_paddle_flag, bool eng_gpu_flag,
    ns_seq_ocr_model::CSeqModel &seq_model,  
    nsseqocrchn::CSeqModelCh &seq_model_ch);
    
int proc_output_dir(std::string output_path);
};
#endif  //__PROC_FUNC_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
