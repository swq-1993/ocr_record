/***************************************************************************
 * 
 * Copyright (c) 2016 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
/**
 * @file seql_asia_word_seg_processor.cpp
 * @author xieshufu(com@baidu.com)
 * @date 2016/07/04 15:24:50
 * @brief 
 *  
 **/
#include "seql_asia_word_seg_processor.h"
#include <iostream>
#include "SeqLDecodeDef.h"
#include "mul_row_segment.h"
#include "dll_main.h"

extern char g_pimage_title[256];
extern char g_psave_dir[256];
extern int g_iline_index;

int CSeqLAsiaWordSegProcessor::set_file_detect_line_rects(\
        const std::vector<SSeqLESegDecodeSScanWindow> &lineDectVec) {
    
    _m_in_line_dect_vec.clear();
    _m_in_seg_line_rect_vec.clear();
    _m_word_line_rect_vec.clear();
    _m_in_line_dect_vec = lineDectVec;

    return 0;
}

// interface for different modules
// it needs to set the suitable parameter values
// otherwise, the default parameter is used
void CSeqLAsiaWordSegProcessor::set_exd_parameter(bool extend_flag) {
    
    _m_extend_flag = extend_flag;
    return ;
}

void CSeqLAsiaWordSegProcessor::set_row_seg_parameter(bool row_segment_flag) {
    _m_row_segment_flag = row_segment_flag;
    return ;
}

int CSeqLAsiaWordSegProcessor::gen_dect_line_rect(int iWidth, int iHeight){
   
    SSeqLESegDecodeSScanWindow temp_line;
    temp_line.left = 0;
    temp_line.right = iWidth-1;
    temp_line.top  = 0;
    temp_line.bottom = iHeight-1;
    temp_line.detectConf = 0;
    temp_line.isBlackWdFlag = 0;
    _m_in_line_dect_vec.push_back(temp_line);
	
    return 0; 
}

void  CSeqLAsiaWordSegProcessor::cal_row_extend_range(\
        std::vector<SeqLESegDecodeRect> &vecLines, int iImageH) {
    int vec_lines_size = vecLines.size();    
    for (int i = 0; i < vec_lines_size; i++) {
        SeqLESegDecodeRect &prect0 = vecLines[i];
        prect0.extendTop = 0;
        prect0.extendBot = iImageH-1;
        int x00 = prect0.left;
        int x01 = prect0.right;
        int y00 = prect0.top;
        int y01 = prect0.bottom;
        int w00 = x01 - x00 + 1;
        int h00 = y01 - y00 + 1;

        bool btop_overlap = false;
        bool bbot_overlap = false;
        for (int j = 0; j < vec_lines_size; j++) {
            if (i == j) {
                continue;
            }
            SeqLESegDecodeRect &prect1 = vecLines[j];
            int x10 = prect1.left;
            int x11 = prect1.right;
            int y10 = prect1.top;
            int y11 = prect1.bottom;
            int w10 = x11 - x10 + 1;
            int h10 = y11 - y10 + 1;
            int xlen = std::max(x01, x11) - std::min(x00, x10) + 1;
            if (xlen > w00 + w10) {
                continue;
            }
            // check top side
            int ylen = std::max(y01, y11) - std::min(y00, y10) + 1;
            if (ylen > h00 + h10) {
                continue;
            }
            if (y10 <= y00) {
                btop_overlap = true;
            }
            // check bottom side
            if (y01 <= y11) {
                bbot_overlap = true;
            }
            if (btop_overlap && bbot_overlap) {
                break;
            }
        }
        if (btop_overlap) {
            prect0.extendTop = prect0.top;
        }
        if (bbot_overlap) {
            prect0.extendBot = prect0.bottom;
        }

    }

    return ;
}

// divide the dectection results into rows and columns
int CSeqLAsiaWordSegProcessor::proc_dect_rows(
    const std::vector<SSeqLESegDecodeSScanWindow> &indectVec,
    std::vector<SeqLESegDecodeRect> &vec_row_lines,
    IplImage *gray_img) {

    int ret = 0;
    int gw = gray_img->width;
    int gh = gray_img->height;
    // xieshufu add
    const double vertical_aspect_ratio = 2.0;
    // call the MultipleRowSegmentation module
    int idect_line_num = indectVec.size();
    for (unsigned int iline_idx = 0; iline_idx < idect_line_num; iline_idx++) {
        bool is_hor_flag = true;
        SeqLESegDecodeRect temp_line;
        temp_line.left   = std::max(indectVec[iline_idx].left, 0);
        temp_line.right  = std::min(indectVec[iline_idx].right, gw - 1);
        temp_line.top    = std::max(indectVec[iline_idx].top, 0);
        temp_line.bottom = std::min(indectVec[iline_idx].bottom, gh - 1);
        temp_line.isBlackWdFlag = indectVec[iline_idx].isBlackWdFlag;
        
        int line_height = temp_line.bottom - temp_line.top + 1;
        int line_width = temp_line.right - temp_line.left + 1;
        double vertical_height_thresh =  vertical_aspect_ratio*line_width;

        // do not support the image whose width and height are too small
        if (line_height<SEQL_MIN_IMAGE_HEIGHT || line_width<SEQL_MIN_IMAGE_WIDTH) {
            is_hor_flag = true;
            std::cerr << "Is too thin Line, forbid chinese segment" << std::endl;
            continue;
        }
        // classify whether the input line image belongs to vertical column
        if (line_height > vertical_height_thresh) { 
            std::cerr << "Is vertical Line, forbid chinese recognize" << std::endl;
            continue ;
        }

        std::vector<SeqLESegDecodeRect> vec_rect;
        if (_m_row_segment_flag) {
            // call the row segment function
            segment_mulrow_data(gray_img, &temp_line, vec_rect);
            int irow_num = vec_rect.size();
            if (irow_num <= 1) {
                // it is one single row
                vec_row_lines.push_back(temp_line);
            }
            else {
                for (unsigned int i = 0; i < irow_num; i++) {
                    vec_row_lines.push_back(vec_rect[i]);
                }
            }
        } else {
            // do not call the row segment function
            vec_row_lines.push_back(temp_line);
        }
    }

    return ret;
}

void CSeqLAsiaWordSegProcessor::draw_cc_rst(const IplImage *pOrgColorImg,
    std::vector<SeqLESegDecodeRect> &vec_row_lines) {

    char file_path[256];
    snprintf(file_path, 256, "%s/%s-rowrct-1.jpg",\
        g_psave_dir, g_pimage_title);
    IplImage *ptmp_image = NULL;
    ptmp_image = cvCloneImage(pOrgColorImg);
    for (unsigned int i = 0; i < _m_in_seg_line_rect_vec.size(); i++) {
    
        std::vector<SSeqLEngRect>& rect_vec_ = _m_in_seg_line_rect_vec[i].rectVec_;
        int rect_num = rect_vec_.size();
        for (unsigned int irct_idx = 0; irct_idx < rect_num; irct_idx++) {
            int left =  rect_vec_[irct_idx].left_;
            int top =  rect_vec_[irct_idx].top_;
            int right = rect_vec_[irct_idx].left_ +  rect_vec_[irct_idx].wid_;
            int bottom =  rect_vec_[irct_idx].top_ +  rect_vec_[irct_idx].hei_;
            cvRectangle(ptmp_image, cvPoint(left, top),\
                cvPoint(right, bottom), cvScalar(0, 255, 0), 1);
        }
    }
    for (unsigned int linei = 0; linei < vec_row_lines.size(); linei++) {
        SeqLESegDecodeRect temp_line = vec_row_lines[linei];
        cvRectangle(ptmp_image, cvPoint(temp_line.left, temp_line.top),\
            cvPoint(temp_line.right, temp_line.bottom), \
            cvScalar(255, 0, 0), 1);
    }
    cvSaveImage(file_path, ptmp_image);
    cvReleaseImage(&ptmp_image);
    return ;
}

// segment in the horizontal row / vertical column
// hor_flag:true the horizontal row
// hor_flag:false the vertical column
int CSeqLAsiaWordSegProcessor::segment_detect_lines(unsigned char *gim, int gw, int gh,
    std::vector<SeqLESegDecodeRect> &vec_lines, 
    bool hor_flag, std::vector<SSeqLEngLineRect> &vec_seg_line_rect) {
    
    int ret = 0;
    for (unsigned int linei = 0; linei < vec_lines.size(); linei++) {
        SeqLESegDecodeRect temp_line = vec_lines[linei];
        g_iline_index = linei;

        int line_width = temp_line.right - temp_line.left + 1;
        int line_height = temp_line.bottom - temp_line.top + 1;
        
        if (line_height < SEQL_MIN_IMAGE_HEIGHT\
                || line_width < SEQL_MIN_IMAGE_WIDTH) {
            std::cerr << "Is too thin Line, forbid chinese recognize" << std::endl;
            continue;
        }

        SSeqLESegRect *rects = NULL;
        int rect_num  = 0;
        int min_top = temp_line.extendTop;
        int max_bottom = temp_line.extendBot;
        
        seg_char_one_row(gim, gw, gh, min_top, max_bottom,\
            &temp_line, hor_flag, rects, rect_num);

        SSeqLEngLineRect temp_seg_line;
        temp_seg_line.isBlackWdFlag_ = temp_line.isBlackWdFlag;
        temp_seg_line.left_ = temp_line.left;
        temp_seg_line.top_ = temp_line.top;
        temp_seg_line.wid_ = temp_line.right - temp_line.left + 1;
        temp_seg_line.hei_ = temp_line.bottom - temp_line.top + 1;
        if (rect_num > 0) {        
            temp_seg_line.rectVec_.resize(rect_num);
            for (unsigned int i = 0; i < rect_num; i++) {
                temp_seg_line.rectVec_[i].left_ = rects[i].left + temp_line.left;
                temp_seg_line.rectVec_[i].wid_ = (rects[i].right - rects[i].left + 1);
                temp_seg_line.rectVec_[i].top_ = rects[i].top + temp_line.top;
                temp_seg_line.rectVec_[i].hei_ = (rects[i].bottom - rects[i].top + 1);
            }
        } else {
            temp_seg_line.rectVec_.resize(1);
            temp_seg_line.rectVec_[0].left_ = temp_line.left;
            temp_seg_line.rectVec_[0].wid_ = temp_line.right - temp_line.left + 1;
            temp_seg_line.rectVec_[0].top_ = temp_line.top;
            temp_seg_line.rectVec_[0].hei_ = temp_line.bottom - temp_line.top + 1;
        }
        vec_seg_line_rect.push_back(temp_seg_line);

        if (rects) {
            delete []rects;
            rects = NULL;
        }
    }
    return ret;
}
void CSeqLAsiaWordSegProcessor::process_mulrow(const IplImage *pcolor_img,\
        std::vector<SSeqLESegDecodeSScanWindow> &indectVec)  {

    _m_in_seg_line_rect_vec.clear();
    if (pcolor_img->width<SEQL_MIN_IMAGE_WIDTH \
            || pcolor_img->height<SEQL_MIN_IMAGE_HEIGHT \
            || pcolor_img->width>SEQL_MAX_IMAGE_WIDTH\
            || pcolor_img->height>SEQL_MAX_IMAGE_HEIGHT) {
        return;
    }
   
    _m_pinput_img = (IplImage*)pcolor_img; 

    int gw = pcolor_img->width;
    int gh = pcolor_img->height;
    int nchannels = pcolor_img->nChannels;
    IplImage *gray_img = cvCreateImage(cvSize(gw, gh), pcolor_img->depth, 1);
    if (nchannels != 1) {
        cvCvtColor(pcolor_img, gray_img, CV_BGR2GRAY);
    } else {
        cvCopy(pcolor_img, gray_img, NULL);
    }

    unsigned char *gim = NULL;
    gim = new unsigned char[gw * gh];
    for (int i = 0; i < gh; i++) {
        memcpy(gim + i * gw,\
                gray_img->imageData + i * gray_img->widthStep,\
                sizeof(unsigned char) * gw);
    }

    // call the MultipleRowSegmentation module
    std::vector<SeqLESegDecodeRect> vec_row_lines;
    proc_dect_rows(indectVec, vec_row_lines, gray_img);

    // compute the extend range for the row
    cal_row_extend_range(vec_row_lines, gh);

    // process the horizontal row by row
    std::vector<SSeqLEngLineRect> vec_seg_row_rect;
    segment_detect_lines(gim, gw, gh, vec_row_lines, true, vec_seg_row_rect);
    for (int i = 0; i < vec_seg_row_rect.size(); i++) {
        _m_in_seg_line_rect_vec.push_back(vec_seg_row_rect[i]);
    }

    if (gray_img) {
        cvReleaseImage(&gray_img);
        gray_img = NULL;
    }
    if (gim) {
        delete []gim;
        gim = NULL;
    }

    int idraw_rect_flag = g_ini_file->get_int_value("DRAW_ALL_CC_RST",\
            IniFile::_s_ini_section_global);
    if (idraw_rect_flag) {
        draw_cc_rst(pcolor_img, vec_row_lines);
    }

    return ;
}

bool left_seql_rect_first(const SSeqLEngRect &r1, const SSeqLEngRect &r2) {
    return r1.left_ < r2.left_;
}

void CSeqLAsiaWordSegProcessor::combine_rect_set_ada(const SSeqLEngLineRect &tempOrgLine,\
         SSeqLEngLineRect &groupTempLine)  {

    // group the neighbor rectangles in the row
    int iexd_vertical_size = 1;
    //int image_w = _m_pinput_img->width;
    int image_h = _m_pinput_img->height;
    int row_left = tempOrgLine.left_;
    int row_right = tempOrgLine.left_ + tempOrgLine.wid_ - 1;

    const std::vector<SSeqLEngRect> &prect_vec = tempOrgLine.rectVec_;
    int irow_rect_vec_size = prect_vec.size();
    const double dratio = 0.15;
    const double dratio1 = 0.5;
    int istart = 0;
    
    while (1) {
        int imerge_rct_top = prect_vec[istart].top_;
        int imerge_rct_bot = prect_vec[istart].top_ + prect_vec[istart].hei_ - 1;
        int imerge_rct_top0 = imerge_rct_top;
        int imerge_rct_bot0 = imerge_rct_bot;
        int imerge_rct_h0 = prect_vec[istart].hei_;
        int iend = -1;

        std::vector<int> win_idx_vec;
        win_idx_vec.push_back(istart);
        for (int i = istart + 1; i < irow_rect_vec_size; i++) {
            int irct_top = prect_vec[i].top_;
            int irct_bot = irct_top + prect_vec[i].hei_ - 1;
            imerge_rct_top = std::min(imerge_rct_top, irct_top);
            imerge_rct_bot = std::max(imerge_rct_bot, irct_bot);
            int imerge_rct_h = imerge_rct_bot - imerge_rct_top + 1;
            int imerge_rct_w = prect_vec[i].left_ - prect_vec[istart].left_ + prect_vec[i].wid_ + 1;
            double daspect_ratio = (double)imerge_rct_w / imerge_rct_h;

            double dmax_dist = imerge_rct_h * dratio;
            double dnb_dif_thresh = imerge_rct_h * dratio1;
            int itop_dist = irct_top - imerge_rct_top + 1;
            int ibot_dist = imerge_rct_bot - irct_bot + 1;
            int ih_dif = abs(imerge_rct_h - imerge_rct_h0);
            int inb_cc_dist = prect_vec[i].left_ - 
                (prect_vec[i - 1].left_ + prect_vec[i - 1].wid_ - 1) + 1;
            if (daspect_ratio >= 2.0) {
                if (itop_dist >= dmax_dist || ibot_dist >= dmax_dist\
                        || ih_dif >= dmax_dist || inb_cc_dist >= dnb_dif_thresh) {
                    iend = i;
                    break ;
                }
            }
            
            win_idx_vec.push_back(i);
            imerge_rct_top0 = imerge_rct_top;
            imerge_rct_bot0 = imerge_rct_bot;
            imerge_rct_h0 = imerge_rct_h;
        }

        imerge_rct_top = std::max(imerge_rct_top - iexd_vertical_size, 0);
        imerge_rct_bot = std::min(imerge_rct_bot + iexd_vertical_size, image_h - 1);
       
        bool bfinished = false; 
        SSeqLEngRect merge_rect;
        merge_rect.score_ = 0;
        merge_rect.bePredictFlag_ = true;
        
        if (istart == 0) {
            merge_rect.left_ = row_left;
        } else  {
            // in this mode, there is one rectangle which is overlapped
            merge_rect.left_ = prect_vec[istart].left_;
            // in this mode, there is no rectangles which are overlapped
            //merge_rect.left_ = prect_vec[istart].left_ + prect_vec[istart].wid_ - 1;
        }

        if (iend == -1) {
            merge_rect.top_ = imerge_rct_top;
            merge_rect.hei_ = imerge_rct_bot - imerge_rct_top + 1;
            merge_rect.wid_ = row_right - merge_rect.left_ + 1;
        } else {
            iend = std::max(iend, istart + 2);
            if (iend > irow_rect_vec_size) {
                iend = irow_rect_vec_size;
                bfinished = true;
            }
            imerge_rct_top0 = std::min(imerge_rct_top0, prect_vec[iend - 1].top_);
            imerge_rct_bot0 = std::max(imerge_rct_bot0,\
                    prect_vec[iend - 1].top_ + prect_vec[iend - 1].hei_ - 1);
            imerge_rct_top0 = std::max(imerge_rct_top0 - iexd_vertical_size, 0);
            imerge_rct_bot0 = std::min(imerge_rct_bot0\
                    + iexd_vertical_size, image_h - 1);
            merge_rect.top_ = imerge_rct_top0;
            merge_rect.hei_ = imerge_rct_bot0 - imerge_rct_top0 + 1;
            merge_rect.wid_ = prect_vec[iend - 1].left_\
                              + prect_vec[iend - 1].wid_ - merge_rect.left_;
        }
    
        if (merge_rect.wid_ > SEQL_MIN_IMAGE_HEIGHT\
                && merge_rect.hei_ > SEQL_MIN_IMAGE_WIDTH) {
            groupTempLine.rectVec_.push_back(merge_rect);
        }

        if (iend == -1 || bfinished) {
            break;
        }

        istart = std::max(iend - 1, 0);
    }
    // avoid the most right cc is added repeatedly
    int irect_num = groupTempLine.rectVec_.size();
    if (irect_num >= 2) {
        SSeqLEngRect &rect1 = groupTempLine.rectVec_[irect_num - 1];
        SSeqLEngRect &rect2 = groupTempLine.rectVec_[irect_num - 2];
        double daspect_ratio1 = (double)rect1.wid_ / rect1.hei_;
        if (daspect_ratio1 <= 0.5) {
            rect2.top_ = std::min(rect2.top_, rect1.top_);
            int iright = std::max(rect2.left_ + rect2.wid_ - 1,\
                    rect1.left_ + rect1.wid_ - 1);
            int ibottom = std::max(rect2.top_ + rect2.hei_ - 1,\
                    rect1.top_ + rect1.hei_ - 1);
            rect2.wid_ = iright - rect2.left_ + 1;
            rect2.hei_ = ibottom - rect2.top_ + 1;
            std::vector<SSeqLEngRect>::iterator it = \
                groupTempLine.rectVec_.begin() + irect_num - 1;
            groupTempLine.rectVec_.erase(it);
        }
    }

    return ;
}

void CSeqLAsiaWordSegProcessor::group_line_rects(
    const std::vector<SSeqLEngLineRect> &seg_line_rect_vec, 
    int group_width,
    std::vector<SSeqLEngLineRect> &combine_line_rect_vec) {
    // row by row
    int image_w = _m_pinput_img->width;
    int image_h = _m_pinput_img->height;
    int exd_vertical_size = 1;
    const double exd_horizontal_ratio = 0.0;
    const unsigned int iseg_line_vec_size = seg_line_rect_vec.size();
    for (unsigned int i = 0; i < iseg_line_vec_size; i++) { 
        const SSeqLEngLineRect & temp_org_line = seg_line_rect_vec[i];
        SSeqLEngLineRect & group_temp_line = combine_line_rect_vec[i];
        group_temp_line.isBlackWdFlag_ = temp_org_line.isBlackWdFlag_;
        group_temp_line.left_ = temp_org_line.left_;
        group_temp_line.top_ = temp_org_line.top_;
        group_temp_line.wid_ = temp_org_line.wid_;
        group_temp_line.hei_ = temp_org_line.hei_;
        group_temp_line.rectVec_.clear();
        // xie shufu add
        // this row has only one rectangle
        const int irow_rect_vec_size = temp_org_line.rectVec_.size();
        if (irow_rect_vec_size <= 0) {
            continue ;
        }
        
        // in this mode, the whole row is used as the input
        SSeqLEngRect row_rect = temp_org_line.rectVec_[0];
        row_rect.bePredictFlag_ = true;
        row_rect.score_ = 0;

        int row_top = row_rect.top_;
        int row_left = row_rect.left_;
        int row_right  = row_rect.left_ + row_rect.wid_-1;
        int row_bottom = row_rect.top_ + row_rect.hei_-1;
        for (int j = 1; j < irow_rect_vec_size; j++) {
            const SSeqLEngRect & rect_b = temp_org_line.rectVec_[j];
            row_top = std::min(row_top, rect_b.top_);
            row_left = std::min(row_left, rect_b.left_);
            row_right = std::max(row_right, rect_b.left_ + rect_b.wid_ - 1);
            row_bottom = std::max(row_bottom, rect_b.top_ + rect_b.hei_ - 1);
        }
        int irow_h = row_bottom - row_top + 1;
        int iexd_horizontal_size = (int)(irow_h * exd_horizontal_ratio);
        
        row_right = std::max(row_right, temp_org_line.left_ + temp_org_line.wid_ - 1);
        row_top = std::max(row_top - exd_vertical_size, 0);
        row_bottom = std::min(row_bottom + exd_vertical_size, image_h - 1);
        row_left = std::max(row_left - iexd_horizontal_size, 0);
        row_right = std::min(row_right + iexd_horizontal_size, image_w - 1);
        row_rect.left_ = row_left;
        row_rect.top_  = row_top;
        row_rect.wid_  = row_right - row_left + 1;
        row_rect.hei_  = row_bottom - row_top + 1;
        
        if (row_rect.wid_ < SEQL_MIN_IMAGE_HEIGHT\
             || row_rect.hei_ < SEQL_MIN_IMAGE_WIDTH) {
            continue;
        }
           
        if (group_width == -1 || irow_rect_vec_size == 1) {
            group_temp_line.rectVec_.push_back(row_rect);
            continue ;
        }
        
        //
        combine_rect_set_ada(temp_org_line, group_temp_line);
    }

    return ;
}
void CSeqLAsiaWordSegProcessor::word_group_nearby_adaptive(int groupWidth) {

    if (_m_pinput_img == NULL) {
        return ;
    }
    // sort the row rectangles
    std::vector<SSeqLEngLineRect> org_seg_line_vec = _m_in_seg_line_rect_vec; 
    for (unsigned int i = 0; i < org_seg_line_vec.size(); i++) {
        sort(org_seg_line_vec[i].rectVec_.begin(),\
            org_seg_line_vec[i].rectVec_.end(), left_seql_rect_first);
    }

    // group the word rectangle for the row
    const unsigned int seg_line_vec_size = org_seg_line_vec.size();
    _m_word_line_rect_vec.resize(seg_line_vec_size);
    group_line_rects(org_seg_line_vec, groupWidth, _m_word_line_rect_vec);

    return ;
}
/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
