#ifndef NSCOMMON_PDF_SAVER_H
#define NSCOMMON_PDF_SAVER_H

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <string.h>
#include <exception>
#include "hpdf.h"
#include <iostream>
#include <limits>
#include "locrecg.h"
#include <iconv.h>
#include <sstream>
#include<algorithm>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <iostream>
#include <math.h>

namespace nsmocr
{
typedef struct
{
    double color_r;
    double color_g;
    double color_b;
}PDFColor;

typedef struct
{
    int left;
    int top;
    int width;
    int height;
}PDFRect;

typedef struct
{
    PDFColor color;
    PDFRect rect;
    std::string text;
}PDFResult;

class CPDFSaver
{
public:
    CPDFSaver(const nsmocr::recg_result_t& recg_result, std::string path, std::string query_sign, 
            const IplImage* src_img);
    ~CPDFSaver();
    int init();
    int save();
    int get_ocr_color(nsmocr::recg_region_t src, int i);

private:
    static void error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void *user_data);
    bool utf8_to_gbk(const char *inbuf, size_t inlen, char *outbuf, size_t outlen);
    void handle_rect_overlap();
    static bool sort_pic_by_y(const cv::Rect &rect1, const cv::Rect &rect2);
    void handle_pic_overlap();
    void cover_text();
    void generate_contours();
    bool is_small_area(CvRect rect);
    bool is_overlap_text(cv::Rect rect);
    bool is_overlap_pic(cv::Rect rect);
    void generate_pic_rects();
    void insert_pic(HPDF_Doc pdf, HPDF_Page page, float scale, int height);
    //int check_path();
    int save_file(const char * file_name);
    int calc_real_height(nsmocr::recg_region_t &src);
    int cut_core_img();

private:
    std::string _outpath;
    std::string _query_sign;

    IplImage* _threshold_img;
    IplImage* _src_img;
    IplImage* _cut_img;
    std::vector<cv::Rect> _frag_rects;
    std::vector<cv::Rect> _pic_rects; 
    std::vector<PDFResult> _results; 
    std::vector<PDFResult> _origin_results; 
    std::vector<int> _pic_offset;
    std::vector<int> _real_height;

    const int EDGE_BLANK_SIZE;
    const float SMALL_FONT_THRESHOLD;
    const float FONT_PIXEL_PROP;
    const int FONT_SIZE_TRY_TIMES;
    const mode_t CREATE_DIR_MODE;
    const std::string PDF_DIR_NAME;
    const std::string TMP_DIR_NAME;
    const int SMALL_AREA_SCALE;
    const HPDF_PageSizes PDF_PAGE_SIZE;
    const HPDF_REAL A4_WIDTH;
    const HPDF_REAL A4_HEIGHT;
};
}

#endif
