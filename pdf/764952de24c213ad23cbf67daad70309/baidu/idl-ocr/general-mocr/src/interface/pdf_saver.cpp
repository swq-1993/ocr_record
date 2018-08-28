#include "pdf_saver.h"

using nsmocr::CPDFSaver;
namespace nsmocr
{
CPDFSaver::CPDFSaver(const nsmocr::recg_result_t& recg_result, std::string path, 
        std::string query_sign, const IplImage* src_img) : EDGE_BLANK_SIZE(80), 
    SMALL_FONT_THRESHOLD(0.7), FONT_PIXEL_PROP(0.75), FONT_SIZE_TRY_TIMES(20), 
    CREATE_DIR_MODE(0775), PDF_DIR_NAME("/pdf"), TMP_DIR_NAME("/tmp"), SMALL_AREA_SCALE(500),
    PDF_PAGE_SIZE(HPDF_PAGE_SIZE_A4), A4_WIDTH(595.276), A4_HEIGHT(841.89)
{
    _outpath = path;
    _query_sign = query_sign;
    cv::Mat temp;
    temp = cv::Mat(src_img, true);
    //cv::imwrite("temp.jpg", temp);
    const std::vector<nsmocr::recg_region_t>& regions = recg_result.regions;
    for (int i = 0; i < (int)regions.size(); i++)
    {
        PDFResult result;
        nsmocr::recg_region_t src = regions[i];
        get_ocr_color(src, i);
        //cv::imwrite("line" + i + ".jpg", line);
        std::cout << "result rect:" << src.left << " ";
            std::cout << src.top << " " << std::endl;
        result.rect.left = src.left;
        result.rect.top = src.top;
        result.rect.width = src.right - src.left + 1;
        result.rect.height = src.bottom - src.top + 1;
        result.text = src.ocrresult;

        int real_height = calc_real_height(src);
        if (real_height <= 0)
        {
            real_height = _results[i].rect.height;
        }
        _real_height.push_back(real_height);

        _origin_results.push_back(result);
        _results.push_back(result);
    }

    _src_img = cvCloneImage(src_img);
}

CPDFSaver::~CPDFSaver()
{
    cvReleaseImage(&_src_img);
    cvReleaseImage(&_threshold_img);
    cvReleaseImage(&_cut_img);
}

int CPDFSaver::get_ocr_color(nsmocr::recg_region_t src, int i, cv::Mat input)
{
    int ret = 0;
    int left = src.left;
    int top = src.top;
    int width  = src.right - src.left + 1;
    int height = src.bottom - src.top + 1;
    int bottom = src.bottom;
    int right = src.right;
    //top = top - 1;
    if (top < 0)
    {
        top = 0;
    }
    //left = left - 2;
    if (left < 0)
    {
        left = 0;
    }
    std::cout << "get color:" << left << " " << top << " " << width << " " << height << std::endl;
    cv::Mat temp_line;  
    //cv::Rect rect(left, top, width, height);
    //cv::Mat roiImage = _src_img(cv::Rect(left, top, width, height));  
    //_src_img(rect).copyTo(line);
    cv::Mat temp_img;
    temp_img = cv::Mat(_src_img, true);
    //cv::Mat temp_img(_src_img); 
    //temp_line = temp_img(rect);
    //temp_img(rect).copyTo(temp_line);
    std::string a;
    a = std::to_string(i);
    cv::imwrite("roiimage" + a + ".jpg" , input);
    //srcImage(rect).copyTo(line);
    //cv::imwrite("roi.jpg", line); 
    //cv::Rect rect = _pic_rects[i];
    return ret;
}

int CPDFSaver::init()
{
    int ret = 0;
    ret = cut_core_img();
    if (ret != 0)
    {
        return -1;
    }
    cover_text();
    generate_contours();
    generate_pic_rects();
    handle_rect_overlap();
    handle_pic_overlap();
    /*
    cvSaveImage("./src.JPG", _src_img);
    cvSaveImage("./cut_img.JPG", _cut_img);
    cvSaveImage("./thre.JPG", _threshold_img);
    */
    return 0;
}

int CPDFSaver::save()
{
    //pdf file name
    std::string file_name = _outpath + PDF_DIR_NAME + "/" + _query_sign + ".pdf"; 
    std::cout<<"file_name:"<<file_name<<std::endl;
    HPDF_Doc pdf = HPDF_New(error_handler, NULL);
    if (!pdf)
    {
        WARNING_LOG("error: cannot create PdfDoc object\n");
        return -1;
    } 

    HPDF_Page page;
    HPDF_REAL height;
    HPDF_REAL width;
    HPDF_Font font;
    try
    {
        page = HPDF_AddPage(pdf);
        HPDF_STATUS status = HPDF_Page_SetSize(page, PDF_PAGE_SIZE, HPDF_PAGE_PORTRAIT);
        if (status != HPDF_OK)
        {
            WARNING_LOG("error: cannot set page size\n");
            HPDF_Free(pdf);
            return -1;
        }
        HPDF_SetCompressionMode(pdf, HPDF_COMP_ALL);
        height = HPDF_Page_GetHeight(page);
        width = HPDF_Page_GetWidth(page);

        HPDF_UseCNSFonts(pdf);
        //HPDF_UseCNTFonts(pdf);
        HPDF_UseCNSEncodings(pdf);
        //HPDF_UseCNTEncodings(pdf);
        font = HPDF_GetFont(pdf, "SimSun", "GBK-EUC-H");
        //std::cout << "_results.size():" << _results.size() << std::endl;
        if (_results.size() == 0)
        {
            HPDF_SaveToFile(pdf, file_name.c_str());
            HPDF_Free(pdf);
            return 0;
        }
    }catch(...)
    {
        //std::cout<<"enter catch"<<std::endl;
        HPDF_Free(pdf);
        return -1;
    }

    //calc avgHeight & scale
    int avg_height = 0;
    int total_height = 0;
    int max_utf8_textsize = 0;
    for (int i = 0; i < _results.size(); i++)
    {
        total_height += _real_height[i];
        int utf8_textsize = _results[i].text.size();
        if (max_utf8_textsize < utf8_textsize)
        {
            max_utf8_textsize = utf8_textsize;
        }
    }
    avg_height = total_height / _results.size();

    int img_width = _cut_img->width;
    float scale = width / img_width;

    // conv utf8 to gbk
    const int max_out = 4 * max_utf8_textsize + 1;
    char* buf_out = new char[max_out]; 
    try
    {
        for (int i = 0; i < _results.size(); i++)
        {
            // small word use small font size
            /*
               float height_prop= (float)_results[i].rect.height / avg_height;
               if (height_prop <= SMALL_FONT_THRESHOLD) 
               {
               HPDF_Page_SetFontAndSize(page, font, (int)(x_scale * _results[i].rect.height * FONT_PIXEL_PROP));
               }
               else
               {
               HPDF_Page_SetFontAndSize(page, font, (int)(x_scale * avg_height * FONT_PIXEL_PROP));
               }
               */
            //HPDF_Page_SetFontAndSize(page, font, (int)(scale * avg_height * FONT_PIXEL_PROP));
            HPDF_Page_SetFontAndSize(page, font, (int)_real_height[i] * scale * FONT_PIXEL_PROP);
            /*std::cout << "_result rect:" << _results[i].rect.left << " ";
            std::cout << _results[i].rect.top << " ";
            std::cout << _results[i].rect.width << " ";
            std::cout << _results[i].rect.height << " " << std::endl;*/
            // words should be included in width, try to modify font size
            int x_pos = _results[i].rect.left * scale;
            int y_pos = height - (_results[i].rect.top + avg_height) * scale;

            //calc width available
            int width_avail = width;
            if (i != _results.size() - 1)
            {
                const float ONE_LINE_THRESHOLD = 0.8;
                int overlap_height = _results[i].rect.top + _results[i].rect.height 
                    - _results[i + 1].rect.top;
                int join_avg_height = (_results[i].rect.height + _results[i + 1].rect.height) / 2;
                if (overlap_height > join_avg_height * ONE_LINE_THRESHOLD)
                {
                    int x_pos_next = _results[i + 1].rect.left * scale;
                    if (x_pos_next > x_pos + join_avg_height)
                    {
                        width_avail = x_pos_next;
                    }
                }

            }

            memset(buf_out, 0, max_out);
            utf8_to_gbk(_results[i].text.c_str(), _results[i].text.size(), buf_out, max_out);
            std::cout<<"buf_out:"<<std::string(buf_out)<<std::endl;
            _results[i].text = std::string(buf_out); 

            for (int j = 0; j < FONT_SIZE_TRY_TIMES; j++)
            {
                HPDF_REAL cur_size = HPDF_Page_GetCurrentFontSize(page);
                if (cur_size < 2.0)
                {
                    break;
                }
                HPDF_Page_SetFontAndSize(page, font, cur_size - 1);
                HPDF_REAL real_width;
                HPDF_UINT byte_num = HPDF_Page_MeasureText(page, _results[i].text.c_str(), 
                        width_avail - x_pos, HPDF_FALSE, &real_width);
                if (byte_num == strlen(_results[i].text.c_str()))
                {
                    break;
                }
            }

            HPDF_Page_BeginText(page);
            HPDF_Page_MoveTextPos(page, x_pos, y_pos);
            HPDF_Page_ShowText(page, _results[i].text.c_str());
            HPDF_Page_EndText(page);

        }
        std::cout<<"try to save pdf"<<std::endl;
        insert_pic(pdf, page, scale, height);
        DEBUG_LOG("pdf save to file, %s", file_name.c_str());
        std::cout<<"pdf save to file:"<<file_name.c_str()<<std::endl;
        std::string text_file_name = _outpath + PDF_DIR_NAME + "/" + _query_sign + ".txt";
        save_file(text_file_name.c_str());
        HPDF_SaveToFile(pdf, file_name.c_str());
    }catch (...)
    {
        std::cout<<"enter the catch"<<std::endl;
        delete [] buf_out;
        HPDF_Free(pdf);
        return -1;
    }

    delete [] buf_out;
    HPDF_Free(pdf);
    return 0;
}

void CPDFSaver::error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void *user_data)
{
    WARNING_LOG("ERROR: error_no=%04X, detail_no=%u\n", 
            (HPDF_UINT)error_no, (HPDF_UINT)detail_no);
    throw std::exception();
}

bool CPDFSaver::utf8_to_gbk(const char *inbuf, size_t inlen, char *outbuf, size_t outlen)
{
    char *enc_to = "GBK";
    char *enc_from = "UTF-8";

    iconv_t cd = iconv_open(enc_to, enc_from);
    if (cd == (iconv_t)-1)
    {   
        return false;
    }   

    char *tmpin = new char[inlen];
    char * in_for_delete = tmpin;
    memcpy(tmpin, inbuf, inlen);

    size_t ret = iconv(cd, &tmpin, &inlen, &outbuf, &outlen);
    if (ret == -1) 
    {   
        iconv_close(cd);
        delete [] in_for_delete;
        return false;
    }   

    iconv_close(cd);
    delete [] in_for_delete;
    return true;
}

void CPDFSaver::handle_rect_overlap()
{
    const int DEPTH = 3;
    const float OVERLAP_THRESHOLD = 0.5;
    if (_results.size() == 0)
    {
        return;
    }
    //std::cout << "enter handle_rect_overlap" << std::endl;
    int times = _results.size() - DEPTH;
    times  = (times > 0) ? times : 0;
    for (int i = 0; i < times; i++)
    {
        for (int j = 0; j < DEPTH; j++)
        {
            if ((_results[i].rect.top + _real_height[i] > _results[i + j + 1].rect.top) && 
                    (_results[i].rect.left + _results[i].rect.width > 
                     _results[i + j + 1].rect.left))
            {
                int overlap_height = _results[i].rect.top + _real_height[i] 
                    - _results[i + j + 1].rect.top;
                int join_avg_height = (_real_height[i] + _real_height[i + 1]) / 2;
                //2 rect in 1 line
                if ((overlap_height > join_avg_height * OVERLAP_THRESHOLD) && 
                        (_results[i].rect.left + _results[i].rect.width 
                         - _results[i + j + 1].rect.left < join_avg_height))
                {
                    _results[i + j + 1].rect.left += join_avg_height;
                }
                else
                {
                    _results[i + j + 1].rect.top += overlap_height;
                }
            }
        }
    }

    for (int i = times; i < _results.size() - 1; i++)
    {
        for (int j = i + 1; j < _results.size(); j++)
        {
            if ((_results[i].rect.top + _real_height[i] > _results[j].rect.top) && 
                    (_results[i].rect.left + _results[i].rect.width > _results[j].rect.left))
            {
                int overlap_height = _results[i].rect.top +  _real_height[i] - _results[j].rect.top;
                int join_avg_height = (_real_height[i] + _real_height[j]) / 2;
                //2 rect in 1 line
                if ((overlap_height > join_avg_height * OVERLAP_THRESHOLD) && 
                        (_results[i].rect.left + _results[i].rect.width - _results[j].rect.left 
                         < join_avg_height))
                {
                    _results[j].rect.left += join_avg_height;
                }
                else
                {
                    _results[j].rect.top += overlap_height;
                }
            }
        }
    }

    for (int i = 0; i < _results.size() - 1; i++)
    {
        int before_offset = _results[i].rect.top - _origin_results[i].rect.top;
        int cur_offset = _results[i + 1].rect.top - _origin_results[i + 1].rect.top;
        if (cur_offset < before_offset)
        {
            _results[i + 1].rect.top = _origin_results[i + 1].rect.top + before_offset; 
        }
    }
    //std::cout << "finish handle_rect_overlap" << std::endl;
    
}

bool CPDFSaver::sort_pic_by_y(const cv::Rect &rect1, const cv::Rect &rect2)
{
    return rect1.y < rect2.y;
}

void CPDFSaver::handle_pic_overlap()
{

    if (_pic_rects.size() == 0 || _origin_results.size() < 1)
    {
        return;
    }
    //std::cout << "enter handle_pic_overlap" << std::endl;
/*
    for (int i = 0; i <  _results.size(); i++)
    {
        for (int j = 0; j < _pic_rects.size(); j++)
        {
            if ((_results[i].rect.top + _results[i].rect.height > _pic_rects[j].y) &&
                    (_results[i].rect.left + _results[i].rect.width > _pic_rects[j].x))
            {
                int overlap_height = _results[i].rect.top + _results[i].rect.height - _pic_rects[j].y;
                _pic_rects[j].y += overlap_height;
            }
        }
    }
    std::sort(_pic_rects.begin(), _pic_rects.end(), sort_pic_by_y);

    for(int i = 0; i < _pic_rects.size() - 1; i++)
    {
        for (int j = i + 1; j < _pic_rects.size(); j++)
        {
            if ((_pic_rects[i].y + _pic_rects[i].height > _pic_rects[j].y) &&
                    (_pic_rects[i].x + _pic_rects[i].width > _pic_rects[j].x))
            {
                int overlap_height = _pic_rects[i].y + _pic_rects[i].height - _pic_rects[j].y;
                _pic_rects[j].y += overlap_height;
            }
        }
    }
    */

    for (int j = 0; j < _pic_rects.size(); j++)
    {
        int i = 0;
        //std::cout << "enter pic_rect for function" << std::endl;
        for (; i <  _origin_results.size() - 1; i++)
        {
            if (_origin_results[i].rect.top > _pic_rects[j].y)
            {
                _pic_offset.push_back(0);
                break;
            }
            else if ((_origin_results[i].rect.top < _pic_rects[j].y) && 
                    (_origin_results[i + 1].rect.top > _pic_rects[j].y))
            {
                int overlap_height = _results[i].rect.top - _origin_results[i].rect.top;
                _pic_offset.push_back(overlap_height);
                break;
            }
        }

        if (i == _origin_results.size() - 1)
        {
            int overlap_height = _results[_origin_results.size() - 1].rect.top - 
                _origin_results[_origin_results.size() - 1].rect.top;
            _pic_offset.push_back(overlap_height);
        }
    }
    //std::cout << "finish pic_rect function" << std::endl;    

}

void CPDFSaver::cover_text()
{
    int size =  _results.size();
    CvPoint ** pt = new CvPoint*[size];
    int * npts = new int[size];
    //std::cout << "enter cover_text" << std::endl; 
    for (int i = 0; i < size; i++)
    {
        pt[i] = new CvPoint[4];
        pt[i][0] = cvPoint(_results[i].rect.left, _results[i].rect.top);
        pt[i][1] = cvPoint(_results[i].rect.left + _results[i].rect.width, _results[i].rect.top);
        pt[i][2] = cvPoint(_results[i].rect.left + _results[i].rect.width, _results[i].rect.top + 
                _results[i].rect.height);
        pt[i][3] = cvPoint(_results[i].rect.left, _results[i].rect.top + _results[i].rect.height);
    }

    for (int i = 0; i < size; i++)
    {
        npts[i] = 4;
    }

    cvFillPoly(_threshold_img, pt, npts, size, cvScalar(255)); 

    delete [] npts;
    for (int i = 0; i < size; i++)
    {
        delete [] pt[i];
    }
    delete [] pt;
    //std::cout << "finish cover_text" << std::endl;
}

void CPDFSaver::generate_contours()
{
    IplImage * threshold_img_clone = cvCloneImage(_threshold_img);
    CvSeq* contours = NULL;   
    CvMemStorage* storage = cvCreateMemStorage(0); 
    //std::cout << "enter generate_contours" << std::endl; 
    int totals = cvFindContours(threshold_img_clone, storage, &contours, sizeof(CvContour), 
            CV_RETR_TREE, CV_CHAIN_APPROX_NONE, cvPoint(0, 0));
    CvSeq* contours_temp = contours;

    while (contours_temp != NULL)
    {
        //std::cout << "enter generate_contours while" << std::endl;
        CvRect rect = cvBoundingRect(contours_temp, 0);
        if (!is_overlap_text(rect))
        {
            _frag_rects.push_back(rect);
            if (contours_temp->h_next != NULL)
            {
                //contours_temp->v_prev->v_next = contours_temp->h_next;
                contours_temp->h_next->v_prev = contours_temp->v_prev;
                contours_temp = contours_temp->h_next;
                continue;
            }
            else
            {
                if (contours_temp->v_prev == NULL)
                {
                    break;
                }
                if (contours_temp->v_prev->h_next == NULL)
                {
                    break;
                }

                contours_temp->v_prev->h_next->v_prev = contours_temp->v_prev->v_prev;
                contours_temp = contours_temp->v_prev->h_next;
            }
        }
        else
        {
            if (contours_temp->v_next != NULL)
            {
                contours_temp  = contours_temp->v_next;
                continue;
            }
            else 
            {
                if (contours_temp->h_next != NULL)
                {
                    //contours_temp->v_prev->v_next = contours_temp->h_next;
                    contours_temp->h_next->v_prev = contours_temp->v_prev;
                    contours_temp = contours_temp->h_next;
                    continue;
                }
                else
                {
                    if (contours_temp->v_prev == NULL)
                    {
                        break;
                    }
                    if (contours_temp->v_prev->h_next == NULL)
                    {
                        break;
                    }

                    contours_temp->v_prev->h_next->v_prev = contours_temp->v_prev->v_prev;
                    contours_temp = contours_temp->v_prev->h_next;
                }
            }
        }
    }

    if (storage != NULL)
    {
        cvReleaseMemStorage(&storage);
    }

    if (threshold_img_clone != NULL)
    {
        cvReleaseImage(&threshold_img_clone);
    }
    //std::cout << "finish find contours function" << std::endl;
}

bool CPDFSaver::is_small_area(CvRect rect)
{
    double min_area = _cut_img->height * _cut_img->width / SMALL_AREA_SCALE;
    //CvRect rect = cvBoundingRect(contours,0);
    float area = rect.width * rect.height;
    if (area < min_area)
    {
        return true;
    }
    else if (rect.height < 3 || rect.width < 3)
    {
        return true;
    }
    return false;
}

bool CPDFSaver::is_overlap_text(cv::Rect rect)
{
    int total_overlap_area = 0;
    int size =  _results.size();
    for (int i = 0; i < size; i++)
    {
        cv::Rect tmp_rect(_results[i].rect.left, _results[i].rect.top, _results[i].rect.width, 
                _results[i].rect.height);
        total_overlap_area = total_overlap_area + (tmp_rect & rect).area();
    }
    return ((float)total_overlap_area) / rect.area() > 0;
}

bool CPDFSaver::is_overlap_pic(cv::Rect rect)
{
    int total_overlap_area = 0;
    int size =  _pic_rects.size();
    for (int i = 0; i < size; i++)
    {
        total_overlap_area = total_overlap_area + (_pic_rects[i] & rect).area();
    }
    return ((float)total_overlap_area) / rect.area() > 0;
}

void CPDFSaver::generate_pic_rects()
{
    //std::cout << "enter generate_pic_rects" << std::endl;
    while (_frag_rects.size() > 0)
    {
        //std::cout << "enter generate_pic_rects while" << std::endl;
        std::vector<cv::Rect>::iterator it_pos = _frag_rects.begin();
        cv::Rect comb_rect = *it_pos;
        _frag_rects.erase(it_pos);

        it_pos = _frag_rects.begin();
        while (it_pos != _frag_rects.end())
        {
            cv::Rect tmp_rect = comb_rect | *it_pos;
            if (!is_overlap_text(tmp_rect) && !is_overlap_pic(tmp_rect))
            {
                comb_rect = tmp_rect;
                it_pos = _frag_rects.erase(it_pos);
            }
            else
            {
                it_pos++;
            }
        }
        if (!is_small_area(comb_rect))
        {
            _pic_rects.push_back(comb_rect);
        }
    }
    //std::cout << "finish generate_pic_rects" << std::endl;
}

void CPDFSaver::insert_pic(HPDF_Doc pdf, HPDF_Page page, float scale, int height)
{
    std::string tmp_path =  _outpath + PDF_DIR_NAME + TMP_DIR_NAME + "/";
    std::cout<<"tmp_path:"<<tmp_path<<std::endl; 
    //for (std::vector<cv::Rect>::iterator it_pos = _pic_rects.begin(); it_pos!= _pic_rects.end(); it_pos++)
    std::cout<<"_pic_rects:"<<_pic_rects.size()<<std::endl;
    for (int i = 0; i < _pic_rects.size(); i++)
    {
        cv::Rect rect = _pic_rects[i];
        
        cvSetImageROI(_cut_img, rect);
        IplImage * ipl_img = cvCreateImage(cvSize(rect.width * scale, rect.height * scale), 
                _cut_img->depth, _cut_img->nChannels);
        cvZero(ipl_img);
        cvResize(_cut_img, ipl_img, CV_INTER_AREA);
        cvResetImageROI(_cut_img);

        //save tmp pic fragment
        std::stringstream img_name;
        img_name << tmp_path +  _query_sign + "_" << i << ".jpg";
        cvSaveImage(img_name.str().c_str(), ipl_img);

        //HPDF_BYTE *buf = (unsigned char*)ipl_img->imageData;
        std::cout<<"before HPDF_LoadJpegImageFromFile;"<<std::endl;
        std::cout<<img_name.str()<<std::endl; 
        HPDF_Image img = HPDF_LoadJpegImageFromFile(pdf, img_name.str().c_str());
        std::cout<<"after HPDF_LoadJpegImageFromFile;"<<std::endl;
        //HPDF_Image img = HPDF_LoadRawImageFromMem(pdf, buf, ipl_img->width, ipl_img->height, HPDF_CS_DEVICE_RGB, 8);
        int x_pos = rect.x * scale;
        int y_pos = height - (rect.y  + rect.height + _pic_offset[i]) * scale;
        HPDF_Page_DrawImage(page, img, x_pos, y_pos, HPDF_Image_GetWidth(img), 
                HPDF_Image_GetHeight(img));
        cvReleaseImage(&ipl_img);
        std::remove(img_name.str().c_str());
    }
}
/*
int CPDFSaver::check_path()
{
    if (access(_outpath.c_str(), 0) == -1)
    {
        int flag = mkdir(_path.c_str(), 0775);
        if (flag != 0)
        {
            return -1;
        }
        
        flag = mkdir((_path + "/pdf").c_str(), 0775);
        if (flag != 0)
        {
            return -1;
        }

        flag = mkdir((_path + "/pdf/tmp").c_str(), 0775);
        if (flag != 0)
        {
            return -1;
        }

        
    }

}
*/
int CPDFSaver::save_file(const char * file_name)
{
    std::ofstream out;
    out.open(file_name, std::ios::out | std::ios::trunc);
    if (!out.is_open())
    {
        WARNING_LOG("file:%s cannot open", file_name);
        return -1;
    }

    for (int i = 0; i < _results.size(); i++)
    {
        out <<  _results[i].text << std::endl;
    }
    out.close();
    return 0;
}

int CPDFSaver::calc_real_height(nsmocr::recg_region_t &src)
{
    if (src.poly_location.size() != 8)
    {
        WARNING_LOG("poly do not has 4 points");
        return -1;
    }
    else
    {
        int x1 = src.poly_location[0];
        int y1 = src.poly_location[1];
        int x2 = src.poly_location[2];
        int y2 = src.poly_location[3];
        int x3 = src.poly_location[4];
        int y3 = src.poly_location[5];
        int x4 = src.poly_location[6];
        int y4 = src.poly_location[7];

        return sqrt((y4 - y1) * (y4 - y1) + (x4 - x1) * (x4 - x1));
    }
}

int CPDFSaver::cut_core_img()
{
    if (_results.size() == 0)
    {
        WARNING_LOG("No text recognized!");
        return -1;
    }

    HPDF_REAL pdf_page_width = A4_WIDTH;
    HPDF_REAL pdf_page_height = A4_HEIGHT;
    HPDF_REAL pdf_page_text_width = pdf_page_width - 2 * EDGE_BLANK_SIZE;
    HPDF_REAL pdf_page_text_height = pdf_page_height - 2 * EDGE_BLANK_SIZE;

    int min_x = INT_MAX;
    int min_y = INT_MAX;
    int max_x = 0;
    int max_y = 0;
    for (int i = 0; i < _results.size(); i++)
    {
        if (min_x > _results[i].rect.left)
        {
            min_x = _results[i].rect.left;
        }
        if (max_x < _results[i].rect.left + _results[i].rect.width)
        {
            max_x = _results[i].rect.left + _results[i].rect.width;
        }
        if (min_y > _results[i].rect.top)
        {
            min_y = _results[i].rect.top;
        }
        if (max_y < _results[i].rect.top + _results[i].rect.height)
        {
            max_y = _results[i].rect.top + _results[i].rect.height;
        }
    
    }
    //std::cout << "enter cut_core_img:" << std::endl;
    int core_left = 0;
    int core_top = 0;
    int core_right = 0;
    int core_bottom = 0;
    int core_text_width = max_x - min_x + 1;
    int core_text_height = max_y - min_y + 1;

    float width_scale = core_text_width / pdf_page_text_width;
    float height_scale = core_text_height / pdf_page_text_height;
    float large_scale = 0;
    int img_border = 0;
    std::cout << "width_scale:" << width_scale << " height_scale:" << height_scale << std::endl;
    if (width_scale > height_scale)
    {
        int height_append_top = 0;
        int height_append_bottom = 0;
        if (_src_img->height != core_text_height)
        {
            int height_append = width_scale * pdf_page_text_height - core_text_height;

            height_append_top = ((float)min_y / (_src_img->height - core_text_height)) * 
                height_append;
            height_append_bottom = ((float)(_src_img->height - max_y) / (_src_img->height - 
                        core_text_height)) * height_append;
        }
        
        core_left = min_x;
        core_top = (min_y - height_append_top) > 0 ? (min_y - height_append_top) : 0; 
        core_right = max_x;
        core_bottom = (max_y + height_append_bottom) < _src_img->height ? (max_y + 
                height_append_bottom) : (_src_img->height - 1);
        large_scale = width_scale;
        img_border =  width_scale * EDGE_BLANK_SIZE;
    }
    else
    {
        int width_append_left = 0;
        int width_append_right = 0;
        if (_src_img->width != core_text_width)
        {
            int width_append = height_scale * pdf_page_text_width - core_text_width;

            width_append_left = ((float)min_x / (_src_img->width - core_text_width)) * width_append;
            width_append_right = ((float)(_src_img->width - max_x) / (_src_img->width - 
                        core_text_width)) * width_append;
        }

        core_left = (min_x - width_append_left) > 0 ? (min_x - width_append_left) : 0;
        core_top = min_y;
        core_right = (max_x + width_append_right) < _src_img->width ? (max_x + width_append_right) 
            : (_src_img->width - 1);
        core_bottom = max_y;
        large_scale = height_scale;
        img_border =  height_scale * EDGE_BLANK_SIZE;
    }

    //add border
    int cut_left = ((core_left + 1) > img_border) ? (core_left - img_border) : 0;
    int cut_top = ((core_top + 1) > img_border) ? (core_top - img_border) : 0;
    int cut_right = (_src_img->width - core_right - 1) > img_border ? (core_right + img_border) : 
        (_src_img->width - 1);
    int cut_bottom = (_src_img->height - core_bottom - 1) > img_border ? (core_bottom + img_border) 
        : (_src_img->height - 1);
    int cut_width = cut_right - cut_left + 1;
    int cut_height = cut_bottom - cut_top + 1;

    // calc after-cut position
    for (int i = 0; i < _results.size(); i++)
    {
        _results[i].rect.left -= cut_left;
        _results[i].rect.top -= cut_top;
        _origin_results[i].rect.left -= cut_left;
        _origin_results[i].rect.top -= cut_top;
    }

    //finally cut cut
    cv::Rect rect_from(cut_left, cut_top, cut_width, cut_height);
    cvSetImageROI(_src_img, rect_from);
    int to_img_width = pdf_page_width * large_scale;
    int to_img_height = pdf_page_height * large_scale;
    _cut_img = cvCreateImage(cvSize(to_img_width, to_img_height), _src_img->depth, 
            _src_img->nChannels);
    cvSet(_cut_img, CV_RGB(255, 255, 255));
    int to_left = img_border - (core_left - cut_left);
    int to_top = img_border - (core_top - cut_top);
    cv::Rect rect_to(to_left, to_top, cut_width, cut_height);
    cvSetImageROI(_cut_img, rect_to);
    cvCopy(_src_img, _cut_img, 0);
    cvResetImageROI(_src_img);
    cvResetImageROI(_cut_img);

    _threshold_img = cvCreateImage(cvGetSize(_cut_img), _cut_img->depth, 1); 
    cvCvtColor(_cut_img,  _threshold_img, CV_RGB2GRAY);
    cvThreshold(_threshold_img, _threshold_img, 0, 255, CV_THRESH_OTSU | CV_THRESH_BINARY);
    
    // calc final position
    for (int i = 0; i < _results.size(); i++)
    {
        _results[i].rect.left += to_left;
        _results[i].rect.top += to_top;
        _origin_results[i].rect.left += to_left;
        _origin_results[i].rect.top += to_top;
    }
    //std::cout << "finish cut_core_img" << std::endl;
    return 0;
}
};
