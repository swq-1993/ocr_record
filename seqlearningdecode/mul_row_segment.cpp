/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file MulRowSegment.cpp
 * @author xieshufu(com@baidu.com)
 * @date 2015/06/24 17:47:05
 * @brief 
 *  
 **/
#include "mul_row_segment.h"
#include "SeqLEngSegmentFun.h"
#include "common_func.h"
#include "dll_main.h"

extern char g_pimage_title[256];
extern char g_psave_dir[256];
extern int g_iline_index;

int get_cropped_gray_patch(IplImage *pImage,  SeqLESegDecodeRect *pRect, IplImage *&pGrayImg);

void save_rect_image(unsigned char *pbyData, int iWidth, int iHeight, SSeqLESegRect *rects,\
        int rectNum, char *pstrExtName);
void save_mulrow_image(unsigned char *pbyData, int iWidth, int iHeight,\
        std::vector<SeqLESegDecodeRect> vecRect);

void merge_neighbor_cc(SSeqLESegRect *rects, int &rectNum, int iMinWH);

int link_cc_to_row(SSeqLESegRect *rects, int rectNum,\
        int isBlackWdFlag, std::vector<SeqLESegDecodeRect> &vecRect);

void remove_invalid_rect(std::vector<SeqLESegDecodeRect> &vecRect);


//Input: pImage: image data
//       pRect: the row rectangle 
//Output: vecRect: the vector of row rectangles      
int segment_mulrow_data(IplImage *pImage, SeqLESegDecodeRect *pRect,\
        std::vector<SeqLESegDecodeRect> &vecRect)  {
    int i_ret = 0;

    int n_channel = pImage->nChannels;
    if (n_channel!=1 && n_channel!=3) {
        return -1;
    }


    //1. get the cropped row image data
    IplImage  *pgray_img = NULL;
    i_ret = get_cropped_gray_patch(pImage, pRect, pgray_img);
    if (i_ret!=0) {
        return i_ret;
    }

    //2. find the connected components
    int is_black_wd_flag = pRect->isBlackWdFlag;

    int icrop_w = pgray_img->width;
    int icrop_h = pgray_img->height;
    int icrop_dim = icrop_w*icrop_h;
    unsigned char *pbycrop_data = new unsigned char[icrop_dim];
    for (int i = 0; i < icrop_h; i++) {
        memcpy(pbycrop_data+i*icrop_w, pgray_img->imageData+i*pgray_img->widthStep, icrop_w);
    }
    SSeqLESegRect *rects = NULL;
    int rect_num = 0;
    int imin_wh = std::min(icrop_w, icrop_h);
    find_line_image_rect(pbycrop_data, icrop_w, icrop_h, pRect, rects, rect_num);

    int idraw_cc_single_flag = g_ini_file->get_int_value(\
            "DRAW_MUL_ROW_CC_SINGLE_FLAG", IniFile::_s_ini_section_global);
    if (idraw_cc_single_flag == 1) {
        save_rect_image(pbycrop_data, icrop_w, icrop_h, rects, rect_num, "single");
    }

    merge_neighbor_cc(rects, rect_num, imin_wh);
    
    int idraw_cc_merge_flag = g_ini_file->get_int_value(\
            "DRAW_MUL_ROW_CC_MERGE_FLAG", IniFile::_s_ini_section_global);
    if (idraw_cc_merge_flag == 1) {
        save_rect_image(pbycrop_data, icrop_w, icrop_h, rects, rect_num, "merge");
    }

    //3. link these connected components into the rows
    link_cc_to_row(rects, rect_num, is_black_wd_flag, vecRect);
    remove_invalid_rect(vecRect);
    
    int idraw_cc_link_flag = g_ini_file->get_int_value(\
            "DRAW_MUL_ROW_CC_LINK_FLAG", IniFile::_s_ini_section_global);
    if (idraw_cc_link_flag == 1) {
        save_mulrow_image(pbycrop_data, icrop_w, icrop_h, vecRect);
    }
    int ivec_rect_size = vecRect.size();
    for (int i = 0; i < ivec_rect_size; i++) {
        vecRect[i].left += pRect->left;
        vecRect[i].top += pRect->top;
        vecRect[i].right += pRect->left;
        vecRect[i].bottom += pRect->top;
    }

    if (pgray_img) {
        cvReleaseImage(&pgray_img);
        pgray_img = NULL;
    }
    if (pbycrop_data) {
        delete []pbycrop_data;
        pbycrop_data = NULL;
    }
    if (rects) {
        delete []rects;
        rects = NULL;
    }

    return i_ret;
}

int get_cropped_gray_patch(IplImage *pImage,  SeqLESegDecodeRect *pRect, IplImage *&pGrayImg)  {
    int iret = 0;
    int image_w = pImage->width;
    int image_h = pImage->height;

    int ileft = pRect->left;
    int itop = pRect->top;
    int iright = pRect->right;
    int ibottom = pRect->bottom;
    int icrop_w = std::min(iright - ileft + 1, image_w-1);
    int icrop_h = std::min(ibottom - itop + 1, image_h-1);
    if (icrop_w <= 0 || icrop_h <= 0 ||\
            icrop_w > image_w || icrop_h > image_h) {
        printf("invalid position of the detected row!\n");
        return -1;
    }
    IplImage *pcrop_img = cvCreateImage(cvSize(icrop_w, icrop_h),\
            pImage->depth, pImage->nChannels);

    cvSetImageROI(pImage, cvRect(ileft, itop, icrop_w, icrop_h));
    cvCopy(pImage, pcrop_img, 0);
    cvResetImageROI(pImage);
    
    int n_channel = pImage->nChannels;
    pGrayImg = cvCreateImage(cvSize(icrop_w, icrop_h), 8, 1);
    if (n_channel == 1) {
        cvCopy(pcrop_img, pGrayImg, 0);
    }
    else {
        cvCvtColor(pcrop_img, pGrayImg, CV_BGR2GRAY);
    }
    if (pcrop_img) {
        cvReleaseImage(&pcrop_img);
        pcrop_img = NULL;
    }

    return iret;
} 

void save_rect_image(unsigned char *pbyData, int iWidth, int iHeight, SSeqLESegRect *rects,\
        int rectNum, char *pstrExtName)  {

    int image_dim = iWidth*iHeight;
    unsigned char *ptemp = new unsigned char[image_dim*3];
    for (int i = 0; i < image_dim; i++) {
        ptemp[i*3]  = pbyData[i];
        ptemp[i*3+1]  = pbyData[i];
        ptemp[i*3+2]  = pbyData[i];
    }
    for (int irct_idx = 0; irct_idx < rectNum; irct_idx++) {
        int r = 0;
        int g = 0;
        int b = 0;
        if (irct_idx % 2 == 0) {
            r = 255;
            g = 0;
            b = 0;
        }
        else {
            r = 0;
            g = 255;
            b = 0;
        }
        draw_rectangle(ptemp, iWidth, iHeight,\
            rects[irct_idx].left, rects[irct_idx].right, \
            rects[irct_idx].top, rects[irct_idx].bottom, 1, r, g, b);
    }
    char file_path[256];
    snprintf(file_path, 256, "%s/%s-%d-rect-%s.jpg", g_psave_dir,\
            g_pimage_title, g_iline_index, pstrExtName);
    save_color_data(ptemp, iWidth, iHeight, file_path);
    
    if (ptemp) {
        delete []ptemp;
        ptemp = NULL;
    }

    return ;
}

void save_mulrow_image(unsigned char *pbyData, int iWidth, int iHeight,\
        std::vector<SeqLESegDecodeRect> vecRect)  {

    int image_dim = iWidth*iHeight;
    unsigned char *ptemp = new unsigned char[image_dim*3];
    for (int i = 0; i < image_dim; i++) {
        ptemp[i*3]  = pbyData[i];
        ptemp[i*3+1]  = pbyData[i];
        ptemp[i*3+2]  = pbyData[i];
    }
    for (int irct_idx = 0; irct_idx < vecRect.size(); irct_idx++) {
        draw_rectangle(ptemp, iWidth, iHeight,\
            vecRect[irct_idx].left, vecRect[irct_idx].right, \
            vecRect[irct_idx].top, vecRect[irct_idx].bottom,\
            1, 255, 0, 0);
    }
    char file_path[256];
    snprintf(file_path, 256, "%s/%s-%d-mulrow.jpg", g_psave_dir, g_pimage_title, g_iline_index);
    save_color_data(ptemp, iWidth, iHeight, file_path);
    
    if (ptemp) {
        delete []ptemp;
        ptemp = NULL;
    }

    return ;
}

void merge_cc(SSeqLESegRect *rects, int rectNum, int *piCCFlag)   {

    for (int i = 0; i < rectNum; i++) {
        SSeqLESegRect *rt1 = rects + i;
        if (piCCFlag[i] == 1) {
            continue;
        }
	
        int rw1 = rt1->right - rt1->left + 1;
        int rh1 = rt1->bottom - rt1->top + 1;
        bool bupdate_flag = false;

        // recursively check the relationship between this rectangle and other rectangles
        while (1)  {
            bupdate_flag = false;
            for (int j = 0; j < rectNum; j++) {
                SSeqLESegRect *rt2 = rects + j;
                if (piCCFlag[j] == 1 || j == i) {
                    continue;
                }
                // check whether two rectangles are horizontally overlaped
                bool boverlap_wflag = false;
                bool boverlap_hflag = false;

                int rw2 = rt2->right - rt2->left + 1;
                int rh2 = rt2->bottom - rt2->top + 1;
                int imin_x = std::min(rt1->left, rt2->left);
                int imax_x = std::max(rt1->right, rt2->right);
                int ix_len = imax_x - imin_x + 1;
                float foverlap_w = rw1+rw2-ix_len;

                int imin_y = std::min(rt1->top, rt2->top);
                int imax_y = std::max(rt1->bottom, rt2->bottom);
                int iy_len = imax_y - imin_y + 1;
                float foverlap_h = rh1 + rh2 - iy_len;
                int imin_h = std::min(rh1, rh2);
                int imin_w = std::min(rw1, rw2);
                float foverlap_ht = imin_h*0.5;
                float foverlap_ht1 = std::min((-0.1) * imin_h, -3.0);
                float foverlap_wt = imin_w * 0.8;
                float foverlap_wt1 = (-0.1) * imin_w;

                // these two CCs are horizontally overlapped
                if (foverlap_w >= foverlap_wt1 && foverlap_h >= foverlap_ht) {
                    boverlap_wflag = true;
                }
                else if (foverlap_h >= foverlap_ht1 && foverlap_w >= foverlap_wt && rectNum <= 5) {
                    // these two CCs are vertically overlapped
                    boverlap_hflag = true;
                }
     /*           else if(fOverlapH>=5 && fOverlapW>=-5)  {
                    bOverlapHFlag = true;
                }*/

                if (boverlap_wflag || boverlap_hflag) {
                    rt1->left   = MIN_SEQLESEG(rt1->left, rt2->left);
                    rt1->right  = MAX_SEQLESEG(rt1->right, rt2->right);
                    rt1->top    = MIN_SEQLESEG(rt1->top, rt2->top);
                    rt1->bottom = MAX_SEQLESEG(rt1->bottom, rt2->bottom);
                    rw1 = rt1->right - rt1->left + 1;
                    rh1 = rt1->bottom - rt1->top + 1;
                    piCCFlag[j]  = 1;
                    bupdate_flag = true;
                }
            }
            if (!bupdate_flag) {
                break;
            }
        }
    }
}


// merge the ccs which are vertically or horizontally adjacent
void merge_neighbor_cc(SSeqLESegRect *rects, int &rectNum, int iMinWH)  {
    if (rectNum <= 1) {
        return;
    }

    int *picc_flag = NULL;
    int in_rect_num = rectNum;
    SSeqLESegRect *pin_rects = new SSeqLESegRect[in_rect_num];
    memcpy(pin_rects, rects, sizeof(SSeqLESegRect)*rectNum);

    picc_flag = new int[rectNum];
    memset(picc_flag, 0, sizeof(int)*rectNum);
    merge_cc(pin_rects, in_rect_num, picc_flag);

    // remove the CCs whose size are too small
    float dcc_size_thresh = iMinWH*0.1;
    for (int i = 0; i < in_rect_num; i++) {
        if (picc_flag[i] == 1) {
            continue;
        }
        int icc_w = pin_rects[i].right - pin_rects[i].left + 1;
        int icc_h = pin_rects[i].bottom - pin_rects[i].top + 1;
        if (icc_w <= dcc_size_thresh || icc_h <= dcc_size_thresh) {
            picc_flag[i] = 1;
        }
    }

    rectNum = 0;
    for (int i = 0; i < in_rect_num; i++) {
        if (picc_flag[i] == 1) {
            continue;
        }
        rects[rectNum] = pin_rects[i];
        rectNum++;
    }

    if (picc_flag) {
        delete []picc_flag;
        picc_flag = NULL;
    }
    if (pin_rects) {
        delete []pin_rects;
        pin_rects = NULL;
    }

    return ;
}
int find_cc_with_max_w(SSeqLESegRect *rects, int rectNum, int *piCCRowLabel, int &iCCIndex)  {
    iCCIndex = -1;

    int imin_value = -1;
    for (int i = 0; i < rectNum; i++) {
        if (piCCRowLabel[i]!=-1) {
            continue;
        }
        int icc_w = rects[i].right - rects[i].left + 1;
        if (imin_value == -1) {
            imin_value = icc_w;
            iCCIndex = i;
        }
        else {
            if (imin_value < icc_w) {
                imin_value = icc_w;
                iCCIndex = i;
            }
        }
    }

    return 0;
}

void link_cc_to_seedcc(SSeqLESegRect *rects, int rectNum, int *piCCRowLabel, \
        int iRowLabel, SeqLESegDecodeRect &MergeCC)  {

    bool bupdate_flag = false;

    while (1) {    

        bupdate_flag = false;
        //int imerge_w = MergeCC.right - MergeCC.left + 1;
        int imerge_h = MergeCC.bottom - MergeCC.top + 1;
        for (int i = 0; i < rectNum; i++) {
            if (piCCRowLabel[i] != -1) {
                continue;
            }

            //
            //int icc_w = rects[i].right - rects[i].left + 1;
            int icc_h = rects[i].bottom - rects[i].top + 1;
            int imin_y = std::min(MergeCC.top, rects[i].top);
            int imax_y = std::max(MergeCC.bottom, rects[i].bottom);

            // check whether the two CCs are overlapped along vertical direction
            int iy_len = imax_y - imin_y + 1;
            int ioverlap_h = imerge_h + icc_h - iy_len;
            int imin_h = std::min(imerge_h, icc_h);
            double doverlap_h_t = imin_h*0.5;
            if (ioverlap_h < doverlap_h_t) {
                continue;
            }
            // check the horizontal distance
            int ihor_dist = rects[i].left - MergeCC.right + 1;
            int imin_w = (icc_h + imerge_h) / 2; //std::min(icc_w, imerge_w);
            double dhordist_t = imin_w*3.0;
            if (ihor_dist < dhordist_t) {
                MergeCC.left = std::min(MergeCC.left, rects[i].left);
                MergeCC.right = std::max(MergeCC.right, rects[i].right);
                MergeCC.top = std::min(MergeCC.top, rects[i].top);
                MergeCC.bottom = std::max(MergeCC.bottom, rects[i].bottom);
                piCCRowLabel[i] = iRowLabel;
                bupdate_flag = true;
            }
        }

        if (!bupdate_flag) {
            break;
        }
    }
}

int link_cc_to_row(SSeqLESegRect *rects, int rectNum, \
        int isBlackWdFlag, std::vector<SeqLESegDecodeRect> &vecRect)  {
    int iret = 0;
    
   //1. find the CC with the minimum x coordinate
   //2. link other CCs to this CC
   //3. in the remaining CCs, find the CC with the minimum x coodinate
   //4. repeat step 2 until all the CCs are labeled
    int *picc_row_label = NULL;
    picc_row_label = new int[rectNum];

    // the initial value for each CC
    for (int i = 0; i < rectNum; i++) {
        picc_row_label[i] = -1;
    }
    int irow_num = 0;
    while (1) {
        int imin_cc_index = -1;
        find_cc_with_max_w(rects, rectNum, picc_row_label, imin_cc_index);
        if (imin_cc_index == -1) {
            break;
        }
        picc_row_label[imin_cc_index] = irow_num;

        SeqLESegDecodeRect merge_cc;
        merge_cc.isBlackWdFlag = isBlackWdFlag;
        merge_cc.left = rects[imin_cc_index].left;
        merge_cc.top = rects[imin_cc_index].top;
        merge_cc.right = rects[imin_cc_index].right;
        merge_cc.bottom = rects[imin_cc_index].bottom;
        link_cc_to_seedcc(rects, rectNum, picc_row_label, irow_num, merge_cc);
        vecRect.push_back(merge_cc);

        irow_num++;
    }

    if (picc_row_label) {
        delete []picc_row_label;
        picc_row_label = NULL;
    }

    return iret;
}

void merge_vertical_overlap_cc(std::vector<SeqLESegDecodeRect> &vecRect, int *piCCFlag)   {

    std::vector<SeqLESegDecodeRect> in_vec_rect = vecRect;
    int rect_num = in_vec_rect.size();

    vecRect.clear();
    for (int i = 0; i < rect_num; i++) {
        SeqLESegDecodeRect rt1 = in_vec_rect[i];
        if (piCCFlag[i] == 1) {
            continue;
        }
		
        int rw1 = rt1.right - rt1.left + 1;
        int rh1 = rt1.bottom - rt1.top + 1;
        bool bupdate_flag = false;

        // recursively check the relationship between this rectangle and other rectangles
        while (1) {
            bupdate_flag = false;
            for (int j = 0; j < rect_num; j++) {
                if (piCCFlag[j] == 1 || j == i) {
                    continue;
                }

                // check whether two rectangles are horizontally overlaped
                SeqLESegDecodeRect rt2 = in_vec_rect[j];
                bool boverlap_h_flag = false;

                //int rw2 = rt2.right - rt2.left + 1;
                int rh2 = rt2.bottom - rt2.top + 1;
                int imin_y = std::min(rt1.top, rt2.top);
                int imax_y = std::max(rt1.bottom, rt2.bottom);
                int iy_len = imax_y - imin_y + 1;
                float foverlap_h = rh1 + rh2 - iy_len;
                float foverlap_h_t = 1;
                
                // these two CCs are horizontally overlapped
                if (foverlap_h >= foverlap_h_t) {
                    // these two CCs are vertically overlapped
                    boverlap_h_flag = true;
                }

                if (boverlap_h_flag) {
                    rt1.left   = MIN_SEQLESEG(rt1.left, rt2.left);
                    rt1.right  = MAX_SEQLESEG(rt1.right, rt2.right);
                    rt1.top    = MIN_SEQLESEG(rt1.top, rt2.top);
                    rt1.bottom = MAX_SEQLESEG(rt1.bottom, rt2.bottom);
                    rw1 = rt1.right - rt1.left + 1;
                    rh1 = rt1.bottom - rt1.top + 1;
                    piCCFlag[j]  = 1;
                    bupdate_flag = true;
                }
            }
            if (!bupdate_flag) {
                break;
            }
        }
        vecRect.push_back(rt1);
    }
}
void remove_invalid_rect(std::vector<SeqLESegDecodeRect> &vecRect)  {
    int ivec_rect_size = vecRect.size();

    if (ivec_rect_size <= 1) {
        return ;
    }
    int *piflag = new int[ivec_rect_size];
    memset(piflag, 0, sizeof(int)*ivec_rect_size);

    int imax_area = -1;
    int imax_idx = -1;
    for (int i = 0; i < ivec_rect_size; i++) {
        int iarea = (vecRect[i].right-vecRect[i].left+1)*(vecRect[i].bottom-vecRect[i].top+1);
        if (imax_area < iarea) {
            imax_area = iarea;
            imax_idx = i;
        }
    }

    // remove those CC with small height value
    int imax_area_h = vecRect[imax_idx].bottom - vecRect[imax_idx].top + 1;
    float fthresh = imax_area_h*0.25;
    for (int i = 0; i < ivec_rect_size; i++) {
        int irect_h = vecRect[i].bottom - vecRect[i].top + 1;
        if (irect_h < fthresh) {
            piflag[i] = 1;
        }
    }
    // check whether CCs are overlapped vertically
    merge_vertical_overlap_cc(vecRect, piflag);
    
    if (piflag) {
        delete []piflag;
        piflag = NULL;
    }
    return ;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
