#include "common_func.h"
#include <stdio.h>
#include <cv.h>
#include <cxcore.h>
#include <highgui.h>
#include <dirent.h>

int save_color_data(unsigned char *im, int w, int h,  char *filepath) {

    IplImage *pimage = NULL;
    unsigned char *pby_data = NULL;
    pimage = cvCreateImage(cvSize(w, h), IPL_DEPTH_8U, 3);
    int iwidth_step = pimage->widthStep;
    pby_data = (unsigned char*) pimage->imageData;
    for (int i = 0; i < h; i++) {
        memcpy(pby_data + i * iwidth_step, im + i * w * 3, w * 3);
    }
    cvSaveImage(filepath, pimage);
    cvReleaseImage(&pimage);

    return 0;
}
void large_value_index(float *pVector, int iDim, int iSelNum, int *pSelIndex)  {
    int *psort_index = new int[iDim];
    quicksort(psort_index, pVector, iDim);

    for (int i = 0; i < iSelNum; i++) {
        pSelIndex[i] = psort_index[iDim-1-i];
    }

    if (psort_index) {
        delete []psort_index;
        psort_index = NULL;
    }
    return ;
}

// find the k largest number in the array
int large_value_quick_index(float *pVector, int iDim, int iSelNum, int *pSelIndex) {
    if (pVector == NULL || iDim < iSelNum) {
        return -1;
    }
    int *pbuf = new int[iSelNum * 3];
    int *psel_val_idx = pbuf;
    int *porg_val_idx = psel_val_idx + iSelNum;
    int *pflag = porg_val_idx + iSelNum;
    float *psel_val = new float[iSelNum];
    float *psort_val = new float[iSelNum];
    for (int i = 0; i < iSelNum; i++) {
        psel_val[i] = pVector[i];
    }
    //ascending order
    quicksort(psel_val_idx, psel_val, iSelNum);
    for (int i = 0; i < iSelNum; i++) {
        psort_val[i] = psel_val[psel_val_idx[i]];
        pflag[i] = 0;
    }
    for (int i = 0; i < iSelNum; i++) {

        for (int j = 0; j < iSelNum; j++) {
            if (pflag[j] == 1) {
                continue;
            }
            if (psort_val[i] == pVector[j]) {
                porg_val_idx[i] = j;
                pflag[j] = 1;
                break;
            }
        }
    }
    
    for (int i = iSelNum; i < iDim; i++) {
        float fmin_val = psort_val[0];
        if (pVector[i] < fmin_val) {
            continue;
        }
        int locate_index = -1;
        for (int j = 0; j < iSelNum - 1; j++) {
            if (pVector[i] >= psort_val[j] && \
                pVector[i] <= psort_val[j + 1]) {
                locate_index = j;
                break;
            }
        }
        if (locate_index != -1) {
            for (int j = 1; j <= locate_index; j++) {
                psort_val[j-1] = psort_val[j];
                porg_val_idx[j-1] = porg_val_idx[j];
            }
            psort_val[locate_index] = pVector[i];
            porg_val_idx[locate_index] = i; 
        } else {
            for (int j = 1; j<iSelNum; j++) {
                psort_val[j-1] = psort_val[j];
                porg_val_idx[j-1] = porg_val_idx[j];
            }
            psort_val[iSelNum - 1] = pVector[i];
            porg_val_idx[iSelNum - 1] = i;
        }
    }

    for (int i = 0; i < iSelNum; i++) {
        pSelIndex[i] = porg_val_idx[iSelNum - 1 - i];
    }
    if (pbuf) {
        delete []pbuf;
        pbuf = NULL;
    }
    if (psel_val) {
        delete []psel_val;
        psel_val = NULL;
    }
    if (psort_val) {
        delete []psort_val;
        psort_val = NULL;
    }

    return 0;
}
void draw_line(unsigned char *im, int w, int h, int x0, int y0,\
        int x1, int y1, int ext,\
        unsigned char r, unsigned char g, unsigned char b)  {

    if (!im || w <= 0 || h <= 0 || \
        x0 < 0 || y0 < 0 || x0 >= w || y0 >= h) {
        return ;
    }

    int ind = 0;
    //line equation: A*x+B*y+C=0
    double da = y0 - y1;
    double db =  x1 - x0;
    double dc = x0 * y1 - x1 * y0;

    int imin_x = std::max(std::min(x0, x1), 0);
    int imax_x = std::min(std::max(x0, x1), w - 1);
    int imin_y = std::max(std::min(y0, y1), 0);
    int imax_y = std::min(std::max(y0, y1), h - 1);
    if (fabs(da) > fabs(db)) {
        for (int i = imin_y; i <= imax_y; i++) {
            int x = (int)(-db * i / da - dc / da);
            x = std::max(std::min(x, w - 1), 0);
            int jmin = std::max(x - ext, 0);
            int jmax = std::min(x + ext, w - 1);
            for (int j = jmin; j <= jmax; j++) {
                ind = (i * w + j) * 3;
                im[ind++] = b;
                im[ind++] = g;
                im[ind++] = r;
            }
        }
    }
    else {
        for (int i = imin_x; i <= imax_x; i++) {
            int y = (int)(-da * i / db - dc / db);
            y = std::max(std::min(y, h - 1), 0);
            int jmin = std::max(y - ext, 0);
            int jmax = std::min(y + ext, h - 1);
            for (int j = jmin; j <= jmax; j++) {
                ind = (j * w + i) * 3;
                im[ind++] = b;
                im[ind++] = g;
                im[ind++] = r;
            }			
        }
    }
    return ;
}

void draw_rectangle(unsigned char *im, int w, int h,\ 
     int left, int right, int top, int bottom,\
     int ext, unsigned char r, unsigned char g, unsigned char b) {
    if (!im || w <= 0 || h <= 0) {
        return ;
    }
    if (left < 0 || left >= w || right < 0 || right >= w\
       || top < 0 || top >= h || bottom < 0 || bottom >= h) {
        return ;
    }

    draw_line(im, w, h, left, top, right, top, ext, r, g, b);
    draw_line(im, w, h, left, top, left, bottom, ext, r, g, b);
    draw_line(im, w, h, right, bottom, right, top, ext, r, g, b);
    draw_line(im, w, h, right, bottom, left, bottom, ext, r, g, b);

    return ;
}

//unicode to string
std::string seq_lunicode_vec_2_utf8_str(std::vector<int>& vec) {
    int n_size = 0;
    char* p = new char[6 * vec.size() + 1];
    int unicode = 0;
    unsigned char* q = (unsigned char*) p;
    for (int i = 0; i < vec.size(); i++)
    {
        unicode = vec[i];
        
        if (unicode >= 0 && unicode <= 0x7f) // 1 byte.
        {
            q[0] = (unsigned char)unicode;     
            q++;
            n_size++;
        }
        else if (unicode <= 0x7ff) // 2 bytes.
        {
            q[0] = 0xc0 | (unicode >> 6);
            q[1] = 0x80 | (unicode & (0xff >> 2)); 
            q = q + 2;
            n_size = n_size + 2;
        }
        else if (unicode <= 0xffff) // 3 bytes.
        {
            q[0] = 0xe0 | (unicode >> 12);
            q[1] = 0x80 | ((unicode >> 6) & (0xff >> 2));
            q[2] = 0x80 | (unicode & (0xff >> 2));    
            q = q + 3;
            n_size = n_size + 3;
        }
        else if (unicode <= 0x1fffff) // 4 bytes.
        {
            q[0] = 0xf0 | (unicode >> 18);
            q[1] = 0x80 | ((unicode >> 12) & (0xff >> 2));
            q[2] = 0x80 | ((unicode >> 6) & (0xff >> 2));
            q[3] = 0x80 | (unicode & (0xff >> 2));   
            q = q + 4;
            n_size = n_size + 4;
        }
        else if (unicode <= 0x3ffffff) // 5 bytes.
        {
            q[0] = 0xf0 | (unicode >> 24);
            q[1] = 0x80 | ((unicode >> 18) & (0xff >> 2));
            q[2] = 0x80 | ((unicode >> 12) & (0xff >> 2));
            q[3] = 0x80 | ((unicode >> 6) & (0xff >> 2));
            q[4] = 0x80 | (unicode & (0xff >> 2));
            q = q + 5;
            n_size = n_size + 5;
        }
        else // 6 bytes.
        {    
            q[0] = 0xf0 | (unicode >> 30);
            q[1] = 0x80 | ((unicode >> 24) & (0xff >> 2));
            q[2] = 0x80 | ((unicode >> 18) & (0xff >> 2));
            q[3] = 0x80 | ((unicode >> 12) & (0xff >> 2));
            q[4] = 0x80 | ((unicode >> 6) & (0xff >> 2));
            q[5] = 0x80 | (unicode & (0xff >> 2));
            q = q + 6;
            n_size = n_size + 6;
        }
    }
    q[0] = '\0';
    std::string result(p);
    delete[] p;
    return result;
}


//string to unicode
void seq_lutf8_str_2_unicode_vec(const char* ps, std::vector<int>& vec) {
    const char* p = ps;
    unsigned char* q = NULL;

    vec.clear();
    int code = 0;

    while (p != NULL && strlen(p) > 0) {
        q = (unsigned char *)p;
       
        if (q[0] < 0xc0) // ascii;
        {
            code = (int)(q[0]);
            vec.push_back(code);
       //     std::cout<<"code:"<<code<<std::endl;   
            p++;
        }
        else if (q[0] < 0xE0) //2 byte char;
        {
            unsigned char t1 = q[0] & (0xff >> 3);
            unsigned char t2 = q[1] & (0xff >> 2);        
            code = t1 * 64 + t2;
            vec.push_back(code);
         //   std::cout<<"code:"<<code<<std::endl;        
            p = p + 2;
        }
        else if (q[0] < 0xF0) //3 byte char;
        {
            unsigned char t1 = q[0] & (0xff >> 4);
            unsigned char t2 = q[1] & (0xff >> 2);
            unsigned char t3 = q[2] & (0xff >> 2);
            code = t1 * 4096 + t2 * 64 + t3;
           // std::cout<<"code:"<<code<<std::endl;
            vec.push_back(code);               
            p = p + 3;
        }
        else if (q[0] < 0xF8) //4 byte char;
        {
            unsigned char t1 = q[0] & (0xff >> 5);
            unsigned char t2 = q[1] & (0xff >> 2);
            unsigned char t3 = q[2] & (0xff >> 2);
            unsigned char t4 = q[3] & (0xff >> 2);
            code = t1 * 262144 + t2 * 4096 + t3 * 64 + t4;
           // std::cout<<"code:"<<code<<std::endl;
            vec.push_back(code);
            p = p + 4;
        }
        else if (q[0] < 0xFC) //5 byte char.
        {
            unsigned char t1 = q[0] & (0xff >> 6);
            unsigned char t2 = q[1] & (0xff >> 2);
            unsigned char t3 = q[2] & (0xff >> 2);
            unsigned char t4 = q[3] & (0xff >> 2);
            unsigned char t5 = q[4] & (0xff >> 2);
            code = t1 * 16777216 + t2 * 262144 + t3 * 4096 + t4 * 64 + t5;
           // std::cout<<"code:"<<code<<std::endl;
            vec.push_back(code);        
            p = p + 5;
        }
        else //6 byte char.
        {
            unsigned char t1 = q[0] & (0xff >> 7);
            unsigned char t2 = q[1] & (0xff >> 2);
            unsigned char t3 = q[2] & (0xff >> 2);
            unsigned char t4 = q[3] & (0xff >> 2);
            unsigned char t5 = q[4] & (0xff >> 2);
            unsigned char t6 = q[5] & (0xff >> 2);
            code = t1 * 1073741824 + t2 * 16777216 + t3 * 262144 + t4 * 4096 + t5 * 64 + t6;
           // vec.push_back(code);        
            p = p + 6;
        }        
    }
}    

int check_chn(std::string character, bool &bchn) {
    std::vector<int> uni_code;
    seq_lutf8_str_2_unicode_vec(character.c_str(), uni_code);
    for (int i = 0; i < uni_code.size(); i++) {
        if (uni_code[i] >= 0xFEE0 && uni_code[i] < 0xFEE0 + 0x80) {
            uni_code[i] = uni_code[i] - 0xFEE0;
        }            
    }
    if (uni_code.size() > 0 &&
            uni_code[0] >= 0x4E00 && uni_code[0] <= 0x9FBB)
    {
        bchn = true;
        return 0;
    }
    bchn = false;
    return 0;

}

int check_num_str(std::string strLabel, bool &bNum)  {
    char plabel[4];
    int ilabel_len = strLabel.length();
    snprintf(plabel, 4, "%s", strLabel.data());

    bNum = false;
    for (int i = 0; i < ilabel_len; i++) {
        if (plabel[i] >= '0' && plabel[i] <= '9') {
            bNum = true;
            break;
        }
    }

    return 0;
}

int check_eng_str(std::string strLabel, bool &bEng, bool &bHighLowType)  {
    char plabel[4];
    int ilabel_len = strLabel.length();
    snprintf(plabel, 4, "%s", strLabel.data());

    bEng = false;
    for (int i = 0; i < ilabel_len; i++) {
        if (plabel[i] >= 'a' && plabel[i] <= 'z') {
            bHighLowType = false;
            bEng = true;
        }
        else if (plabel[i] >= 'A' && plabel[i] <= 'Z')  {
            bHighLowType = true;
            bEng = true;
        }
        if (bEng) {
            break;
        }
    }

    return 0;
}
bool check_punc(int code) {
    if (code >= 0xFEE0 && code < 0xFEE0 + 0x80) {
        code = code- 0xFEE0;
    }
    if (code >= 32 && code <= 47) {
        return true;
    }
    else if (code >= 58 && code <= 64) {
        return true;
    }
    else if (code >= 91 && code <= 96) {
        return true;
    }
    else if (code >= 123 && code <= 126) {
        return true;
    }

    // xieshufu 2015/12/10 refine
    // add 247: the code is one symbol
    switch (code) {
        case 183:
            return true;		   
        case 215:
            return true;
        case 247: 
            return true;
        case 8212:
            return true;
        case 8216:
            return true;
        case 8217:
            return true;
        case 8220:
            return true;
        case 8221:
            return true;
        case 8230:
            return true;
        case 12289:
            return true;
        case 12290:
            return true;
        case 12298:
            return true;
        case 12299:
            return true;
        case 12302:
            return true;
        case 12303:
            return true;
        case 12304:
            return true;
        case 12305:
            return true;
    }	

    return false;
}

// rotate the image with one angle
IplImage* rotate_image(IplImage* src, int angle, bool clockwise) {
    angle = abs(angle) % 180;
    if (angle > 90) {
        angle = 90 - (angle % 90);
    }
    IplImage* dst = NULL;
    int width = (double)(src->height * sin(angle * CV_PI / 180.0)) +
        (double)(src->width * cos(angle * CV_PI / 180.0)) + 1;
    int height = (double)(src->height * cos(angle * CV_PI / 180.0)) +
        (double)(src->width * sin(angle * CV_PI / 180.0)) + 1;
    int tempLength = sqrt(src->width * src->width + src->height * src->height) + 10;
    int tempX = (tempLength + 1) / 2 - src->width / 2;
    int tempY = (tempLength + 1) / 2 - src->height / 2;
    int flag = -1;

    dst = cvCreateImage(cvSize(width, height), src->depth, src->nChannels);
    cvZero(dst);
    IplImage* temp = cvCreateImage(cvSize(tempLength, tempLength), src->depth, src->nChannels);
    cvZero(temp);

    cvSetImageROI(temp, cvRect(tempX, tempY, src->width, src->height));
    cvCopy(src, temp, NULL);
    cvResetImageROI(temp);

    if (clockwise) {
        flag = 1;
    }
    float m[6];
    int w = temp->width;
    int h = temp->height;
    m[0] = (float) cos(flag * angle * CV_PI / 180.0);
    m[1] = (float) sin(flag * angle * CV_PI / 180.0);
    m[3] = -m[1];
    m[4] = m[0];
    m[2] = w * 0.5f;
    m[5] = h * 0.5f;
    CvMat M = cvMat(2, 3, CV_32F, m);
    cvGetQuadrangleSubPix(temp, dst, &M);
    cvReleaseImage(&temp);

    return dst;
}

void save_gbk_result(std::vector<SSeqLEngLineInfor> out_line_vec,\
    const char* savename, char *imgname, int imWidth, int imHeight)  {
    
    FILE *fp_ret_file = NULL;
    fp_ret_file = fopen(savename, "wt");
    if (fp_ret_file == NULL)  {
        printf("Can not open the file %s\n", savename);
        return;
    }
    fprintf(fp_ret_file, "%s\t%d\t%d\t\n", imgname, imWidth, imHeight);
 
    // choose the saving format for English row
    // 1: the same format as Chinese, each character is preserved e.g., "c h i n e s e"
    // 0: the English format and each word is preserved e.g., "chinese"
    int en_line_format = g_ini_file->get_int_value("EN_LINE_FORMAT",\
            IniFile::_s_ini_section_global);
    unsigned int out_line_vec_size = out_line_vec.size();
    
    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        printf("recognition result: [%d, %d, %d,%d] %s\n",\
                out_line_vec[i].left_, out_line_vec[i].top_,\
                out_line_vec[i].wid_, out_line_vec[i].hei_, out_line_vec[i].lineStr_.data());
    }

    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        std::string str_line_rst = "";
        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            str_line_rst += out_line_vec[i].wordVec_[j].wordStr_;
        }
        out_line_vec[i].lineStr_ = str_line_rst;
    }
    
    for (unsigned int i = 0; i < out_line_vec.size(); i++) {
        if (out_line_vec[i].lineStr_.length() <= 0) {
            continue ;
        }
        char *pgbk_str = NULL;
        pgbk_str = utf8_to_ansi(out_line_vec[i].lineStr_);
        fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t",\
                out_line_vec[i].left_, out_line_vec[i].top_,\
                out_line_vec[i].wid_, out_line_vec[i].hei_, pgbk_str);
        if (pgbk_str) {
            free(pgbk_str);
            pgbk_str = NULL;
        } 

        if (en_line_format == 0) {
            for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
                SSeqLEngWordInfor word = out_line_vec[i].wordVec_[j];
                char *pword_str = NULL;
                pword_str = utf8_to_ansi(word.wordStr_);
                fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t",\
                        word.left_, word.top_, word.wid_, word.hei_, pword_str);
                if (pword_str) {
                    free(pword_str);
                    pword_str = NULL;
                } 
            }
        } else if (en_line_format == 1) {
            for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
                SSeqLEngWordInfor word = out_line_vec[i].wordVec_[j];
                char *pword_str = NULL;

                //std::cout << word.wordStr_ << " " << std::endl;
                if (!word.left_add_space_) {
                    pword_str = utf8_to_ansi(word.wordStr_);
                    fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t",\
                            word.left_, word.top_, word.wid_, word.hei_, pword_str);
                    if (pword_str) {
                        free(pword_str);
                        pword_str = NULL;
                    } 
                } else {
                    for (int k = 0; k < word.charVec_.size(); k++) {
                        SSeqLEngCharInfor char_info = word.charVec_[k];
                        std::string char_str = char_info.charStr_;
                        pword_str = utf8_to_ansi(char_str);
                        fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t",
                          char_info.left_, char_info.top_,
                          char_info.wid_, char_info.hei_, pword_str);
                        
                        if (pword_str) {
                            free(pword_str);
                            pword_str = NULL;
                        } 
                    }
                }
            }

        }
        fprintf(fp_ret_file, "\n");
    }
    fclose(fp_ret_file);

    return;
}

void save_utf8_result(std::vector<SSeqLEngLineInfor> out_line_vec,\
    const char* savename, char *imgname, int imWidth, int imHeight)  {

    FILE *fp_ret_file = NULL;
    fp_ret_file = fopen(savename, "wt");
    if (fp_ret_file == NULL)  {
        printf("Can not open the file %s\n", savename);
        return;
    }
    fprintf(fp_ret_file, "%s\t%d\t%d\t\n", imgname, imWidth, imHeight);
 
    // choose the saving format for English row
    // 1: the same format as Chinese, each character is preserved e.g., "c h i n e s e"
    // 0: the English format and each word is preserved e.g., "chinese"
    unsigned int out_line_vec_size = out_line_vec.size();
    
    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        printf("recognition result: [%d, %d, %d,%d] %s\n",\
                out_line_vec[i].left_, out_line_vec[i].top_,\
                out_line_vec[i].wid_, out_line_vec[i].hei_, out_line_vec[i].lineStr_.data());
    }

    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        std::string str_line_rst = "";
        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            str_line_rst += out_line_vec[i].wordVec_[j].wordStr_;
        } 
        out_line_vec[i].lineStr_ = str_line_rst;
    }
    
    for (unsigned int i = 0; i < out_line_vec.size(); i++) {
        if (out_line_vec[i].lineStr_.length() <= 0) {
            continue ;
        }
        std::string line_str = out_line_vec[i].lineStr_;
        fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t",\
                out_line_vec[i].left_, out_line_vec[i].top_,\
                out_line_vec[i].wid_, out_line_vec[i].hei_, 
                line_str.data());

        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            SSeqLEngWordInfor word = out_line_vec[i].wordVec_[j];
            std::string word_str = word.wordStr_;
            fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t",\
                word.left_, word.top_, word.wid_, word.hei_, word_str.data());
        }
        fprintf(fp_ret_file, "\n");
    }
    fclose(fp_ret_file);

    return;
}

void save_utf8_result_prob(std::vector<SSeqLEngLineInfor> out_line_vec,\
    const char* savename, char *imgname, int imWidth, int imHeight)  {

    FILE *fp_ret_file = NULL;
    fp_ret_file = fopen(savename, "wt");
    if (fp_ret_file == NULL)  {
        printf("Can not open the file %s\n", savename);
        return;
    }
    fprintf(fp_ret_file, "%s\t%d\t%d\t\n", imgname, imWidth, imHeight);
 
    int out_line_vec_size = out_line_vec.size();
    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        std::string str_line_rst = "";
        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            str_line_rst += out_line_vec[i].wordVec_[j].wordStr_;
        }
        out_line_vec[i].lineStr_ = str_line_rst;
        std::cout << str_line_rst << std::endl;
    }
    
    for (unsigned int i = 0; i < out_line_vec.size(); i++) {
        if (out_line_vec[i].lineStr_.length() <= 0) {
            continue ;
        }
        std::string line_str = out_line_vec[i].lineStr_;
        fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\n",\
                out_line_vec[i].left_, out_line_vec[i].top_,\
                out_line_vec[i].wid_, out_line_vec[i].hei_, 
                line_str.data());

        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            SSeqLEngWordInfor word = out_line_vec[i].wordVec_[j];
            std::string word_str = word.wordStr_;
            double reg_prob = word.reg_prob;
            fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t%.4f\n",\
                word.left_, word.top_, word.wid_, word.hei_, word_str.data(),
                reg_prob);
        }
    }
    fclose(fp_ret_file);

    return;
}

void save_utf8_result_mul_cand(std::vector<SSeqLEngLineInfor> out_line_vec,\
    const char* savename, char *imgname, int imWidth, int imHeight) {
    
    FILE *fp_ret_file = NULL;
    fp_ret_file = fopen(savename, "wt");
    if (fp_ret_file == NULL)  {
        printf("Can not open the file %s\n", savename);
        return;
    }
    fprintf(fp_ret_file, "%s\t%d\t%d\t\n", imgname, imWidth, imHeight);
 
    unsigned int out_line_vec_size = out_line_vec.size();
    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        std::string str_line_rst = "";
        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            SSeqLEngWordInfor word = out_line_vec[i].wordVec_[j];
            str_line_rst += word.wordStr_;
            /*for (int k = 0; k < word.cand_word_vec.size(); k++) {
                str_line_rst += word.cand_word_vec[k].wordStr_;
            }*/
        }
        out_line_vec[i].lineStr_ = str_line_rst;
    }
    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        printf("recognition result: [%d, %d, %d,%d] %s\n",\
                out_line_vec[i].left_, out_line_vec[i].top_,\
                out_line_vec[i].wid_, out_line_vec[i].hei_, out_line_vec[i].lineStr_.data());
    }

    
    for (unsigned int i = 0; i < out_line_vec.size(); i++) {
        if (out_line_vec[i].lineStr_.length() <= 0) {
            continue ;
        }
        std::string line_str = out_line_vec[i].lineStr_;
        fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t",\
                out_line_vec[i].left_, out_line_vec[i].top_,\
                out_line_vec[i].wid_, out_line_vec[i].hei_, 
                line_str.data());

        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            SSeqLEngWordInfor word = out_line_vec[i].wordVec_[j];
            std::string word_str = word.wordStr_;
            fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t",\
                word.left_, word.top_, word.wid_, word.hei_, word_str.data());
            for (int k = 0; k < word.cand_word_vec.size(); k++) {
                fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t",\
                    word.cand_word_vec[k].left_, word.cand_word_vec[k].top_, 
                    word.cand_word_vec[k].wid_, word.cand_word_vec[k].hei_, 
                    word.cand_word_vec[k].wordStr_.data());
            }
        }
        fprintf(fp_ret_file, "\n");
    }
    fclose(fp_ret_file);

    return;
}

void save_utf8_result_mul_cand_menu(std::vector<SSeqLEngLineInfor> out_line_vec,\
    const char* savename, char *imgname, int imWidth, int imHeight) {
    
    FILE *fp_ret_file = NULL;
    fp_ret_file = fopen(savename, "wt");
    if (fp_ret_file == NULL)  {
        printf("Can not open the file %s\n", savename);
        return;
    }
    fprintf(fp_ret_file, "%s\t%d\t%d\t\n", imgname, imWidth, imHeight);
 
    unsigned int out_line_vec_size = out_line_vec.size();
    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        std::string str_line_rst = "";
        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            SSeqLEngWordInfor word = out_line_vec[i].wordVec_[j];
            str_line_rst += word.wordStr_;
            for (int k = 0; k < word.cand_word_vec.size(); k++) {
                str_line_rst += word.cand_word_vec[k].wordStr_;
            }
        }
        out_line_vec[i].lineStr_ = str_line_rst;
    }
    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        printf("recognition result: [%d, %d, %d,%d] %s\n",\
                out_line_vec[i].left_, out_line_vec[i].top_,\
                out_line_vec[i].wid_, out_line_vec[i].hei_, out_line_vec[i].lineStr_.data());
    }

    
    for (unsigned int i = 0; i < out_line_vec.size(); i++) {
        if (out_line_vec[i].lineStr_.length() <= 0) {
            continue ;
        }
        std::string line_str = out_line_vec[i].lineStr_;
        fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%d\t%s\n",\
                i, out_line_vec[i].left_, out_line_vec[i].top_,\
                out_line_vec[i].wid_, out_line_vec[i].hei_, 
                line_str.data());

        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            SSeqLEngWordInfor word = out_line_vec[i].wordVec_[j];
            std::string word_str = word.wordStr_;
            fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%d\t%s\t%.3f\t",\
                i, word.left_, word.top_, word.wid_, word.hei_, word_str.data(), \
				word.reg_prob);
            for (int k = 0; k < word.cand_word_vec.size(); k++) {
                fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t%.3f\t",\
                    word.cand_word_vec[k].left_, word.cand_word_vec[k].top_, 
                    word.cand_word_vec[k].wid_, word.cand_word_vec[k].hei_, 
                    word.cand_word_vec[k].wordStr_.data(),
					word.cand_word_vec[k].reg_prob);
            }
            fprintf(fp_ret_file, "\n");
        }
    }
    fclose(fp_ret_file);

    return;
}

void save_gbk_result_mul_cand(std::vector<SSeqLEngLineInfor> out_line_vec,\
    const char* savename, char *imgname, int imWidth, int imHeight) {
    
    FILE *fp_ret_file = NULL;
    fp_ret_file = fopen(savename, "wt");
    if (fp_ret_file == NULL)  {
        printf("Can not open the file %s\n", savename);
        return;
    }
    fprintf(fp_ret_file, "%s\t%d\t%d\t\n", imgname, imWidth, imHeight);
 
    unsigned int out_line_vec_size = out_line_vec.size();
    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        std::string str_line_rst = "";
        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            SSeqLEngWordInfor word = out_line_vec[i].wordVec_[j];
            str_line_rst += out_line_vec[i].wordVec_[j].wordStr_;
            for (int k = 0; k < word.cand_word_vec.size(); k++) {
                str_line_rst += word.cand_word_vec[k].wordStr_;
            }
        }
        out_line_vec[i].lineStr_ = str_line_rst;
    }
    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        printf("recognition result: [%d, %d, %d,%d] %s\n",\
                out_line_vec[i].left_, out_line_vec[i].top_,\
                out_line_vec[i].wid_, out_line_vec[i].hei_, out_line_vec[i].lineStr_.data());
    }
    
    for (unsigned int i = 0; i < out_line_vec.size(); i++) {
        if (out_line_vec[i].lineStr_.length() <= 0) {
            continue ;
        }
        char *pgbk_str = NULL;
        pgbk_str = utf8_to_ansi(out_line_vec[i].lineStr_);
        fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t",\
                out_line_vec[i].left_, out_line_vec[i].top_,\
                out_line_vec[i].wid_, out_line_vec[i].hei_, pgbk_str);
        if (pgbk_str) {
            free(pgbk_str);
            pgbk_str = NULL;
        } 

        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            SSeqLEngWordInfor word = out_line_vec[i].wordVec_[j];
            char *pword_str = NULL;
            pword_str = utf8_to_ansi(word.wordStr_);
            fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t",\
                word.left_, word.top_, word.wid_, word.hei_, pword_str);
            if (pword_str) {
                free(pword_str);
                pword_str = NULL;
            } 
            for (int k = 0; k < word.cand_word_vec.size(); k++) {
                char *new_str = NULL;
                new_str = utf8_to_ansi(word.cand_word_vec[k].wordStr_);
                fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t", \
                    word.cand_word_vec[k].left_, word.cand_word_vec[k].top_,
                    word.cand_word_vec[k].wid_, word.cand_word_vec[k].hei_, 
                    new_str);
                if (new_str) {
                    delete []new_str;
                    new_str = NULL;
                }
            }
        }
        fprintf(fp_ret_file, "\n");
    }
    fclose(fp_ret_file);

    return;
}

char* utf8_to_ansi(const std::string from) {
    char *inbuf = const_cast<char*>(from.c_str());
    size_t inlen = strlen(inbuf);
    size_t outlen = inlen * 4;
    char *outbuf = (char *)malloc(inlen * 4);

    bzero(outbuf, inlen * 4);
    char *in = inbuf;
    char *out = outbuf;
    
    iconv_t cd = iconv_open("GBK", "UTF-8");
    iconv(cd, &in, &inlen, &out, &outlen);
    iconv_close(cd);

    return outbuf;
}

int get_img_list(std::string input_dir, std::vector<std::string> &img_name_v) {
    DIR *dir = NULL;
    struct dirent* ptr = NULL;
    if ((dir = opendir(input_dir.c_str())) == NULL) {
        std::cerr << "Open image dir failed. Dir: " << input_dir.c_str() << std::endl;
        return -1;
    }
    while (ptr = readdir(dir)) {
        if (ptr->d_name[0] == '.') {
            continue;
        }
        std::string str_file_name = std::string(ptr->d_name);
        if (str_file_name.length() < 5){
            std::cerr << "Wrong image file format: " << str_file_name << std::endl;
            continue;
        }
        else{
            std::string post_str = str_file_name.substr(str_file_name.length() - 4, 4);
            if (post_str.compare(".jpg") != 0 && post_str.compare(".png") != 0){
                std::cerr << "Wrong image file format: " << str_file_name << std::endl;
                continue;
            }
        }
        img_name_v.push_back(str_file_name);
    }
    closedir(dir);

    return 0;
}

void save_utf8_result_batch(std::vector<SSeqLEngLineInfor> out_line_vec,\
    std::vector<std::string> img_name_v,
    const char* savename)  {

    FILE *fp_ret_file = NULL;
    fp_ret_file = fopen(savename, "wt");
    if (fp_ret_file == NULL)  {
        printf("Can not open the file %s\n", savename);
        return;
    }
 
    unsigned int out_line_vec_size = out_line_vec.size();
/*    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        printf("%s: [%d, %d, %d,%d] %s\n",\
            img_name_v[i].data(),
            out_line_vec[i].left_, out_line_vec[i].top_,\
            out_line_vec[i].wid_, out_line_vec[i].hei_, out_line_vec[i].lineStr_.data());
    }*/

    for (unsigned int i = 0; i < out_line_vec.size(); i++) {
        if (out_line_vec[i].lineStr_.length() <= 0) {
            continue ;
        }
        std::string line_str = out_line_vec[i].lineStr_;
        fprintf(fp_ret_file, "%s\t%d\t%d\t%d\t%d\t%s\t",\
                img_name_v[i].data(),\
                out_line_vec[i].left_, out_line_vec[i].top_,\
                out_line_vec[i].wid_, out_line_vec[i].hei_, 
                line_str.data());

        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            SSeqLEngWordInfor word = out_line_vec[i].wordVec_[j];
            std::string word_str = word.wordStr_;
            fprintf(fp_ret_file, "%d\t%d\t%d\t%d\t%s\t",\
                word.left_, word.top_, word.wid_, word.hei_, word_str.data());
        }
        fprintf(fp_ret_file, "\n");
    }
    fclose(fp_ret_file);

    return;
}

void post_proc_line_recog_result(std::vector<SSeqLEngLineInfor> &out_line_vec) {
    int out_line_vec_size = out_line_vec.size();
    if (out_line_vec_size <= 1) {
        return ;
    }
    for (int i = 0; i < out_line_vec_size; i++) {
        SSeqLEngLineInfor& line_a = out_line_vec[i];
        int line_a_x0 = line_a.left_;
        int line_a_y0 = line_a.top_;
        int line_a_x1 = line_a_x0 + line_a.wid_ - 1;
        int line_a_y1 = line_a_y0 + line_a.hei_ - 1;
        for (int j = i + 1; j < out_line_vec_size; j++) {
            SSeqLEngLineInfor& line_b = out_line_vec[j];
            int line_b_x0 = line_b.left_;
            int line_b_y0 = line_b.top_;
            int line_b_x1 = line_b_x0 + line_b.wid_ - 1;
            int line_b_y1 = line_b_y0 + line_b.hei_ - 1;
            if (line_b_x0 > line_a_x1 || line_b_x1 < line_a_x0
                 || line_b_y0 > line_a_y1 || line_b_y1 < line_a_y0) {
                continue ;
            }
            int x_len = std::max(line_a_x1, line_b_x1) -
                std::min(line_a_x0, line_b_x0) + 1;
            int y_len = std::max(line_a_y1, line_b_y1) - 
                std::min(line_a_y0, line_b_y0) + 1;
            if (x_len >= line_a.wid_ + line_b.wid_ ||
                    y_len >= line_a.hei_ + line_b.hei_) {
                continue ;
            }
            merge_two_line(line_a, line_b);        
        }
    }
}

void merge_two_line(SSeqLEngLineInfor& line_a, SSeqLEngLineInfor& line_b) {
    int line_a_size = line_a.wordVec_.size();
    int line_b_size = line_b.wordVec_.size();
    int *pword_a_flag = new int[line_a_size];
    int *pword_b_flag = new int[line_b_size];

    memset(pword_a_flag, 0, sizeof(int) * line_a_size);
    memset(pword_b_flag, 0, sizeof(int) * line_b_size);

    for (int index_a = 0; index_a < line_a_size; index_a++) {
        SSeqLEngWordInfor& word_a = line_a.wordVec_[index_a];
        int word_a_x0 = word_a.left_;
        int word_a_y0 = word_a.top_;
        int word_a_x1 = word_a_x0 + word_a.wid_ - 1;
        int word_a_y1 = word_a_y0 + word_a.hei_ - 1;
        for (int index_b = 0; index_b < line_b_size; index_b++) {
            SSeqLEngWordInfor& word_b = line_b.wordVec_[index_b];
            int word_b_x0 = word_b.left_;
            int word_b_y0 = word_b.top_;
            int word_b_x1 = word_b_x0 + word_b.wid_ - 1;
            int word_b_y1 = word_b_y0 + word_b.hei_ - 1;
            if (word_b_x0 > word_a_x1 || word_b_x1 < word_a_x0) {
                continue;
            }
            int x_len = std::max(word_a_x1, word_b_x1) 
                - std::min(word_a_x0, word_b_x0) + 1;
            int y_len = std::max(word_a_y1, word_b_y1) 
                - std::min(word_a_y0, word_b_y0) + 1;
            int x_overlap = word_a.wid_ + word_b.wid_ - x_len;
            int y_overlap = word_a.hei_ + word_b.hei_ - y_len;
            float mean_wid_t = (word_a.wid_ + word_b.wid_) / 4.0;
            float mean_hei_t = (word_a.hei_ + word_b.hei_) / 4.0;
            if (x_overlap > mean_wid_t && y_overlap > mean_hei_t) {
                // preserve the word with high confidence  
                if (word_a.regLogProb_ >= word_b.regLogProb_) {
                    pword_b_flag[index_b] = 1;
                } else {
                    pword_a_flag[index_a] = 1;
                }
            }
        }
    }
    // erase the word in vector a
    std::vector<SSeqLEngWordInfor>::iterator it_a = line_a.wordVec_.begin();
    int index_a = 0;
    while (it_a != line_a.wordVec_.end()) {
        if (pword_a_flag[index_a] == 1) {
            it_a = line_a.wordVec_.erase(it_a);
        } else {
            it_a++;
        }
        index_a++;
    }
    // erase the word in vector b
    std::vector<SSeqLEngWordInfor>::iterator it_b = line_b.wordVec_.begin();
    int index_b = 0;
    while (it_b != line_b.wordVec_.end()) {
        if (pword_b_flag[index_b] == 1) {
            it_b = line_b.wordVec_.erase(it_b);
        } else {
            it_b++;
        }
        index_b++;
    }
    // free the memory
    if (pword_a_flag) {
        delete []pword_a_flag;
        pword_a_flag = NULL;
    }
    if (pword_b_flag) {
        delete []pword_b_flag;
        pword_b_flag = NULL;
    }

    return ;
}

