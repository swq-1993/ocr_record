/***************************************************************************
 * 
 * Copyright (c) 2017 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file proc_func.cpp
 * @author xieshufu(com@baidu.com)
 * @date 2017/01/19 13:32:20
 * @brief 
 *  
 **/
#include "proc_func.h"
#include <dirent.h>

namespace nsseqocrchn
{
void get_trans_coords(cv::Rect& rect, cv::Mat& coord_x, cv::Mat& coord_y, 
        cv::Point& min_coord, cv::Point& max_coord);

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

void cal_un_distorted_box(const std::vector<rcnnchar::Box> detected_boxes, 
    const cv::Mat coord_x, const cv::Mat coord_y,
    std::vector<rcnnchar::Box> &un_distorted_boxes) {

    int rows = coord_x.rows;
    int cols = coord_x.cols;
    for (unsigned int i = 0; i < detected_boxes.size(); ++i){
        int box_x0 = (int)detected_boxes[i]._coord[0];
        int box_y0 = (int)detected_boxes[i]._coord[1];
        int box_w = (int)detected_boxes[i]._coord[2];
        int box_h = (int)detected_boxes[i]._coord[3];
        int box_x1 = box_x0 + box_w - 1;
        int box_y1 = box_y0 + box_h - 1;

        float min_dist0 = rows * cols;
        int un_distorted_x0 = -1;
        int un_distorted_y0 = -1;
        float min_dist1 = rows * cols;
        int un_distorted_x1 = -1;
        int un_distorted_y1 = -1;
        float min_dist2 = rows * cols;
        int un_distorted_x2 = -1;
        int un_distorted_y2 = -1;
        float min_dist3 = rows * cols;
        int un_distorted_x3 = -1;
        int un_distorted_y3 = -1;
        for (int row_idx = 0; row_idx < rows; row_idx++) {
            for (int col_idx = 0; col_idx < cols; col_idx++) {
                int org_x = coord_x.at<unsigned short>(row_idx, col_idx);
                int org_y = coord_y.at<unsigned short>(row_idx, col_idx);
                float dist = sqrt((org_x - box_x0)*(org_x - box_x0) + 
                                (org_y - box_y0)*(org_y - box_y0));
                if (dist < min_dist0) {
                    min_dist0 = dist;
                    un_distorted_x0 = col_idx;
                    un_distorted_y0 = row_idx;	
                }
                dist = sqrt((org_x - box_x1)*(org_x - box_x1) + (org_y - box_y0)*(org_y - box_y0));
                if (dist < min_dist1) {
                    min_dist1 = dist;
                    un_distorted_x1 = col_idx;
                    un_distorted_y1 = row_idx;	
                }
                dist = sqrt((org_x - box_x0)*(org_x - box_x0) + (org_y - box_y1)*(org_y - box_y1));
                if (dist < min_dist2) {
                    min_dist2 = dist;
                    un_distorted_x2 = col_idx;
                    un_distorted_y2 = row_idx;	
                }
                dist = sqrt((org_x - box_x1)*(org_x - box_x1) + (org_y - box_y1)*(org_y - box_y1));
                if (dist < min_dist3) {
                    min_dist3 = dist;
                    un_distorted_x3 = col_idx;
                    un_distorted_y3 = row_idx;	
                }
            }
        }
        //
        int min_x0 = std::min(un_distorted_x0, un_distorted_x1);
        int min_x1 = std::min(un_distorted_x2, un_distorted_x3);
        int un_distorted_box_x0 = std::min(min_x0, min_x1);

        int min_y0 = std::min(un_distorted_y0, un_distorted_y1);
        int min_y1 = std::min(un_distorted_y2, un_distorted_y3);
        int un_distorted_box_y0 = std::min(min_y0, min_y1);

        int max_x0 = std::max(un_distorted_x0, un_distorted_x1);
        int max_x1 = std::max(un_distorted_x2, un_distorted_x3);
        int un_distorted_box_x1 = std::max(max_x0, max_x1);

        int max_y0 = std::max(un_distorted_y0, un_distorted_y1);
        int max_y1 = std::max(un_distorted_y2, un_distorted_y3);
        int un_distorted_box_y1 = std::max(max_y0, max_y1);
        rcnnchar::Box un_distorted_box;
        un_distorted_box._coord[0] = un_distorted_box_x0;
        un_distorted_box._coord[1] = un_distorted_box_y0;
        un_distorted_box._coord[2] = un_distorted_box_x1 - un_distorted_box_x0 + 1;
        un_distorted_box._coord[3] = un_distorted_box_y1 - un_distorted_box_y0 + 1;
        un_distorted_box._score = detected_boxes[i]._score;
        un_distorted_boxes.push_back(un_distorted_box);
    }

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
        temp_line.winSet.clear();

        cv::Mat coord_x = text_lines[i].get_coord_x();
        cv::Mat coord_y = text_lines[i].get_coord_y();
        std::vector<rcnnchar::Box> row_box_vec = text_lines[i].get_box_path();
        std::vector<rcnnchar::Box> un_distorted_boxes; 
        cal_un_distorted_box(row_box_vec, coord_x, coord_y, un_distorted_boxes);
        
        for (unsigned int j = 0; j < un_distorted_boxes.size(); j++) {
            SSeqLESegDetectWin box;
            if (un_distorted_boxes[j]._score < 0.05f) {
                continue;
            }
            box.left = static_cast<int>(un_distorted_boxes[j]._coord[0] + 0.5);
            box.top = static_cast<int>(un_distorted_boxes[j]._coord[1] + 0.5);
            box.right = static_cast<int>(un_distorted_boxes[j]._coord[2] + 
                           un_distorted_boxes[j]._coord[0] + 0.5);
            box.bottom = static_cast<int>(un_distorted_boxes[j]._coord[3] + 
                            un_distorted_boxes[j]._coord[1] + 0.5);
            temp_line.winSet.push_back(box);
        }
        in_line_dect_vec.push_back(temp_line);
    }
    return 0;
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

    //save line information
    /*for (unsigned int i_line = 0; 
        i_line < undistorted_line_imgs.size(); ++i_line) {
        std::vector<float> xcoord;
        std::vector<float> ycoord;
        for (unsigned int i_pts = 0; 
            i_pts < polygons[i_line].get_polygon().size(); 
                ++i_pts) {
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
    }*/
    return ;
}

// initialize the seq model
// default: deepcnn
int init_seq_model(char *input_chn_model_path,
    ns_seq_ocr_model::CSeqModel &seq_model,  
    nsseqocrchn::CSeqModelCh &seq_model_ch
    ) {
    char chn_model_path[256];
    char chn_model_fast_path[256];

    char model_dir[256];
    char esp_model_conf_name_ch[256];
    char inter_lm_target_name_ch[256];
    char inter_lm_trie_name_ch[256];
    char table_name_ch[256];
    
    char model_name_en[256];
    char esp_model_conf_name_en[256];
    char inter_lm_trie_name_en[256];
    char inter_lm_target_name_en[256];
    char table_name_en[256];
    
    snprintf(model_dir, 256, "%s", "./data");
    snprintf(chn_model_fast_path, 256, "%s", input_chn_model_path);
    snprintf(chn_model_path, 256, "%s", input_chn_model_path);
    snprintf(inter_lm_target_name_ch, 256, "%s/chn_data/word_target.vocab", model_dir);
    snprintf(inter_lm_trie_name_ch, 256, "%s/chn_data/word_lm_trie", model_dir);
    snprintf(esp_model_conf_name_ch, 256, "%s/chn_data/sc.conf", model_dir);
    snprintf(table_name_ch, 256, "%s/chn_data/table_10784", model_dir);
    
    snprintf(model_name_en, 256, "%s/model_eng/gru_model_fast.bin", model_dir);
    snprintf(inter_lm_target_name_en, 256, "%s/model_eng/target_word.vocab", model_dir);
    snprintf(inter_lm_trie_name_en, 256, "%s/model_eng/lm_trie_word", model_dir);
    snprintf(esp_model_conf_name_en, 256, "%s/model_eng/sc.conf", model_dir);
    snprintf(table_name_en, 256, "%s/model_eng/table_seqL_95", model_dir);


    int ret = 0;
    ret = seq_model.init_para();
    if (ret != 0) {
        return ret;
    }
    ret = seq_model.init_chn_recog_model(chn_model_path, chn_model_fast_path,
        inter_lm_target_name_ch,
        esp_model_conf_name_ch, inter_lm_trie_name_ch,\
        table_name_ch, false, false);
    if (ret != 0) {
        return -1;
    }
    
    ret = seq_model.init_eng_recog_model(model_name_en,\
        esp_model_conf_name_en, inter_lm_trie_name_en,\
        inter_lm_target_name_en, table_name_en, false, false); 
    if (ret != 0) {
        return -1;
    }

    ret = seq_model_ch.init_model(seq_model._m_eng_search_recog_en,
            seq_model._m_inter_lm_en, 
            seq_model._m_recog_model_en, seq_model._m_eng_search_recog_ch,
            seq_model._m_inter_lm_ch, 
            seq_model._m_recog_model_ch_fast);
    if (ret != 0) {
        return -1;
    }
    
    // initialize the column chinese recognition model
    char col_chn_model_path[256];
    snprintf(col_chn_model_path, 256, "%s/chn_data/col_gru_model.bin", model_dir);
    ret = seq_model.init_col_chn_recog_model(col_chn_model_path, table_name_ch);
    if (ret != 0) {
        return -1;
    }
    ret = seq_model_ch.init_col_recog_model(seq_model._m_recog_model_col_chn);
    if (ret != 0) {
        return -1;
    }
    return ret;
}

// initialize the seq model with paddle/deepcnn mode
int init_model_paddle_deepcnn(char *input_chn_model_path, 
    bool chn_paddle_flag, bool chn_gpu_flag,
    bool eng_paddle_flag, bool eng_gpu_flag,
    ns_seq_ocr_model::CSeqModel &seq_model,  
    nsseqocrchn::CSeqModelChBatch &seq_model_ch_batch) {
 
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
    snprintf(chn_model_path, 256, "%s", input_chn_model_path);

    snprintf(inter_lm_target_name_ch, 256, "%s/chn_data/word_target.vocab", model_dir);
    snprintf(inter_lm_trie_name_ch, 256, "%s/chn_data/word_lm_trie", model_dir);
    snprintf(esp_model_conf_name_ch, 256, "%s/chn_data/sc.conf", model_dir);
    snprintf(table_name_ch, 256, "%s/chn_data/table_10784", model_dir);
    
    snprintf(model_name_en, 256, "%s/model_eng/gru_eng_model/eng_recg_model.bin", model_dir);
    //snprintf(model_name_en, 256, "./data/model_eng/gru_eng_model");
    snprintf(inter_lm_target_name_en, 256, "%s/model_eng/target_word.vocab", model_dir);
    snprintf(inter_lm_trie_name_en, 256, "%s/model_eng/lm_trie_word", model_dir);
    snprintf(esp_model_conf_name_en, 256, "%s/model_eng/sc.conf", model_dir);
    snprintf(table_name_en, 256, "%s/model_eng/table_eng_97", model_dir);

    int ret = 0;
    ret = seq_model.init_para();
    if (ret != 0) {
        return ret;
    }
    // initialize chn model
    ret = seq_model.init_chn_recog_model(chn_model_fast_path, chn_model_fast_path,
        inter_lm_target_name_ch, esp_model_conf_name_ch, inter_lm_trie_name_ch,
        table_name_ch, chn_paddle_flag, chn_gpu_flag);
    if (ret != 0) {
        return -1;
    }

    // initialize eng model
    ret = seq_model.init_eng_recog_model(model_name_en,\
        esp_model_conf_name_en, inter_lm_trie_name_en,\
        inter_lm_target_name_en, table_name_en, \
        eng_paddle_flag, eng_gpu_flag);
    if (ret != 0) {
        return -1;
    }
    ret = seq_model_ch_batch.init_model(seq_model._m_eng_search_recog_en,
            seq_model._m_inter_lm_en, 
            seq_model._m_recog_model_en, seq_model._m_eng_search_recog_ch,
            seq_model._m_inter_lm_ch, 
            seq_model._m_recog_model_ch_fast);
    if (ret != 0) {
        return -1;
    }
    
    // initialize the column chinese recognition model
    char col_chn_model_path[256];
    snprintf(col_chn_model_path, 256, "%s/chn_data/col_gru_model.bin", model_dir);
    ret = seq_model.init_col_chn_recog_model(col_chn_model_path, table_name_ch);
    if (ret != 0) {
        return -1;
    }
    ret = seq_model_ch_batch.init_col_recog_model(seq_model._m_recog_model_col_chn);
    if (ret != 0) {
        return -1;
    }

    return ret;
}

// initialize the seq model with paddle/deepcnn mode
int init_model_paddle_deepcnn_eng_gpu(char *input_chn_model_path, 
    bool chn_paddle_flag, bool chn_gpu_flag,
    bool eng_paddle_flag, bool eng_gpu_flag,
    ns_seq_ocr_model::CSeqModel &seq_model,  
    nsseqocrchn::CSeqModelChBatch &seq_model_ch_batch,
    char* eng_model_path) {
 
    char chn_model_path[256];
    char chn_model_fast_path[256];

    char model_dir[256];
    char esp_model_conf_name_ch[256];
    char inter_lm_target_name_ch[256];
    char inter_lm_trie_name_ch[256];
    char table_name_ch[256];
    char chn_model_wm_path[256];
    
    char* model_name_en = eng_model_path;
    char esp_model_conf_name_en[256];
    char inter_lm_trie_name_en[256];
    char inter_lm_target_name_en[256];
    char table_name_en[256];
    
    snprintf(model_dir, 256, "%s", "./data");
    snprintf(chn_model_fast_path, 256, "%s", input_chn_model_path);
    snprintf(chn_model_path, 256, "%s", input_chn_model_path);

    snprintf(inter_lm_target_name_ch, 256, "%s/chn_data/word_target.vocab", model_dir);
    snprintf(inter_lm_trie_name_ch, 256, "%s/chn_data/word_lm_trie", model_dir);
    snprintf(esp_model_conf_name_ch, 256, "%s/chn_data/sc.conf", model_dir);
    snprintf(table_name_ch, 256, "%s/chn_data/table_10784", model_dir);
    
    //snprintf(model_name_en, 256, "%s/model_eng/gru_model_fast.bin", model_dir);
    snprintf(inter_lm_target_name_en, 256, "%s/model_eng/target_word.vocab", model_dir);
    snprintf(inter_lm_trie_name_en, 256, "%s/model_eng/lm_trie_word", model_dir);
    snprintf(esp_model_conf_name_en, 256, "%s/model_eng/sc.conf", model_dir);
    snprintf(table_name_en, 256, "%s/model_eng/table_eng_97", model_dir);

    int ret = 0;
    ret = seq_model.init_para();
    if (ret != 0) {
        return ret;
    }
    // initialize chn model
    ret = seq_model.init_chn_recog_model(chn_model_fast_path, chn_model_fast_path,
        inter_lm_target_name_ch, esp_model_conf_name_ch, inter_lm_trie_name_ch,
        table_name_ch, chn_paddle_flag, chn_gpu_flag);
    if (ret != 0) {
        return -1;
    }

    // initialize eng model
    ret = seq_model.init_eng_recog_model(model_name_en,\
        esp_model_conf_name_en, inter_lm_trie_name_en,\
        inter_lm_target_name_en, table_name_en, \
        eng_paddle_flag, eng_gpu_flag);
    if (ret != 0) {
        return -1;
    }
    ret = seq_model_ch_batch.init_model(seq_model._m_eng_search_recog_en,
            seq_model._m_inter_lm_en, 
            seq_model._m_recog_model_en, seq_model._m_eng_search_recog_ch,
            seq_model._m_inter_lm_ch, 
            seq_model._m_recog_model_ch_fast);
    if (ret != 0) {
        return -1;
    }
    
    // initialize the column chinese recognition model
    char col_chn_model_path[256];
    snprintf(col_chn_model_path, 256, "%s/chn_data/col_gru_model.bin", model_dir);
    ret = seq_model.init_col_chn_recog_model(col_chn_model_path, table_name_ch);
    if (ret != 0) {
        return -1;
    }
    ret = seq_model_ch_batch.init_col_recog_model(seq_model._m_recog_model_col_chn);
    if (ret != 0) {
        return -1;
    }

    return ret;
}
    
// initialize the seq model with paddle/deepcnn mode
int init_model_paddle_deepcnn(char *input_chn_model_path, 
    bool chn_paddle_flag, bool chn_gpu_flag,
    bool eng_paddle_flag, bool eng_gpu_flag,
    ns_seq_ocr_model::CSeqModel &seq_model,  
    nsseqocrchn::CSeqModelCh &seq_model_ch) {
 
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
    snprintf(chn_model_path, 256, "%s", input_chn_model_path);

    snprintf(inter_lm_target_name_ch, 256, "%s/chn_data/word_target.vocab", model_dir);
    snprintf(inter_lm_trie_name_ch, 256, "%s/chn_data/word_lm_trie", model_dir);
    snprintf(esp_model_conf_name_ch, 256, "%s/chn_data/sc.conf", model_dir);
    snprintf(table_name_ch, 256, "%s/chn_data/table_10784", model_dir);
    
    snprintf(model_name_en, 256, "%s/model_eng/gru_model_fast.bin", model_dir);
    snprintf(inter_lm_target_name_en, 256, "%s/model_eng/target_word.vocab", model_dir);
    snprintf(inter_lm_trie_name_en, 256, "%s/model_eng/lm_trie_word", model_dir);
    snprintf(esp_model_conf_name_en, 256, "%s/model_eng/sc.conf", model_dir);
    snprintf(table_name_en, 256, "%s/model_eng/table_seqL_95", model_dir);

    int ret = 0;
    ret = seq_model.init_para();
    if (ret != 0) {
        return ret;
    }
    // initialize chn model
    ret = seq_model.init_chn_recog_model(chn_model_fast_path, chn_model_fast_path,
        inter_lm_target_name_ch, esp_model_conf_name_ch, inter_lm_trie_name_ch,
        table_name_ch, chn_paddle_flag, chn_gpu_flag);
    if (ret != 0) {
        return -1;
    }

    // initialize eng model
    ret = seq_model.init_eng_recog_model(model_name_en,\
        esp_model_conf_name_en, inter_lm_trie_name_en,\
        inter_lm_target_name_en, table_name_en, \
        eng_paddle_flag, eng_gpu_flag);
    if (ret != 0) {
        return -1;
    }
    ret = seq_model_ch.init_model(seq_model._m_eng_search_recog_en,
            seq_model._m_inter_lm_en, 
            seq_model._m_recog_model_en, seq_model._m_eng_search_recog_ch,
            seq_model._m_inter_lm_ch, 
            seq_model._m_recog_model_ch_fast);
    if (ret != 0) {
        return -1;
    }
    
    // initialize the column chinese recognition model
    char col_chn_model_path[256];
    snprintf(col_chn_model_path, 256, "%s/chn_data/col_gru_model.bin", model_dir);
    ret = seq_model.init_col_chn_recog_model(col_chn_model_path, table_name_ch);
    if (ret != 0) {
        return -1;
    }
    ret = seq_model_ch.init_col_recog_model(seq_model._m_recog_model_col_chn);
    if (ret != 0) {
        return -1;
    }

    return ret;
}
int proc_output_dir(std::string output_path) {
    DIR *output_dir = NULL;
    if ((output_dir = opendir(output_path.c_str())) == NULL) {
        std::cout << "The output_dir does not exist." 
            << " The program will create the dir: " 
            << output_path.c_str() << std::endl;
        if ((mkdir(output_path.c_str(), S_IRWXU | S_IRWXG | S_IRWXO)) < 0) {
            std::cerr << "create output_dir file!" << std::endl;
            return -2;
        }
    }
    closedir(output_dir);

    std::string outname_t = output_path + "/detection_result";
    DIR* output_subdir = NULL;
    if ((output_subdir 
        = opendir(outname_t.c_str())) == NULL) {
        std::cout << "The output_subdir does not exist." 
            << " The program will create the dir: " 
            << outname_t.c_str() << std::endl;
        if ((mkdir(outname_t.c_str(), S_IRWXU 
            | S_IRWXG | S_IRWXO)) < 0) {
            std::cerr << "create output_subdir file!" 
                << std::endl;
            return -2;
        }
    }
    closedir(output_subdir);
    
    outname_t = output_path + "/recog_result";
    output_subdir = NULL;
    if ((output_subdir 
        = opendir(outname_t.c_str())) == NULL) {
        std::cout << "The output_subdir does not exist." 
            << " The program will create the dir: " 
            << outname_t.c_str() << std::endl;
        if ((mkdir(outname_t.c_str(), S_IRWXU 
            | S_IRWXG | S_IRWXO)) < 0) {
            std::cerr << "create output_subdir file!" 
                << std::endl;
            return -2;
        }
    }
    closedir(output_subdir);
    
    outname_t = output_path + "/line_im";
    output_subdir = NULL;
    if ((output_subdir 
        = opendir(outname_t.c_str())) == NULL) {
        std::cout << "The output_subdir does not exist." 
            << " The program will create the dir: " 
            << outname_t.c_str() << std::endl;
        if ((mkdir(outname_t.c_str(), S_IRWXU 
            | S_IRWXG | S_IRWXO)) < 0) {
            std::cerr << "create output_subdir file!" 
                << std::endl;
            return -2;
        }
    }
    closedir(output_subdir);

    outname_t = output_path + "/line_mask";
    output_subdir = NULL;
    if ((output_subdir = opendir(outname_t.c_str())) 
        == NULL) {
        std::cout << "The output_subdir does not exist." 
            << " The program will create the dir: " 
            << outname_t.c_str() << std::endl;
        if ((mkdir(outname_t.c_str(), S_IRWXU 
            | S_IRWXG | S_IRWXO)) < 0) {
            std::cerr << "create output_subdir file!" 
                << std::endl;
            return -2;
        }
    }
    closedir(output_subdir);

    outname_t = output_path + "/coord_x";
    output_subdir = NULL;
    if ((output_subdir 
          = opendir(outname_t.c_str())) == NULL) {
        std::cout << "The output_subdir does not exist." 
            << " The program will create the dir: " 
            << outname_t.c_str() << std::endl;
        if ((mkdir(outname_t.c_str(), S_IRWXU 
            | S_IRWXG | S_IRWXO)) < 0) {
            std::cerr << "create output_subdir file!" 
                << std::endl;
            return -2;
        }
    }
    closedir(output_subdir);

    outname_t = output_path + "/coord_y";
    output_subdir = NULL;
    if ((output_subdir 
        = opendir(outname_t.c_str())) == NULL) {
        std::cout << "The output_subdir does not exist." 
            << " The program will create the dir: " 
            << outname_t.c_str() << std::endl;
        if ((mkdir(outname_t.c_str(), S_IRWXU 
            | S_IRWXG | S_IRWXO)) < 0) {
            std::cerr << "create output_subdir file!" 
                << std::endl;
            return -2;
        }
    }
    closedir(output_subdir);

    return 0;
}

};

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
