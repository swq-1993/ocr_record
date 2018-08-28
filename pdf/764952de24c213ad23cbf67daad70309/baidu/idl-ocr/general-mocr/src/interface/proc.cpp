#include "proc.h"
#include "formula_process.h"
#include "utils.h"
#include "contrast_enhance.h"
#include "image_process.h"
#include "async_save_img.h"
#include "auto.h"
#include "mask.h"
#include "ocr_timer.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <cv.h>
#include <pthread.h>
#include "template_ocr_convert.h"
#include "idcard_api.h"
#include "business_license_api.h" // business license
#include "driving_license_api.h"
#include "driving_license.h"
#include "vehicle_license_api.h"
#include "vehicle_license.h"
#include "license_rotate_api.h"
#include "bdocr_recg.h"
#include "ocrattr_timu.h"
#include "recog_post_process.h"
#include "lottery.h"
#include "detected_obj_type.h"
#include "seq_menu_convert.h"
#include "id_card.h"
#include "lang_cate_classify.h"
#include "medical_model_fusion.h"

extern appocr_conf g_conf;
extern nscommon::CAsyncProc g_async;
extern langcateclassify::lang_cate_data_t g_lang_cate_data;

namespace nsmocr
{

static void warp_box(int& left, int& top, int& right, int& bottom, cv::Mat warp_mat)
{    
    std::vector<cv::Point2f> src;
    std::vector<cv::Point2f> dst;
    src.push_back(cv::Point2f(left,  top));
    src.push_back(cv::Point2f(left,  bottom));
    src.push_back(cv::Point2f(right,  top));
    src.push_back(cv::Point2f(right,  bottom));
    cv::perspectiveTransform(src, dst, warp_mat);

    left   = dst[0].x;
    right  = dst[0].x;
    top    = dst[0].y;
    bottom = dst[0].y;
    for (int i = 1; i < dst.size(); i++)
    {
        left   = std::min(left,   int(dst[i].x));  
        right  = std::max(right,  int(dst[i].x));  
        top    = std::min(top,    int(dst[i].y));  
        bottom = std::max(bottom, int(dst[i].y));  
    }

    return ;
}

// warp recog result
// recog_result_in  [In]:  recg_result before warp
// warp_mat         [In]:  3x3 warp mat
// recog_result_out [Out]: recg_result after warp
static void warp_recog_result(const recg_result_t& recog_result_in, 
                       cv::Mat warp_mat,
                       recg_result_t& recog_result_out)
{
    recog_result_out = recog_result_in;
    recog_result_out.regions.clear();
    for (int i = 0; i < recog_result_in.regions.size(); i++)
    {
        nsmocr::recg_region_t region = recog_result_in.regions[i];

        int region_left = region.left;
        int region_top = region.top;
        int region_right  = region.right;
        int region_bottom = region.bottom;
        warp_box(region.left, region.top, region.right, region.bottom, warp_mat);
       
        for (int j = 0; j < region.wordinfo.size(); j++)
        {
            nsmocr::recg_wordinfo_t& wordinfo = region.wordinfo[j];
            warp_box(wordinfo.rect.left, wordinfo.rect.top, 
                     wordinfo.rect.right, wordinfo.rect.bottom, warp_mat);
        }
        recog_result_out.regions.push_back(region);
    }
    return ;
}

int CProc::init(const init_t& init_arg)
{
    nscommon::time_elapsed_t time_used(__FUNCTION__, 1);
    init_t::get_normal(init_arg, &_init_arg);

    nsmocr::set_ctswitch(&_init_arg.ctrl_switch, _init_arg.databuf);

    int ret = nsmocr::CQueryData::load_image(_init_arg.databuf,
            *_init_arg.conf, _src_img);
   
    if (ret) 
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOAD_IMG);
        WARNING_LOG("logid:%llu load image error!", _init_arg.databuf->m_busFcgi2ocr.m_uLogId);
        return nsmocr::ERR_LOAD_IMG;
    }

    _rescale_ratio = 1.0;
    _image_width  = _src_img->width;
    _image_height = _src_img->height;

    if (nsmocr::CQueryData::is_id_card(_init_arg.databuf))
    {
        int fix_width = 1000;
        if (_src_img->width < fix_width)
        {
            IplImage* rsz_img = 
                      nscommon::CImageProcess::resize_img_to_fix_width(_src_img, fix_width);
        
            if (rsz_img)
            {
                TRACE_LOG("src_img h:d w:%d resize image h:%d w:%d",
                    _src_img->height, _src_img->width,
                    rsz_img->height, rsz_img->width);

                _rescale_ratio = rsz_img->height / float(_src_img->height);
                cvReleaseImage(&_src_img);
                _src_img = rsz_img;
            }
        }
    } 

    if (g_conf.image_save_mode > 0 && nsmocr::CQueryData::is_save_img(_init_arg.databuf)) 
    {
        unsigned int buffer_length = _init_arg.databuf->m_busFcgi2ocr.m_uImageDataLen;
        char* img_buffer = new(std::nothrow) char[buffer_length];
        if (img_buffer != NULL)
        {
            memcpy(img_buffer, _init_arg.databuf->m_busFcgi2ocr.m_pImageData, buffer_length);
            
            nscommon::CAsyncSaveImage::async_save_query_img(img_buffer, buffer_length,
                    g_conf.image_save_path,
                    _init_arg.databuf->m_busFcgi2ocr._m_appid, 
                    _init_arg.databuf->m_busFcgi2ocr.m_pszQuerySign, g_async);
        }
    }

    if (pthread_mutex_init(&_lock, NULL)) {
        _lock_init = false;
        return -1;
    } else {
        _lock_init = true;
    }

    return 0;
}

int CProc::proc()
{
    nscommon::time_elapsed_t time_used(__FUNCTION__, 1);

    if (CQueryData::is_auto_enhance(_init_arg.databuf))
    {
        IplImage *enhanced_img = cvCreateImage(cvGetSize(_src_img), _src_img->depth, 3);
        int ret_enhance = nsmocr::enhance_color_image_contrast(_src_img, enhanced_img);
        if (0 == ret_enhance)
        {
            TRACE_LOG("enhance src_img");
            cvReleaseImage(&_src_img);
            _src_img = enhanced_img;
        }
        else
        {
            cvReleaseImage(&enhanced_img);
        }
    }

    //TODO:check which object_type
    //common
    if (CQueryData::is_locate(_init_arg.databuf))
    {
        return _loc();
    }

    if (CQueryData::is_locate_recg(_init_arg.databuf))
    {
        if (CQueryData::is_formula_with_text(_init_arg.databuf))
        {
            return _loc_recg_with_formula();
        }
        else if (CQueryData::is_formula(_init_arg.databuf))
        {
            return _loc_recg_formula();
        }
        else if (CQueryData::is_id_card(_init_arg.databuf))
        {
            return _loc_recg_idcard();
        }
        else if (CQueryData::is_driving_license(_init_arg.databuf))
        {
            return _loc_recg_driving_license();
        }
        else if (CQueryData::is_vehicle_license(_init_arg.databuf))
        {
            return _loc_recg_vehicle_license();
        }
        else if (CQueryData::is_medical_model_fusion(_init_arg.databuf))
        {
            return _loc_recg_medical_model_fusion();
        }
        else
        {
            return _loc_recg();
        }
    }

    WARNING_LOG("surported?");
    return 0;
}

//-90
static bool sort_region_1(const recg_region_t& a, const recg_region_t& b)
{
    int a_h = a.right - a.left + 1;
    int b_h = b.right - b.left + 1;

    int abs_overlap = min(a.bottom - b.top, b.bottom - a.top);
    int norm = min(a.bottom - a.top, b.bottom - b.top);
    if (abs_overlap > 0 && abs_overlap > 0.3 * norm)
    {
        return a.left < b.left;
    }

    if (a.left + 0.2 * a_h > b.left + b_h * 0.8)
    {
        //DEBUG_LOG("%s > %s", a.ocrresult.c_str(), b.ocrresult.c_str());
        return false;
    }
    else if (a.left + a_h*0.8 < b.left + b_h * 0.2)
    {
        //DEBUG_LOG("%s < %s", a.ocrresult.c_str(), b.ocrresult.c_str());
        return true;
    }
    else
    {
        //DEBUG_LOG("%s left:%d %s left:%d", a.ocrresult.c_str(), a.left, 
        //        b.ocrresult.c_str(), b.left);
        return ((a.bottom - b.bottom) > 0);
    }

}

//-180
static bool sort_region_2(const recg_region_t& a, const recg_region_t& b)
{
    int a_h = a.bottom - a.top + 1;
    int b_h = b.bottom - b.top + 1;
    
    int abs_overlap = min(a.right - b.left, b.right - a.left);
    int norm = min(a.right - a.left, b.right - b.left);
    if (abs_overlap > 0 && abs_overlap > 0.3 * norm)
    {
        return a.bottom > b.bottom;
    }

    if (a.bottom - 0.2 * a_h > b.bottom - b_h * 0.8)
    {
        //DEBUG_LOG("%s > %s", a.ocrresult.c_str(), b.ocrresult.c_str());
        return true;
    }
    else if (a.bottom - a_h*0.8 < b.bottom - b_h * 0.2)
    {
        //DEBUG_LOG("%s < %s", a.ocrresult.c_str(), b.ocrresult.c_str());
        return false;
    }
    else
    {
        //DEBUG_LOG("%s left:%d %s left:%d", a.ocrresult.c_str(), a.left, 
        //        b.ocrresult.c_str(), b.left);
        return ((a.right - b.right) > 0);
    }
}

//-270
static bool sort_region_3(const recg_region_t& a, const recg_region_t& b)
{
    int a_h = a.right - a.left + 1;
    int b_h = b.right - b.left + 1;

    int abs_overlap = min(a.bottom - b.top, b.bottom - a.top);
    int norm = min(a.bottom - a.top, b.bottom - b.top);
    if (abs_overlap > 0 && abs_overlap > 0.3 * norm)
    {
        return a.right > b.right;
    }

    if (a.right - 0.2 * a_h > b.right - b_h * 0.8)
    {
        //DEBUG_LOG("%s > %s", a.ocrresult.c_str(), b.ocrresult.c_str());
        return true;
    }
    else if (a.right - a_h*0.8 < b.right - b_h * 0.2)
    {
        //DEBUG_LOG("%s < %s", a.ocrresult.c_str(), b.ocrresult.c_str());
        return false;
    }
    else
    {
        //DEBUG_LOG("%s left:%d %s left:%d", a.ocrresult.c_str(), a.left, 
        //        b.ocrresult.c_str(), b.left);
        return ((a.top - b.top) < 0);
    }
}

static bool sort_region_0(const recg_region_t& a, const recg_region_t& b)
{
    int a_h = a.bottom - a.top + 1;
    int b_h = b.bottom - b.top + 1;

    int abs_overlap = min(a.right - b.left, b.right - a.left);
    int norm = min(a.right - a.left, b.right - b.left);
    if (abs_overlap > 0 && abs_overlap > 0.3 * norm)
    {
        return a.top < b.top;
    }

    if (a.top + 0.2 * a_h > b.top + b_h * 0.8)
    {
        //DEBUG_LOG("%s > %s", a.ocrresult.c_str(), b.ocrresult.c_str());
        return false;
    }
    else if (a.top + a_h*0.8 < b.top + b_h * 0.2)
    {
        //DEBUG_LOG("%s < %s", a.ocrresult.c_str(), b.ocrresult.c_str());
        return true;
    }
    else
    {
        //DEBUG_LOG("%s left:%d %s left:%d", a.ocrresult.c_str(), a.left, 
        //        b.ocrresult.c_str(), b.left);
        return ((a.left - b.left) < 0);
    }
}

static bool sort_regions(const recg_region_t& a, const recg_region_t& b)
{
    int direction = a.direction;
    
    if (direction == 1)
    {
        return sort_region_1(a, b);
    }
    if (direction == 2)
    {
        return sort_region_2(a, b);
    }
    if (direction == 3)
    {
        return sort_region_3(a, b);
    }

    //assert(false);
    return sort_region_0(a, b);
}

typedef bool (*RecgResultCompareType) (const recg_region_t &, const recg_region_t &);

static void _quick_sort(std::vector<recg_region_t>& a, 
                        RecgResultCompareType lt,
                        int low, int high)
{

    if (low >= high)
    {
        return;
    }
    int first = low;
    int last = high;
    recg_region_t key = a[first];

    while (first < last)
    {
        while (first < last && !lt(a[last], key))
        {
            --last;
        }

        a[first] = a[last];

        if (first < last)
        {
            ++first;
        }

        while (first < last &&  !lt(key, a[first]))
        {
            ++first;
        }

        a[last] = a[first];

        if (first < last)
        {
            --last;
        }
    }
    a[first] = key;

    _quick_sort(a, lt, low, first - 1);
    _quick_sort(a, lt, first + 1, high);
}

static bool sort_recg_rst_line(recg_region_t t1, recg_region_t t2) {
    return t1.sort_line_measure < t2.sort_line_measure;
}

static void _first_std_sort(std::vector<recg_region_t>& regions)
{
    if (regions.size() <= 0) { 
        return ;
    }
    int recg_line_num = regions.size();
    for (int line_idx = 0; line_idx < recg_line_num; line_idx++) {
        char line_info[256];
        snprintf(line_info, 256, "%d_%d_%d_%d_%d", regions[line_idx].left,
            regions[line_idx].top, regions[line_idx].right,
            regions[line_idx].bottom, regions[line_idx].wordinfo.size());
            regions[line_idx].sort_line_measure = std::string(line_info);
    }
    std::sort(regions.begin(), regions.end(), sort_recg_rst_line);
    for (int i = 0; i < recg_line_num; i++)
    {
        DEBUG_LOG("recg_result_sort[%d]: %s", i, regions[i].sort_line_measure.c_str());
    }
    return ;
}

static void _quick_sort(std::vector<recg_region_t>& regions, 
                        RecgResultCompareType cmp)
{
    if (regions.size() <= 1)
    {
        return ;
    }

    _quick_sort(regions, cmp, 0, regions.size() - 1);
}

int CProc::_loc_recg_formula()
{
    nscommon::time_elapsed_t time_used(__FUNCTION__, 1);

    // call locate process to get image direction.
    loc_init_t loc_init_arg;
    loc_init_arg.copy(_init_arg);

    CRcnnLoc proc_loc;
    if (proc_loc.init(&loc_init_arg))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_NO_MEM);
        WARNING_LOG("logid:%d loc init error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_NO_MEM;
    }

    int ret = proc_loc.locate(_src_img);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("logid:%d locate error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_LOC;
    }

    mathocr::MathRecognizor recg(_init_arg.model->_m_formula_model);

    nsmocr::loc_result_t loc_result;
    std::vector<mathocr::loc_result_t> loc_ret_formula; 
    if (nsmocr::CFormulaOCRConvert::convert_input(loc_result, loc_ret_formula))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("convert_input error!");
        return nsmocr::ERR_INTER_FORMAT;
    }

    std::string recg_ret_latex;
    mathocr::recg_result_t recg_ret_symbols;
    
    // get rotated image
    IplImage *rotated_image = nsmocr::rotate_image2up_by_dir(_src_img, proc_loc.get_text_attr());

    if (rotated_image == NULL)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("rotate_image error!");
        return nsmocr::ERR_INTER_FORMAT;
    }

    int ret_code = recg.recognize_math_expression_in_image(rotated_image, -1, loc_ret_formula,
                recg_ret_latex, recg_ret_symbols);
    DEBUG_LOG("recognize_math_expression_in_image, return: %d", ret_code);
    cvReleaseImage(&rotated_image);

    if (ret_code == mathocr::MathRecognizor::MATH_RECOG_ERR)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_RECG);
        FATAL_LOG("mathocr recg error!");
        return nsmocr::ERR_RECG;
    }

    nsmocr::recg_result_t recg_ret;
    if (nsmocr::CFormulaOCRConvert::convert_output(recg_ret_latex, recg_ret_symbols, recg_ret))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("convert_output error!");
        return nsmocr::ERR_INTER_FORMAT;
    }

    proc_loc.rotate_recg_results(&recg_ret);
    get_json_ret(_init_arg.databuf, 
            recg_ret, _src_img, time_used.get_time());

    return 0;
}

int CProc::_loc_recg_with_formula()
{
    nscommon::time_elapsed_t time_used(__FUNCTION__, 1);

    loc_init_t loc_init_arg;
    loc_init_arg.copy(_init_arg);

    CRcnnLoc proc_loc;
    if (proc_loc.init(&loc_init_arg))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_NO_MEM);
        WARNING_LOG("logid:%d loc init error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_NO_MEM;
    }

    int ret = proc_loc.locate(_src_img);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("logid:%d locate error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_LOC;
    }

    loc_result_t* loc_result = proc_loc.get_loc_result();
   
    // get single char, mser and image
    vector<rcnnchar::Box> mser_boxes_fastrcnn;
    vector<rcnnchar::BoxWithMserType> boxes_with_mser_type;
    cv::Mat img  = proc_loc.get_image();
    proc_loc.get_original_mser(mser_boxes_fastrcnn);
    proc_loc.get_nonms_boxes(boxes_with_mser_type);

    // formula ocr start
    IplImage ipl_img(img);
    IplImage* gray_image = NULL; 
    if (ipl_img.nChannels == 3)
    {
        gray_image = cvCreateImage(cvGetSize(&ipl_img), (&ipl_img)->depth, 1);
        cvCvtColor(&ipl_img, gray_image, CV_RGB2GRAY);
    }
    else
    {
        gray_image = cvCloneImage(&ipl_img);
    }

    mathocr::DetectRecgRequest request;
    mathocr::DetectRecgResponse response;
//    int formula_wait_id = formula_process_start(gray_image, 
//                                                mser_boxes_fastrcnn,
//                                                boxes_with_mser_type,
//                                                request,
//                                                response);
    pthread_t formula_thread_id = formula_process(gray_image,
                                                  mser_boxes_fastrcnn,
                                                  boxes_with_mser_type,
                                                  request,
                                                  response);

    // normal procedure of recognition
    CProcRecg proc_recg;
    recg_result_t* recg_result = NULL;
    do {
        recg_init_t recg_init_arg;
        recg_init_arg.copy(_init_arg);

        ret = proc_recg.init(&recg_init_arg);
        if (ret)
        {
            break;
        }

        //recg_result_t recg_result;
        ret = proc_recg.recg(*loc_result);
        if (ret)
        {
            break;
        }

        recg_result = proc_recg.get_recg_result();
        ret = proc_loc.post_act(recg_result);
        //recg_result->print_info();
        DEBUG_LOG("END POST ACT!"); 

        /*init the detected obj type
        std::string none_type = "-1";
        recg_result->img_info.detected_obj_type = none_type;*/
        if (ret)
        {
            break;
        }
    } while (false);

    // formula ocr wait
    int formula_result_size = 0;
    recg_result_t formula_result;
    recg_result_t latex_result;
    if (g_conf._with_latex_result > 0) {
//        formula_process_wait(formula_wait_id, formula_result_size, response, \
//                &formula_result, &latex_result);
        formula_process_result(formula_thread_id, response, &formula_result, &latex_result);
    } else {
//        formula_process_wait(formula_wait_id, formula_result_size, response, &formula_result);
        formula_process_result(formula_thread_id, response, &formula_result);
    }

    // release gray image
    if (gray_image)
    {
        cvReleaseImage(&gray_image);
        gray_image = NULL;
    }

    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, ret);
        WARNING_LOG("logid:%d _loc_recg_with_formula process error!", 
            CQueryData::get_logid(_init_arg.databuf));
        return ret;
    }

    // post process
    proc_loc.rotate_recg_results(&formula_result);
    _first_std_sort(recg_result->regions);

    // merge result
    recg_result_t all;
    if (g_conf._result_merge > 0) {
        merge_math_formula_result(formula_result, *recg_result, all);
    } else {
        all = formula_result;
        all.add(*recg_result);
    }

    //add latex result
    if (!latex_result.regions.empty()) {
        all.add(latex_result);
    }

    all.set_direction();

    //std::sort(all.regions.begin(),
    //        all.regions.end(), sort_regions);

    _first_std_sort(all.regions);
    _quick_sort(all.regions, sort_regions);

    //TODO:get json by user's parameter
    get_json_ret(_init_arg.databuf, 
            all, _src_img, time_used.get_time());
    return 0;
}

int CProc::_loc_recg()
{
    nscommon::time_elapsed_t time_used(__FUNCTION__, 1);
    std::string lang_type = CQueryData::get_lang_type(_init_arg.databuf);

    loc_init_t loc_init_arg;
    loc_init_arg.copy(_init_arg);

    CProcLoc proc_loc;
    if (proc_loc.init(&loc_init_arg))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_NO_MEM);
        WARNING_LOG("log:%d proc_loc init error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_NO_MEM;
    }

    DEBUG_LOG("proc_loc init ok!");

    int ret = proc_loc.locate(_src_img);
    if (ret) {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d locate error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_LOC;
    }
    loc_result_t* loc_result = proc_loc.get_loc_result();
    // language category classify
    std::vector<lang_prob_t> lang_prob_list;
    recg_init_t recg_init_arg;
    recg_init_arg.copy(_init_arg);

    /*bool use_overall_table_model = CQueryData::is_get_lang_prob(_init_arg.databuf);
    bool on_off = false;
    if (use_overall_table_model) {
        langcateclassify::swift_overall_table_model(recg_init_arg, on_off);
    }*/

    CProcRecg proc_recg;
    ret = proc_recg.init(&recg_init_arg);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_NO_MEM);
        WARNING_LOG("log:%d recg init error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_NO_MEM;
    }

    ret = proc_recg.recg(*loc_result);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_RECG);
        WARNING_LOG("log:%d recg error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_RECG;
    }

    DEBUG_LOG("GET recognition!");
    recg_result_t* recg_result = proc_recg.get_recg_result();
    DEBUG_LOG("END GET recognition!");

    /*on_off = true;
    if (use_overall_table_model) {
        langcateclassify::swift_overall_table_model(recg_init_arg, on_off);
    }*/

    int classify_lang_cate = -1;
    /*if (CQueryData::is_get_lang_prob(_init_arg.databuf)) {
        std::vector<linegenerator::TextLineDetectionResult> text_lines;
        if (proc_loc.get_detector()->get_name() == "rcnn_loc") {
            reinterpret_cast<CRcnnLoc*>(proc_loc.get_detector())->get_line_result(text_lines);
            langcateclassify::calc_lang_cate_prob(_init_arg, _src_img, recg_init_arg, loc_result, \
                    classify_lang_cate, text_lines, recg_result, &_lock);
            recg_result->classify_lang_cate = classify_lang_cate;
            // attach language probability result, not support right now
            if (CQueryData::is_auto_detect_lang_type(_init_arg.databuf) \
                    || CQueryData::is_get_lang_prob(_init_arg.databuf)) {
                recg_result->lang_prob_map = lang_prob_list;
                recg_result->classify_lang_cate = classify_lang_cate;
            }
        }
    }*/

    if (CQueryData::is_get_lang_prob(_init_arg.databuf)) {
        std::vector<linegenerator::TextLineDetectionResult> text_lines;
        if (proc_loc.get_detector()->get_name() == "rcnn_loc") {
            reinterpret_cast<CRcnnLoc*>(proc_loc.get_detector())->get_line_result(text_lines);
            langcateclassify::calc_lang_cate_prob(_init_arg, _src_img, \
                recg_init_arg, proc_loc, loc_result, \
                classify_lang_cate, text_lines, recg_result);
            recg_result->classify_lang_cate = classify_lang_cate;
        }
    }

    if (CQueryData::is_filter_unreliable_recog_lines(_init_arg.databuf))
    {
        if (CQueryData::is_utf8(_init_arg.databuf))
        {
            DEBUG_LOG("filter_unreliable_recog_line.");
            filter_unreliable_recog_line_utf8(*recg_result);
        }
        else
        {
            WARNING_LOG("filter_unreliable_recog_line only support UTF8 encoding.");
        }
    }

    DEBUG_LOG("START POST ACT!");
    ret = proc_loc.post_act(recg_result);
    //recg_result->print_info();
    DEBUG_LOG("END POST ACT!");
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d post act error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_LOC;
    }

    recg_result->set_direction();
    _first_std_sort(recg_result->regions);
    _quick_sort(recg_result->regions, sort_regions);
    // for baidu translation menu	
    if (CQueryData::is_bdtrans_menu_chn(_init_arg.databuf)) {
        nsmocr::CMenuConvert::reform_line_recg_rst(*recg_result);
    }

    DEBUG_LOG("start of raw recog result.");
    recg_result->print_info();
    DEBUG_LOG("end of raw recog result.");

    //detected obj type
    if (CQueryData::is_detected_lottery(_init_arg.databuf)) {
        DEBUG_LOG("start of find lottery item.");
        std::string lottery_type = "-1";
        if (get_detected_lottery(recg_result, loc_result, lottery_type) == 0) {
            recg_result->img_info.detected_obj_type = lottery_type; 
            nsmocr::obj_type_t lottery_obj_type;
            lottery_obj_type.obj = "lottery";
            lottery_obj_type.result = lottery_type;
            recg_result->detected_obj_type_list.push_back(lottery_obj_type);
        }
        DEBUG_LOG("end of find lottery item.");
    }

    // business license
    if (CQueryData::is_business_license(_init_arg.databuf))
    {
        tmplocr::BDOCR_Result  recog_result_for_tmplocr;
        if (nsmocr::CTemplOCRConvert::convert_input(*recg_result, recog_result_for_tmplocr,
                                                     _image_width, _image_height))
        {
            nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
            WARNING_LOG("CTemplOCRConvert convert input error!");
            return nsmocr::ERR_INTER_FORMAT;
        }

        tmplocr::BusinessLicenseResult business_license_result;
        recognize_business_license(recog_result_for_tmplocr, 
                                   *(_init_arg.model->_m_business_license_model),
                                   business_license_result);

        if (ret != 0)
        {
            nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_RECG);
            WARNING_LOG("Recognize business license Failed!");
            return nsmocr::ERR_RECG;
        }

        if (nsmocr::CTemplOCRConvert::convert_output(business_license_result, *recg_result))
        {
            nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
            WARNING_LOG("CTemplOCRConvert convert output error!");
            return nsmocr::ERR_INTER_FORMAT;
        }
    }

    // lottery
    if (CQueryData::is_lottery(_init_arg.databuf)) {
        std::string chn_lang = "CHN_ENG";
        if (lang_type.length() > 0 && strcmp(lang_type.c_str(), chn_lang.c_str()) != 0) {
            WARNING_LOG("Lottery Language Type Error!\n");
            return -1;
        }
        std::string lottery_table = "./data/lottery_data/lottery_table";
        nsmocr::CLotteryPostProc lottery_post_proc;
        lottery_post_proc.read_lottery_table(lottery_table, 10800);

        lottery_post_proc.lottery_replace_with_table(*recg_result);
        lottery_post_proc.lottery_replace_eng_char_with_cand_num(*recg_result);
        //lottery_post_proc.lottery_add_special_string(*recg_result);
        lottery_post_proc.lottery_replace_special_string(*recg_result);

        //for 3d process
        /*int lottery_type = lottery_post_proc.lottery_check_type(*recg_result);
        if (lottery_type == 2){
            lottery_post_proc.lottery_3d_post_proc(*recg_result);
        }*/
    }

    if (CQueryData::is_mask(_init_arg.databuf))
    {
        recg_result_t mask_result;
        if (CMask::get_mask_result(*recg_result, mask_result, _init_arg.databuf, _src_img))
        {
            nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_MASK);
            WARNING_LOG("get mask error!\n");
            return ERR_MASK;
        }

        get_json_ret(_init_arg.databuf, 
                mask_result, _src_img, time_used.get_time());
    } else {
        if (CQueryData::is_anti_spam(_init_arg.databuf))
        {
            CLocBase* detector = proc_loc.get_detector();
            if (detector->get_name() == "rcnn_loc")
            {
                loc_result_t* loc_result = ((CRcnnLoc*)detector)->get_loc_result_in_query_image();
                recg_result_t rec_result(*loc_result);
                rec_result.set_direction();
                proc_loc.post_act(&rec_result);

                CRcnnLoc loc_tmp;
                loc_tmp.del_blank_results(recg_result);
                get_json_ret(_init_arg.databuf, *recg_result, _src_img, 
                        time_used.get_time(), &rec_result);
                return 0;
            }
        }
        get_json_ret(_init_arg.databuf, *recg_result, _src_img, time_used.get_time());
    }

    return 0;
}
 
int CProc::_loc_recg_driving_license()
{
    nscommon::time_elapsed_t time_used(__FUNCTION__, 1);

    // Driving License Step 1: Template Matching
    nsmocr::CQueryData::set_driving_license_step_one(_init_arg.databuf);
    
    // step 1.1 fast detetion + recognition for template matching
    loc_init_t loc_init_arg_s1;
    loc_init_arg_s1.copy(_init_arg);
    CProcLoc proc_loc_s1;
    if (proc_loc_s1.init(&loc_init_arg_s1))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d proc_loc_s1 init error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }

    int ret = proc_loc_s1.locate(_src_img);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d locate_s1 error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }
    loc_result_t* loc_result_s1 = proc_loc_s1.get_loc_result();
    
    int orig_dir = loc_result_s1->img_info.img_dir;
    DEBUG_LOG("origin direction %d", orig_dir);
    
    recg_init_t recg_init_arg_s1;
    recg_init_arg_s1.copy(_init_arg);

    CProcRecg proc_recg_s1;
    ret = proc_recg_s1.init(&recg_init_arg_s1);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_RECG);
        WARNING_LOG("log:%d recg_s1 init error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }

    ret = proc_recg_s1.recg(*loc_result_s1);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_RECG);
        WARNING_LOG("log:%d recg_s1 error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }

    recg_result_t* recg_result_s1 = proc_recg_s1.get_recg_result();
    ret = proc_loc_s1.post_act(recg_result_s1);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d post act s1 error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }
    recg_result_s1->set_direction();
    _first_std_sort(recg_result_s1->regions);

    DEBUG_LOG("step1 result start");
    recg_result_s1->print_info();
    DEBUG_LOG("step1 result end");

    // Warp Result according to template match.
    tmplocr::BDOCR_Result  recog_result_s1_for_tmplocr;
    if (nsmocr::CTemplOCRConvert::convert_input(*recg_result_s1, recog_result_s1_for_tmplocr,
                                                 _image_width, _image_height))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("CTemplOCRConvert convert input s1 error!");
        return nsmocr::ERR_INTER_FORMAT;
    }

    DEBUG_LOG("rotate results");
    tmplocr::rotate_license_bdocr_result(recog_result_s1_for_tmplocr, 0);
    
    // step 1.2 find template to match
    tmplocr::BDOCR_Struct_Result tmpl_result_s1;
    int  chosen_template_id = -1;
    ret = tmplocr::driving_license_step_one(recog_result_s1_for_tmplocr,
                                      *(_init_arg.model->_m_driving_license_templ_vec),
                                      tmpl_result_s1,
                                      chosen_template_id
                                      );

    DEBUG_LOG("template_id %d", chosen_template_id);
    if (ret || chosen_template_id < 0)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("log:%d driving_license_step one failed!", 
                    CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }
    tmplocr::BDOCR_Template& chosen_template = 
                             (*(_init_arg.model->_m_driving_license_templ_vec))[chosen_template_id];

    DEBUG_LOG("rotate image up");
    IplImage *_src_img_rotate = nsmocr::rotate_image2up_by_dir(_src_img, orig_dir);
    
    if (_src_img_rotate == NULL)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("rotate_image error!");
        return nsmocr::ERR_INTER_FORMAT;
    }
    // Warp Image According to Template Matching Result.
    // step 2.1 warp image to recog.
    cv::Mat query_img_mat(_src_img_rotate, true);
    cvReleaseImage(&_src_img_rotate);
    
    cv::Mat rectified_img_mat;
    cv::Mat warp_mat = cv::Mat(3, 3, CV_64FC1);
  
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            warp_mat.at<double>(i, j) = tmpl_result_s1.warp_mat[i * 3 + j];
        }
    }

    cv::warpPerspective(query_img_mat, rectified_img_mat, 
                    warp_mat, 
                    cv::Size(chosen_template.templ_width, chosen_template.templ_height), \
                    cv::INTER_LINEAR, cv::BORDER_CONSTANT);
    IplImage rectified_img_iplimage(rectified_img_mat);

    // Driving License Step Two.
    // step 2.2 detect and recog. warped image 
    nsmocr::CQueryData::set_driving_license_step_two(_init_arg.databuf);
    loc_init_t loc_init_arg_s2;
    loc_init_arg_s2.copy(_init_arg);
    CProcLoc proc_loc_s2;
    if (proc_loc_s2.init(&loc_init_arg_s2))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d proc_loc_s2 init error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }

    ret = proc_loc_s2.locate(&rectified_img_iplimage);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d locate_s2 error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }

    loc_result_t* loc_result_s2 = proc_loc_s2.get_loc_result();

    recg_init_t recg_init_arg_s2;
    recg_init_arg_s2.copy(_init_arg);
    CProcRecg proc_recg_s2;
    ret = proc_recg_s2.init(&recg_init_arg_s2);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_RECG);
        WARNING_LOG("log:%d recg_s2 init error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }

    ret = proc_recg_s2.recg(*loc_result_s2);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_RECG);
        WARNING_LOG("log:%d recg_s2 error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }
    
    recg_result_t* recg_result_s2 = proc_recg_s2.get_recg_result();
    ret = proc_loc_s2.post_act(recg_result_s2);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d post act s2 error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }
    recg_result_s2->set_direction();

    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_RECG);
        WARNING_LOG("log:%d _resize_recg_result error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }
    _first_std_sort(recg_result_s2->regions);

    DEBUG_LOG("step2 result start");
    recg_result_s2->print_info();
    DEBUG_LOG("step2 result end");

    // step 2.3 get results by structure, post-processing
    tmplocr::BDOCR_Result recog_result_for_tmplocr_s2;
    if (nsmocr::CTemplOCRConvert::convert_input(*recg_result_s2, recog_result_for_tmplocr_s2,
                                                 rectified_img_iplimage.width,
                                                 rectified_img_iplimage.height))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("log:%d CTemplOCRConvert convert input error!", 
                            CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_INTER_FORMAT;
    }

    DEBUG_LOG("driving_license_step_two start");
    DEBUG_LOG("chosen_template: %s", chosen_template.name.c_str());
    
    tmplocr::Idcard_Struct_Result driving_license_result;
    ret = tmplocr::driving_license_step_two(recog_result_for_tmplocr_s2,
                                      chosen_template,
                                      driving_license_result
                                      );
    DEBUG_LOG("driving_license_step_two end");

    if (ret != 0)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("log:%d driving_license_step_two failed!", 
                        CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_ID_CARD;
    }
    // step 3. name align and post-recog. 
    // input: cv::Mat warped_img
    // output: Idcard_Struct_Result & driving_license_result

    DEBUG_LOG("driving_license_step_three start");
    
    nsmocr::CDrivingLicenseProc driv_lic_recognizer(_init_arg);

#pragma omp parallel sections num_threads(2)
{
#pragma omp section
    {
        int ret1 = driv_lic_recognizer.align_recog_name(rectified_img_mat, driving_license_result);
    }    
#pragma omp section
    {
        int ret2 = driv_lic_recognizer.align_recog_num(rectified_img_mat, driving_license_result);
    }
}
    DEBUG_LOG("driving_license num recog. end");
    
    ret = tmplocr::driving_lic_post_processing(chosen_template, driving_license_result);
    DEBUG_LOG("driving_license post processing end");

    if (ret != 0)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("log:%d driving license step.3 failed!",
                    CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_ID_CARD;
    }
    DEBUG_LOG("driving_license_step_three end");
    
    recg_result_t recg_result_dl_struct;
    if (nsmocr::CTemplOCRConvert::convert_output(driving_license_result, recg_result_dl_struct))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("log:%d CTemplOCRConvert convert output error!",
                            CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_INTER_FORMAT;
    }

    ret = _resize_recg_result(recg_result_dl_struct);
    recg_result_dl_struct.set_direction();

    nsmocr::CQueryData::set_driving_license(_init_arg.databuf);
    
    // == warp rotate result back == //
    tmplocr::Idcard_Struct_Result dl_res_rotate;
    cv::Mat warp_mat_inv = warp_mat.inv();
    tmplocr::warp_license_struct_result(driving_license_result, warp_mat_inv, dl_res_rotate);

    DEBUG_LOG("driving license step.3 dir =  %d", dl_res_rotate.img_dir);
    if (orig_dir > 0){
        tmplocr::rotate_license_struct_result(dl_res_rotate, orig_dir);
    }

    if (nsmocr::CTemplOCRConvert::convert_output(dl_res_rotate, *recg_result_s1))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("log:%d CTemplOCRConvert convert output error!",
                              CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_INTER_FORMAT;
    }
    
    get_json_ret(_init_arg.databuf, 
                 *recg_result_s1, _src_img, time_used.get_time());

    return 0;
}

int CProc::_loc_recg_vehicle_license()
{
    nscommon::time_elapsed_t time_used(__FUNCTION__, 1);

    // Driving License Step 1: Template Matching
    nsmocr::CQueryData::set_vehicle_license_step_one(_init_arg.databuf);

    // step 1.1 fast detetion + recognition for template matching
    loc_init_t loc_init_arg_s1;
    loc_init_arg_s1.copy(_init_arg);
    CProcLoc proc_loc_s1;
    if (proc_loc_s1.init(&loc_init_arg_s1))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d proc_loc_s1 init error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }

    int ret = proc_loc_s1.locate(_src_img);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d locate_s1 error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }
    loc_result_t* loc_result_s1 = proc_loc_s1.get_loc_result();

    int orig_dir = loc_result_s1->img_info.img_dir;
    DEBUG_LOG("Origin direction %d", orig_dir);

    recg_init_t recg_init_arg_s1;
    recg_init_arg_s1.copy(_init_arg);

    CProcRecg proc_recg_s1;
    ret = proc_recg_s1.init(&recg_init_arg_s1);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_RECG);
        WARNING_LOG("log:%d recg_s1 init error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }

    ret = proc_recg_s1.recg(*loc_result_s1);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_RECG);
        WARNING_LOG("log:%d recg_s1 error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }

    recg_result_t* recg_result_s1 = proc_recg_s1.get_recg_result();
    ret = proc_loc_s1.post_act(recg_result_s1);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d post act s1 error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }
    recg_result_s1->set_direction();
    _first_std_sort(recg_result_s1->regions);

    DEBUG_LOG("step1 result start");
    recg_result_s1->print_info();
    DEBUG_LOG("step1 result end");

    // Warp Result according to template match.
    tmplocr::BDOCR_Result  recog_result_s1_for_tmplocr;
    if (nsmocr::CTemplOCRConvert::convert_input(*recg_result_s1, recog_result_s1_for_tmplocr,
                                                 _image_width, _image_height))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("CTemplOCRConvert convert input s1 error!");
        return nsmocr::ERR_INTER_FORMAT;
    }

    // == qxm : rotate result == //
    DEBUG_LOG("Rotate results");
    tmplocr::rotate_license_bdocr_result(recog_result_s1_for_tmplocr, 0);

    // step 1.2 find template to match
    tmplocr::BDOCR_Struct_Result tmpl_result_s1;
    int  chosen_template_id = -1;
    ret = tmplocr::driving_license_step_one(recog_result_s1_for_tmplocr,
                                      *(_init_arg.model->_m_vehicle_license_templ_vec),
                                      tmpl_result_s1,
                                      chosen_template_id
                                      );

    DEBUG_LOG("template_id %d", chosen_template_id);
    if (ret || chosen_template_id < 0)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("log:%d vehicle_license_step one failed!", 
                    CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }
    tmplocr::BDOCR_Template& chosen_template = 
                             (*(_init_arg.model->_m_vehicle_license_templ_vec))[chosen_template_id];

    // == qxm : rotate image == //
    DEBUG_LOG("Rotate image");
    IplImage *_src_img_rotate = nsmocr::rotate_image2up_by_dir(_src_img, orig_dir);

    if (_src_img_rotate == NULL)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("rotate_image error!");
        return nsmocr::ERR_INTER_FORMAT;
    }
    // == qxm == //
    
    // Warp Image According to Template Matching Result.
    // step 2.1 warp image to recog.
    cv::Mat query_img_mat(_src_img_rotate, true);
    //cv::Mat query_img_mat(_src_img, true);
    cv::Mat rectified_img_mat;
    cv::Mat warp_mat = cv::Mat(3, 3, CV_64FC1);

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            warp_mat.at<double>(i, j) = tmpl_result_s1.warp_mat[i * 3 + j];
        }
    }

    cv::warpPerspective(query_img_mat, rectified_img_mat, 
                    warp_mat, 
                    cv::Size(chosen_template.templ_width, chosen_template.templ_height), \
                    cv::INTER_LINEAR, cv::BORDER_CONSTANT);
    IplImage rectified_img_iplimage(rectified_img_mat);

    // == qxm release rotated_image == //
    cvReleaseImage(&_src_img_rotate);

    // Driving License Step Two.
    // step 2.2 detect and recog. warped image 
    nsmocr::CQueryData::set_vehicle_license_step_two(_init_arg.databuf);
    loc_init_t loc_init_arg_s2;
    loc_init_arg_s2.copy(_init_arg);
    CProcLoc proc_loc_s2;
    if (proc_loc_s2.init(&loc_init_arg_s2))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d proc_loc_s2 init error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }

    ret = proc_loc_s2.locate(&rectified_img_iplimage);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d locate_s2 error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }

    loc_result_t* loc_result_s2 = proc_loc_s2.get_loc_result();

    recg_init_t recg_init_arg_s2;
    recg_init_arg_s2.copy(_init_arg);
    CProcRecg proc_recg_s2;
    ret = proc_recg_s2.init(&recg_init_arg_s2);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_RECG);
        WARNING_LOG("log:%d recg_s2 init error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }

    ret = proc_recg_s2.recg(*loc_result_s2);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_RECG);
        WARNING_LOG("log:%d recg_s2 error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }
    
    recg_result_t* recg_result_s2 = proc_recg_s2.get_recg_result();
    ret = proc_loc_s2.post_act(recg_result_s2);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d post act s2 error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }
    recg_result_s2->set_direction();

    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_RECG);
        WARNING_LOG("log:%d _resize_recg_result error!", CQueryData::get_logid(_init_arg.databuf));
        return -1;
    }
    _first_std_sort(recg_result_s2->regions);

    DEBUG_LOG("step2 result start");
    recg_result_s2->print_info();
    DEBUG_LOG("step2 result end");

    // step 2.3 get results by structure, post-processing
    tmplocr::BDOCR_Result recog_result_for_tmplocr_s2;
    if (nsmocr::CTemplOCRConvert::convert_input(*recg_result_s2, recog_result_for_tmplocr_s2,
                                                 rectified_img_iplimage.width,
                                                 rectified_img_iplimage.height))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("log:%d CTemplOCRConvert convert input error!", 
                            CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_INTER_FORMAT;
    }

    DEBUG_LOG("vehicle_license_step_two start");
    DEBUG_LOG("chosen_template: %s", chosen_template.name.c_str());
    
    tmplocr::Idcard_Struct_Result vehicle_license_result;
    ret = tmplocr::vehicle_license_step_two(recog_result_for_tmplocr_s2,
                                            chosen_template,
                                            vehicle_license_result);
    DEBUG_LOG("vehicle_license_step_two end");

    if (ret != 0)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("log:%d vehicle_license_step_two failed!", 
                        CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_ID_CARD;
    }

    // step 3. num. align and post-recog. 
    // input: cv::Mat warped_img
    // output: Idcard_Struct_Result & vehicle_license_result

    DEBUG_LOG("vehicle_license_step_three start");
    nsmocr::CQueryData::set_vehicle_license_step_three(_init_arg.databuf);
    
    DEBUG_LOG("vehicle_license fields recog. start");
    nsmocr::CVehicleLicenseProc vehicle_lic_recognizer(_init_arg);
    
    //std::cout << "step3.1 ret = " << ret << std::endl; 
    
    tmplocr::BDOCR_LineInfo vin_line;
    //tmplocr::BDOCR_LineInfo engine_line;
    //tmplocr::BDOCR_LineInfo plate_line;
    //tmplocr::BDOCR_LineInfo brand_line;
   
#pragma omp parallel sections num_threads(4)
{
        
#pragma omp section
        {
            // vin. aligned recog. 
            if (true == vehicle_lic_recognizer.get_vin_line_info(vehicle_license_result, 
                                                                vin_line)) {
                
                int ret1 = vehicle_lic_recognizer.align_recog_vin(rectified_img_mat, &vin_line);
                //int ret1 = vehicle_lic_recognizer.align_recog_raw(rectified_img_mat, vehicle_license_result);
            }else{
                WARNING_LOG("get_vin_line:  vin result empty");
            }
        }
        
#pragma omp section
        {
            // engine aligned recog.
            //int ret2 = vehicle_lic_recognizer.get_engine_line_info(vehicle_license_result, engine_line);
            int ret2 = vehicle_lic_recognizer.align_recog_engine(rectified_img_mat, 
                                                                vehicle_license_result);
        }
        
#pragma omp section
        {
            // plate aligned recog.
            //int ret3 = vehicle_lic_recognizer.get_plate_line_info(vehicle_license_result, plate_line);
            int ret3 = vehicle_lic_recognizer.align_recog_plate(rectified_img_mat, 
                                                                vehicle_license_result);
        }
        
#pragma omp section
        {
            // brand aligned recog. 
            //int ret4 = vehicle_lic_recognizer.get_brand_line_info(vehicle_license_result, brand_line);
            int ret4 = vehicle_lic_recognizer.align_recog_brand(rectified_img_mat, 
                                                                vehicle_license_result);
        }
}
    
    vehicle_lic_recognizer.save_vin_result(vin_line, vehicle_license_result);
    //vehicle_lic_recognizer.save_engine_result(engine_line, vehicle_license_result);
    //vehicle_lic_recognizer.save_plate_result(plate_line, vehicle_license_result);
    //vehicle_lic_recognizer.save_brand_result(brand_line, vehicle_license_result);
    
    DEBUG_LOG("vehicle_license fields recog. end");
    
    //std::cout << "step3.2 ret = " << ret << std::endl; 
    
    ret = tmplocr::vehicle_lic_post_processing(chosen_template, vehicle_license_result);
    DEBUG_LOG("vehicle_license post processing end");
    //std::cout << "step3.3 ret = " << ret << std::endl; 
    
    if (ret != 0)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("log:%d vehicle license step.3 failed!",
                    CQueryData::get_logid(_init_arg.databuf));
    }
    DEBUG_LOG("vehicle_license_step_three end");
    
    recg_result_t recg_result_vl_struct;
    if (nsmocr::CTemplOCRConvert::convert_output(vehicle_license_result, recg_result_vl_struct))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("log:%d CTemplOCRConvert convert output error!",
                            CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_INTER_FORMAT;
    }

    ret = _resize_recg_result(recg_result_vl_struct);
    recg_result_vl_struct.set_direction();

    nsmocr::CQueryData::set_vehicle_license(_init_arg.databuf);

    // == warp rotate result back == //
    tmplocr::Idcard_Struct_Result vl_res_rotate;
    cv::Mat warp_mat_inv = warp_mat.inv();
    tmplocr::warp_license_struct_result(vehicle_license_result, warp_mat_inv, vl_res_rotate);

    DEBUG_LOG("qxm_test vl step 3 dir %d", vl_res_rotate.img_dir);
    if (orig_dir > 0)
    {
        tmplocr::rotate_license_struct_result(vl_res_rotate, orig_dir);
    }
    
    if (nsmocr::CTemplOCRConvert::convert_output(vl_res_rotate, *recg_result_s1))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("log:%d CTemplOCRConvert convert output error!",
                            CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_INTER_FORMAT;
    }
    get_json_ret(_init_arg.databuf, 
                 *recg_result_s1, _src_img, time_used.get_time());
    /*
    // TODO: warp recg_result_final back
    recg_result_t recg_result_final;
    cv::Mat warp_mat_inv = warp_mat.inv();
    warp_recog_result(recg_result_vl_struct, warp_mat_inv, recg_result_final);
    
    recg_result_final.img_info.img_dir = orig_dir;
    recg_result_final.set_direction();
    get_json_ret(_init_arg.databuf, 
                 recg_result_final, _src_img, time_used.get_time());
                 */

    return 0;
}

int CProc::_loc_recg_medical_model_fusion()
{
    nscommon::time_elapsed_t time_used(__FUNCTION__, 1);
    DEBUG_LOG("medical models fusion !");

    //== model one : locate and recognition == //
    nsmocr::CQueryData::set_medical_fusion_model_one(_init_arg.databuf);

    DEBUG_LOG("Model one : begin of recognition!"); 
    loc_init_t loc_init_arg_m1;
    loc_init_arg_m1.copy(_init_arg);

    CProcLoc proc_loc_m1;
    if (proc_loc_m1.init(&loc_init_arg_m1))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_NO_MEM);
        WARNING_LOG("log:%d proc_loc init error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_NO_MEM;
    }

    DEBUG_LOG("Model one proc_loc init ok!");

    int ret = proc_loc_m1.locate(_src_img);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d locate error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_LOC;
    }

    loc_result_t* loc_result_m1 = proc_loc_m1.get_loc_result();

    recg_init_t recg_init_arg_m1;
    recg_init_arg_m1.copy(_init_arg);

    CProcRecg proc_recg_m1;
    ret = proc_recg_m1.init(&recg_init_arg_m1);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_NO_MEM);
        WARNING_LOG("log:%d recg init error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_NO_MEM;
    }

    ret = proc_recg_m1.recg(*loc_result_m1);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_RECG);
        WARNING_LOG("log:%d recg error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_RECG;
    }

    DEBUG_LOG("GET recognition!"); 
    recg_result_t* recg_result_m1 = proc_recg_m1.get_recg_result();
    DEBUG_LOG("END GET recognition!"); 

    DEBUG_LOG("START POST ACT!");
    ret = proc_loc_m1.post_act(recg_result_m1);
    recg_result_m1->print_info();
    DEBUG_LOG("END POST ACT!");
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d post act error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_LOC;
    }

    recg_result_m1->set_direction();
    _first_std_sort(recg_result_m1->regions);
    _quick_sort(recg_result_m1->regions, sort_regions);
    DEBUG_LOG("Model one : end of recognition!"); 
    
    //== model two : recognition, use the locate result == // 
    //== in model one == //
    nsmocr::CQueryData::set_medical_fusion_model_two(_init_arg.databuf);

    loc_init_t loc_init_arg_m2;
    loc_init_arg_m2.copy(_init_arg);

    CProcLoc proc_loc_m2;
    if (proc_loc_m2.init(&loc_init_arg_m2))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_NO_MEM);
        WARNING_LOG("log:%d proc_loc init error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_NO_MEM;
    }

    DEBUG_LOG("Model one proc_loc init ok!");

    ret = proc_loc_m2.locate(_src_img);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d locate error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_LOC;
    }

    loc_result_t* loc_result_m2 = proc_loc_m2.get_loc_result();

    DEBUG_LOG("Model two : begin of recognition!"); 
    recg_init_t recg_init_arg_m2;
    recg_init_arg_m2.copy(_init_arg);

    CProcRecg proc_recg_m2;
    ret = proc_recg_m2.init(&recg_init_arg_m2);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_NO_MEM);
        WARNING_LOG("log:%d recg init error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_NO_MEM;
    }

    ret = proc_recg_m2.recg(*loc_result_m2);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_RECG);
        WARNING_LOG("log:%d recg error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_RECG;
    }

    DEBUG_LOG("Model two : GET recognition!"); 
    recg_result_t* recg_result_m2 = proc_recg_m2.get_recg_result();
    DEBUG_LOG("Model two : END GET recognition!"); 

    DEBUG_LOG("Model two : START POST ACT!");
    ret = proc_loc_m2.post_act(recg_result_m2);
    recg_result_m2->print_info();
    DEBUG_LOG("Model two : END POST ACT!");
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d post act error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_LOC;
    }

    recg_result_m2->set_direction();
    _first_std_sort(recg_result_m2->regions);
    _quick_sort(recg_result_m2->regions, sort_regions);
    //DEBUG_LOG("start of raw recog result.");
    //recg_result_m1->print_info();
    //DEBUG_LOG("end of raw recog result.");
    DEBUG_LOG("Model two : end of recognition!"); 
    
    //== model three : recognition, use the locate result == // 
    //== in model one == //
    nsmocr::CQueryData::set_medical_fusion_model_three(_init_arg.databuf);

    DEBUG_LOG("Model three : begin of recognition!"); 
    recg_init_t recg_init_arg_m3;
    recg_init_arg_m3.copy(_init_arg);

    CProcRecg proc_recg_m3;
    ret = proc_recg_m3.init(&recg_init_arg_m3);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_NO_MEM);
        WARNING_LOG("log:%d recg init error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_NO_MEM;
    }

    ret = proc_recg_m3.recg(*loc_result_m2);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_RECG);
        WARNING_LOG("log:%d recg error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_RECG;
    }

    DEBUG_LOG("Model three : GET recognition!"); 
    recg_result_t* recg_result_m3 = proc_recg_m3.get_recg_result();
    DEBUG_LOG("Model three : END GET recognition!"); 

    DEBUG_LOG("Model three : START POST ACT!");
    ret = proc_loc_m2.post_act(recg_result_m3);
    recg_result_m3->print_info();
    DEBUG_LOG("Model three : END POST ACT!");
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d post act error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_LOC;
    }

    recg_result_m3->set_direction();
    _first_std_sort(recg_result_m3->regions);
    _quick_sort(recg_result_m3->regions, sort_regions);
    DEBUG_LOG("Model three : end of recognition!"); 

    // == begin to fusion the recognition results == //
    DEBUG_LOG("Begin to fusion !"); 
    std::vector<recg_region_t>::iterator iter_m1;
    std::vector<recg_region_t>::iterator iter_m2;
    std::vector<recg_region_t>::iterator iter_m3;
    for (iter_m1 = recg_result_m1->regions.begin(); iter_m1 != recg_result_m1->regions.end();)
    {
        for (iter_m2 = recg_result_m2->regions.begin(); 
            iter_m2 != recg_result_m2->regions.end(); ++iter_m2)
        {
            if (iter_m1->ocrresult == iter_m2->ocrresult)
            {
                break;
            }
        }
        for (iter_m3 = recg_result_m3->regions.begin(); 
            iter_m3 != recg_result_m3->regions.end(); ++iter_m3)
        {
            if (iter_m1->ocrresult == iter_m3->ocrresult)
            {
                break;
            }
        }
        if (iter_m2 != recg_result_m2->regions.end() && iter_m3 != recg_result_m3->regions.end())
        {
            iter_m1->line_prob.min_prob += 0.3;
            ++iter_m1;
        }
        else if (iter_m2 == recg_result_m2->regions.end() &&
            iter_m3 == recg_result_m3->regions.end())
        {
            iter_m1->line_prob.min_prob -= 0.3;
            ++iter_m1;
            //iter_m1 = recg_result_m1->regions.erase(iter_m1);
        }
        else
        {
            iter_m1->line_prob.min_prob += 0.1;
            ++iter_m1;
        }
    }
    DEBUG_LOG("end to fusion !"); 
    
    // begin to post process == //
    DEBUG_LOG("Begin to Post Process .");
    std::string post_str = "";
    nsmocr::CMedicalFusionPrco medical_post_proc;
    for (iter_m1 = recg_result_m1->regions.begin(); iter_m1 != recg_result_m1->regions.end();)
    {
        if (CQueryData::is_image_property_1(_init_arg.databuf))
        {
            post_str = medical_post_proc.parse_money(iter_m1->ocrresult);
        }
        else if (CQueryData::is_image_property_2(_init_arg.databuf))
        {
            post_str = medical_post_proc.parse_date(iter_m1->ocrresult);
        }
        else if (CQueryData::is_image_property_3(_init_arg.databuf))
        {
            post_str = medical_post_proc.parse_ratio(iter_m1->ocrresult);
        }
        else if (CQueryData::is_image_property_5(_init_arg.databuf))
        {
            post_str = medical_post_proc.parse_program_name(iter_m1->ocrresult);
        }
        else
        {
            post_str = iter_m1->ocrresult;
        }

        if (post_str == "")
        {
            iter_m1 = recg_result_m1->regions.erase(iter_m1);
        }
        else
        {
            iter_m1->ocrresult = post_str;
            ++iter_m1;
        }
    }

    //recg_result_m1->print_info();
    DEBUG_LOG("qxm_test get json res.");
    get_json_ret(_init_arg.databuf, 
            *recg_result_m1, _src_img, time_used.get_time());
    DEBUG_LOG("qxm_test get json res done.");

    return 0;
}

int CProc::_loc_recg_idcard()
{
    nscommon::time_elapsed_t time_used(__FUNCTION__, 1);

    loc_init_t loc_init_arg;
    loc_init_arg.copy(_init_arg);

    CProcLoc proc_loc;
    if (proc_loc.init(&loc_init_arg))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_NO_MEM);
        WARNING_LOG("log:%d proc_loc init error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_NO_MEM;
    }

    DEBUG_LOG("proc_loc init ok!");

    int ret = proc_loc.locate(_src_img);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d locate error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_LOC;
    }

    loc_result_t* loc_result = proc_loc.get_loc_result();

    //loc_result->print_info();

    recg_init_t recg_init_arg;
    recg_init_arg.copy(_init_arg);

    CProcRecg proc_recg;
    ret = proc_recg.init(&recg_init_arg);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_NO_MEM);
        WARNING_LOG("log:%d recg init error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_NO_MEM;
    }

    ret = proc_recg.recg(*loc_result);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_RECG);
        WARNING_LOG("log:%d recg error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_RECG;
    }

    DEBUG_LOG("GET recognition!"); 
    recg_result_t* recg_result = proc_recg.get_recg_result();
    DEBUG_LOG("END GET recognition!"); 
    recg_result->print_info();

    /*init the detected obj type
    std::string none_type = "-1";
    recg_result->img_info.detected_obj_type = none_type;*/

    DEBUG_LOG("START POST ACT!"); 
    ret = proc_loc.post_act(recg_result);
    //recg_result->print_info();
    DEBUG_LOG("END POST ACT!"); 
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d post act error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_LOC;
    }

    recg_result->set_direction();

    ret = _resize_recg_result(*recg_result);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_RECG);
        WARNING_LOG("log:%d _resize_recg_result error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_RECG;
    }

    //std::sort(recg_result->regions.begin(),
    //        recg_result->regions.end(), sort_regions);
    
    _first_std_sort(recg_result->regions);
    _quick_sort(recg_result->regions, sort_regions);

    tmplocr::Idcard_Struct_Result idcard_result;
    tmplocr::BDOCR_Result recog_result_for_tmplocr;
    if (nsmocr::CTemplOCRConvert::convert_input(*recg_result, recog_result_for_tmplocr,
                                                 _image_width, _image_height))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("CTemplOCRConvert convert input error!");
        return nsmocr::ERR_INTER_FORMAT;
    }

    DEBUG_LOG("start of raw recog result.");
    recg_result->print_info();
    DEBUG_LOG("end of raw recog result.");

    if (CQueryData::is_id_card_backside(_init_arg.databuf))
    {
        nscommon::time_elapsed_t time_used_back("struct_analysis idcard_back", 1);
        ret = tmplocr::recognize_idcard_back(recog_result_for_tmplocr, 
                                             idcard_result);

        if (ret == 0 && check_idcard_result_complete(idcard_result))
        {
            recg_result->img_info.img_status = "normal";
        }
        else if (ret != 0)
        {
            tmplocr::Idcard_Struct_Result idcard_reversed_result;
            nscommon::time_elapsed_t time_used_front("struct_analysis idcard_front", 1);
            int ret_reversed = tmplocr::recognize_idcard_front(recog_result_for_tmplocr,
                                                               *(_init_arg.model->_m_id_card_templ),
                                                               idcard_reversed_result);
            if (!check_idcard_result_empty(idcard_reversed_result))
            {
                recg_result->img_info.img_status = "reversed_side";
            }
        }
    }
    else
    {
        nscommon::time_elapsed_t time_used_front("struct_analysis idcard_front", 1);
        ret = tmplocr::recognize_idcard_front(recog_result_for_tmplocr, 
                                              *(_init_arg.model->_m_id_card_templ),
                                              idcard_result);

        if (check_idcard_result_complete(idcard_result))
        {
            recg_result->img_info.img_status = "normal";
        }
        else if (check_idcard_result_empty(idcard_result))
        {
            tmplocr::Idcard_Struct_Result idcard_reversed_result;
            nscommon::time_elapsed_t time_used_back("struct_analysis idcard_back", 1);
            int ret_reversed = tmplocr::recognize_idcard_back(recog_result_for_tmplocr,
                                                              idcard_reversed_result);
            if (!check_idcard_result_empty(idcard_reversed_result))
            {
                recg_result->img_info.img_status = "reversed_side";
            }
        }
    }

    if (recg_result->img_info.img_status == "")
    {
        do
        {
            std::vector<rcnnchar::Box> detected_idcard;
            int loc_ret = locate_idcard_fcnn(_src_img,
                    _init_arg.databuf->_m_thread_model._m_fcn_detector_idcard,
                    detected_idcard);
            if (loc_ret != 0)
            {
                break;
            }
            if (detected_idcard.size() == 0)
            {
                recg_result->img_info.img_status = "non_idcard";
                break;
            }
            rcnnchar::Box idcard_rect;
            idcard_rect._score = 0;
            for (size_t i = 0; i < detected_idcard.size(); ++i)
            {
                if (detected_idcard[i]._score > idcard_rect._score)
                {
                    idcard_rect = detected_idcard[i];
                }
            }

            cv::Mat img_mat(_src_img);
            int left = std::max(0, (int)(idcard_rect._coord[0] + 0.5));
            int top = std::max(0, (int)(idcard_rect._coord[1] + 0.5));
            int right = std::min(img_mat.cols,
                    (int)(idcard_rect._coord[0] + idcard_rect._coord[2] + 0.5));
            int bottom = std::min(img_mat.rows,
                    (int)(idcard_rect._coord[1] + idcard_rect._coord[3] + 0.5));
            cv::Mat crop_img = img_mat(cv::Range(top, bottom), cv::Range(left, right));
            cv::Point2f offset(left, top);

            int blur_ret = check_blur(crop_img);
            if (blur_ret != 0 && blur_ret != 1)
            {
                break;
            }
            if (blur_ret == 1)
            {
                recg_result->img_info.img_status = "blurred";
                break;
            }
            int over_exposure_ret = check_over_exposure(crop_img,
                                                        recog_result_for_tmplocr,
                                                        offset);
            if (over_exposure_ret == 1)
            {
                recg_result->img_info.img_status = "over_exposure";
                break;
            }
        } while (0);
    }

    if (recg_result->img_info.img_status == "")
    {
        recg_result->img_info.img_status = "unknown";
    }

    if (nsmocr::CTemplOCRConvert::convert_output(idcard_result, *recg_result))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_INTER_FORMAT);
        WARNING_LOG("CTemplOCRConvert convert output error!");
        return nsmocr::ERR_INTER_FORMAT;
    }

    //if (ret != 0)
    //{
    //    // nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_ID_CARD);
    //    WARNING_LOG("Recogize Idcard Failed!");
    //    // return nsmocr::ERR_ID_CARD;
    //}

    recg_result->set_direction();

    get_json_ret(_init_arg.databuf, 
                 *recg_result, _src_img, time_used.get_time());

    return 0;
}

int CProc::get_json_ret(appocr_thread_data* databuf, 
        recg_result_t& recg_result,
        const IplImage* src_img, int time_proc, recg_result_t* result)
{
    nscommon::time_elapsed_t time_used(__FUNCTION__, 1);

    //TODO:
    /*
       if (transChinesePunct && ctSwitch.codeType == GBK) 
       {
           PostChinesePunct(ret, ret_size);
       } 
       else if (transChinesePunct && ctSwitch.codeType == UTF8)
       {
           postChinesePunctUTF8(ret, ret_size);
       }
    */

    nsmocr::BdOcrRecgConvert::filter_not_json(recg_result, 
            nsmocr::CQueryData::is_utf8(_init_arg.databuf));

    databuf->m_busOcr2fcgi.m_iSrcImgWidth = src_img->width;
    databuf->m_busOcr2fcgi.m_iSrcImgHeight = src_img->height;
    std::cout<<"img_width:"<<src_img->width;
    std::cout<<" img_height :"<<src_img->height<<std::endl;
    std::string json_ret = nsmocr::CGetJson::get_json_ret(_init_arg.databuf, recg_result, result);
    std::cout<<"json_ret:"<<json_ret<<std::endl;
    if (json_ret.length() <= 0)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_JSON);
        WARNING_LOG("get_json_ret error!context is\t%s", 
                databuf->m_busFcgi2ocr.get_info().c_str());
        std::cout<<"m_busFcgi2ocr.get_info().c_str():"<<databuf->m_busFcgi2ocr.get_info().c_str()<<std::endl;
    }
    else
    {
        //strcpy(_init_arg.databuf->m_busOcr2fcgi.m_pszTextData, "");
        //snprintf(_init_arg.databuf->m_busOcr2fcgi.m_pszTextData, "");
        snprintf(_init_arg.databuf->m_busOcr2fcgi.m_pszTextData, 
                databuf->m_busOcr2fcgi.m_iMaxTextDataLen - 1, "%s", json_ret.c_str());
        databuf->m_busOcr2fcgi.m_iTextLen = 
            strlen(_init_arg.databuf->m_busOcr2fcgi.m_pszTextData) + 1;
    }

    NOTICE_LOG("time_used=%d\t%s\tocr_result=%s", 
            time_proc,
            databuf->m_busFcgi2ocr.get_info().c_str(),
            json_ret.c_str());

    return 0;
}

int CProc::_loc()
{
    nscommon::time_elapsed_t time_used(__FUNCTION__, 1);

    loc_init_t loc_init_arg;
    loc_init_arg.copy(_init_arg);

    CRcnnLoc proc_loc;
    if (proc_loc.init(&loc_init_arg))
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_NO_MEM);
        WARNING_LOG("log:%d proc_loc init error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_NO_MEM;
    }

    int ret = proc_loc.locate(_src_img);
    if (ret)
    {
        nsmocr::CQueryData::set_errmsg(_init_arg.databuf, nsmocr::ERR_LOC);
        WARNING_LOG("log:%d locate error!", CQueryData::get_logid(_init_arg.databuf));
        return nsmocr::ERR_LOC;
    }

    loc_result_t* loc_result = proc_loc.get_loc_result_in_query_image();
    recg_result_t rec_result(*loc_result);
    rec_result.set_direction();
    proc_loc.post_act(&rec_result);

    if (CQueryData::is_classify_is_timu(_init_arg.databuf))
    {
        int is_timu = -1;
        /// new version
        is_timu = ocr_attr::is_timu_image(_src_img,
                       _init_arg.model->_m_timu_cnn_model,
                       _init_arg.model->_m_timu_cnn_mean);
        if (is_timu == 1) 
        {
            rec_result.img_info.old_img_dir = loc_result->img_info.img_dir;
            rec_result.img_info.text_type = TEXT_TIMU; 
        } 
        else if (is_timu == 2) 
        {
            rec_result.img_info.old_img_dir = 4;
            rec_result.img_info.text_type = TEXT_GENERAL;
        } 
        else 
        {
            rec_result.img_info.old_img_dir = -1;
            rec_result.img_info.text_type = TEXT_NONE;
        }
    }

    get_json_ret(_init_arg.databuf, rec_result, _src_img, time_used.get_time());
    return 0;
}

int CProc::_resize_recg_result(nsmocr::recg_result_t& recg_result)
{
    if (_rescale_ratio <= 0)
    {
        return -1;
    }

    for (int i = 0; i < recg_result.regions.size(); i++)
    {
        nsmocr::recg_region_t& region = recg_result.regions[i];
        int region_left = region.left;
        int region_top = region.top;
        int region_width  = region.right  - region.left + 1;
        int region_height = region.bottom - region.top  + 1;
        region_left   /= _rescale_ratio;
        region_top    /= _rescale_ratio;
        region_width /= _rescale_ratio;
        region_height  /= _rescale_ratio;
        region.left   = region_left;
        region.top    = region_top;
        region.right  = region_left + region_width - 1;
        region.bottom = region_top  + region_height - 1;
        
        for (int j = 0; j < region.wordinfo.size(); j++)
        {
            nsmocr::recg_wordinfo_t& wordinfo = region.wordinfo[j];
            int word_left = wordinfo.rect.left;
            int word_top = wordinfo.rect.top;
            int word_width  = wordinfo.rect.right  - wordinfo.rect.left + 1;
            int word_height = wordinfo.rect.bottom - wordinfo.rect.top  + 1;
            word_left   /= _rescale_ratio;
            word_top    /= _rescale_ratio;
            word_width /= _rescale_ratio;
            word_height  /= _rescale_ratio;
            wordinfo.rect.left   = word_left;
            wordinfo.rect.top    = word_top;
            wordinfo.rect.right  = word_left + word_width - 1;
            wordinfo.rect.bottom = word_top  + word_height - 1;
        }

    }

    return 0;
}

};
