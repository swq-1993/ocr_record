/**
 * @file SeqLEngWordRecog.cpp
 * Author: hanjunyu(hanjunyu@baidu.com)
 * Create on: 2014-07-02
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 *  
 **/

#include "SeqLEngWordRecog.h"
#include "predictor/seql_predictor.h"
#include "iconv.h"
#include "common_func.h"
#include "dll_main.h"
#include <sys/time.h>

int g_predict_num = 0;
char g_pimage_title[256];
char g_psave_dir[256] = "./temp";
int g_iline_index = 0;
extern int g_irct_index;

void savePredictData(int predict_num, float * data, int width, int height, int channel, float seqLCdnnMean ){
   char saveName[256];
   sprintf(saveName, "./savePredictDir/%d.txt",predict_num);
   std::cout<<" saved..." << saveName << std::endl;
   FILE *fd = fopen(saveName, "wb");
   if(fd==NULL){
      std::cerr << " Can not open " << saveName << std::endl;
   }
   fwrite(&width, sizeof(int), 1,  fd);
   fwrite(&height, sizeof(int), 1,  fd);
   fwrite(&channel, sizeof(int), 1,  fd);
   
   int dataLen = width*height*channel;
   for(int i=0; i<dataLen; i++){
      data[i] = data[i]+seqLCdnnMean;
   }
   
   fwrite(data, sizeof(float), dataLen,  fd);
   
   for(int i=0; i<dataLen; i++){
      data[i] = data[i]-seqLCdnnMean;
   }
   
   fclose(fd);
   return;
}


void loadPredictMatrix(int predict_num, float* & matrix, int & len){
   char loadName[256];
   sprintf(loadName, "./loadPredictDir/%d.txt",predict_num);
   //std::cout<<" loaded..." << loadName << std::endl;
   FILE *fd = fopen(loadName, "rb");
   if(fd==NULL){
      std::cerr << " Can not open " << loadName << std::endl;
   }
   
   fread(&len, sizeof(int), 1, fd);
   matrix = new float[len];
   fread(matrix, sizeof(float), len, fd);

   fclose(fd);
   return;
}


int SeqLUnicode2GBK(wchar_t uniChar, char *gbkChar)
{
    iconv_t cd = iconv_open( "gbk", "wchar_t" ); 
    if( cd == iconv_t(-1) )
        return 0.0f;

    wchar_t unicodes[2];
    unicodes[0] = uniChar;
    char *unicode = (char*)unicodes;
    char *tmpGbk  = gbkChar;
    size_t inLen  = 4;
    size_t outLen = 4; 

    int ret = iconv( cd, &unicode, &inLen, &tmpGbk, &outLen );
    if( ret != 0  )
    {
        iconv_close( cd );
        return 0; 
    }
  
    if( gbkChar[0] < 0 )
        gbkChar[2] = 0;
    else
        gbkChar[1] = 0;

    iconv_close( cd );

    return 0;    
}

int SeqLSCWToX(const wchar_t *sIn, int nInLen, char *codeType, char *&sOut)
{
    char code[100]; 
    strcpy(code, codeType);

    iconv_t cd;
    cd = iconv_open(code, "wchar_t");
    if (cd == (iconv_t)(-1))
    {
        sOut = NULL;
        return -1;
    }
    int nMaxOutLen = nInLen;
    char *pTmp = (char *)malloc(nMaxOutLen + 1);
    memset(pTmp, 0, nMaxOutLen + 1);
    char *pIn = (char *)sIn;
    char *pOut = pTmp;
    size_t nLeftLen = nMaxOutLen;
    size_t nSrcLen = nInLen;

    size_t ret = 0;
    ret = iconv(cd, &pIn, &nSrcLen, &pOut, &nLeftLen);
    if (ret != 0)
    {
        sOut = NULL;
        free(pTmp);
        pTmp = NULL;
        iconv_close(cd);
        return -1;
    }

    pTmp[nMaxOutLen-nLeftLen] = '\0';
    sOut = (char *)malloc(nMaxOutLen - nLeftLen + 1);
    strcpy(sOut, pTmp);

    free(pTmp);
    pTmp = NULL;
    iconv_close(cd);

    return 0;
}

int SeqLUnicode2UTF8(wchar_t uniChar, char *utf8Char) {
    wchar_t tmp[4];
    tmp[0] = uniChar;
    tmp[1] = 0;
    char *charRes = NULL;
    SeqLSCWToX(tmp, wcslen(tmp)*sizeof(wchar_t), "utf-8", charRes);
    if (charRes != NULL)
    {
        strcpy(utf8Char, charRes);
        free(charRes);
        charRes = NULL;
    }
}

int SeqLEuroUnicodeHigh2Low(int A)
{   // por rus spa fra ger ita 
    if (A == 215) { // x symbol
        return A;
    } else if ((A >= 65 && A <= 90) || (A >= 192 && A <= 220) || (A >= 1040 && A <= 1071)) {
        return A + 32;
    }
    else if (A == 1025) {
        return 1105;
    }
    else if (A == 376) {
        return 255;
    }
    else {
        return A;
    }
}

int SeqLEuroUnicodeLow2High(int a) 
{
    if (a == 247) {
        return a;
    } else if ((a >= 97 && a <= 122) || (a >= 224 && a <= 252) || (a >= 1072 && a <= 1103)) {
        return a - 32;
    }
    else if (a == 1105) {
        return 1025;
    }
    else if (a == 255) {
        return 376;
    }
    else {
        return a;
    }
}

void SeqLUTF8Str2UnicodeVec( const char* ps, std::vector<int>& vec )
{
    const char* p = ps;
    unsigned char* q = NULL;
    char szCharacter[10];

    vec.clear();
    int code = 0;

    while (p != NULL && strlen( p ) > 0)
    {
        q = (unsigned char *) p;

        if (q[0] < 0xc0) // ascii;
        {
            code = (int)(q[0]);
            vec.push_back(code);   
            p++;
        }
        else if (q[0] < 0xE0) //2 byte char;
        {
            unsigned char t1 = q[0] & (0xff >> 3);
            unsigned char t2 = q[1] & (0xff >> 2);        
            code = t1 * 64 + t2;
            vec.push_back(code);        
            p = p + 2;
        }
        else if (q[0] < 0xF0) //3 byte char;
        {
            unsigned char t1 = q[0] & (0xff >> 4);
            unsigned char t2 = q[1] & (0xff >> 2);
            unsigned char t3 = q[2] & (0xff >> 2);
            code = t1 * 4096 + t2 * 64 + t3;
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
            vec.push_back(code);        
            p = p + 6;
        }        
    }
}    


char* SeqLUnicodeVec2UTF8Str( std::vector<int>& vec)
{
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
    return p;
}

bool SeqLEuroUnicodeIsPunct(int unicode){
    if (unicode >= 0x20 && unicode <= 0x2F)
    {
        return true;
    }
    else if (unicode >= 0x3A && unicode <= 0x40)
    {
        return true;
    }
    else if (unicode >= 0x5B && unicode <= 0x60)
    {
        return true;
    }
    else if (unicode >= 0xA0 && unicode <= 0xBF)
    {
        return true;
    }
    else if (unicode >= 0x2000 && unicode <= 0x206F)
    {
        return true;
    }
    else if (unicode >= 0x2E00 && unicode <= 0x2E7F)
    {
        return true;
    }
    else
    {
        return false;
    }
    return false;
}
// 48~57
bool SeqLUnicodeIsDigitChar(int unicode){
    if (unicode>=48 && unicode<=57){
        return true;
    }
    return false;
}

bool SeqLUnicodeIsEngChar(int unicode){
    if ((unicode>='A'&& unicode<='Z') || \
            (unicode>='a'&& unicode<='z')){
        return true;
    }
    else {
        return false;
    }
}



char* SeqLEuroUTF8StrHigh2Low(const char* ps)
{
    std::vector<int> univec;
    SeqLUTF8Str2UnicodeVec(ps, univec);
    
    for (int i = 0; i < univec.size(); i++) {
        univec[i] = SeqLEuroUnicodeHigh2Low(univec[i]);
    }
    
    char* out = SeqLUnicodeVec2UTF8Str(univec);
    return out;
}

char* SeqLEuroUTF8StrLow2High(const char* ps)
{
    std::vector<int> univec;
    SeqLUTF8Str2UnicodeVec(ps, univec);
    
    for (int i = 0; i < univec.size(); i++) {
        univec[i] = SeqLEuroUnicodeLow2High(univec[i]);
    }
    
    char* out = SeqLUnicodeVec2UTF8Str(univec);
    return out;
}


int CSeqLEngWordRegModel::seqLCDNNModelTableInit(char *modelPath, char *tablePath, float mean, int height, \
        void *& cdnnModel, unsigned short *&cdnnTable, float &cdnnMean, int &cdnnNormHeight){
    /* Initialize the recognizing model and input, output dims.
     **/
    inference_seq::init(/* model_path */ modelPath,
                    /* model */cdnnModel,
                    /* use_gpu */_use_gpu,
                    /* use_paddle */ _use_paddle);
    dataDim_ = inference_seq::get_input_dim(cdnnModel, _use_paddle);
    labelsDim_ = inference_seq::get_output_dim(cdnnModel, _use_paddle);
    std::cout<<"labelsDim_:"<<labelsDim_<<std::endl;
    cdnnTable = (unsigned short *) malloc(labelsDim_ * sizeof(float));
    cdnnMean = mean;
    cdnnNormHeight = height;

    FILE * fpTable = NULL;
    if( NULL == (fpTable = fopen(tablePath, "r"))){
      fprintf(stderr, "open table file error. \n");
      return -1;
    }
    
    tabelStrVec_.resize(labelsDim_ - 1);
    tabelLowStrVec_.resize(labelsDim_ - 1);
    for (int i = 0; i < labelsDim_-1; i++)
    {
        int code;
        int index;
        fscanf(fpTable, "%d", &code);
        fscanf(fpTable, "%d", &index);
        //full t0 half
        if( code >=0xFEE0  &&  code < 0xFEE0 + 0x80)
            code = code - 0xFEE0;
        cdnnTable[index] = (unsigned short)code;
        
        char gbkStr[4];
        SeqLUnicode2GBK(code, gbkStr);
        
        if (code== SPACECODE) {
           strcpy(gbkStr, " ");  
        }

        tabelStrVec_[i] = gbkStr;
        
        for(unsigned int j=0; j<strlen(gbkStr); j++){
          if( gbkStr[j]>='A' && gbkStr[j]<='Z'){
             gbkStr[j] = gbkStr[j]-'A' + 'a';
          }
        }
        tabelLowStrVec_[i] = gbkStr;

    }
    fclose(fpTable);
    blank_ = labelsDim_ - 1;
    //blank_ = labelsDim_ - 3;
}

int CSeqLEngWordRegModel::seqLCDNNModelTableInitUTF8(char *modelPath, char *tablePath, float mean, int height, \
        void *& cdnnModel, unsigned short *&cdnnTable, float &cdnnMean, int &cdnnNormHeight, char * langType){
    /* Initialize the recognizing model and input, output dims.
     * Set the default initialize parameter.
     **/
    inference_seq::init(/* modelPath */ modelPath,
                    /* model */ cdnnModel,
                    /* use_gpu */ _use_gpu,
                    /* ue_paddle */ _use_paddle);
    dataDim_ = inference_seq::get_input_dim(cdnnModel, _use_paddle);
    labelsDim_ = inference_seq::get_output_dim(cdnnModel, _use_paddle);

    cdnnTable = (unsigned short *) malloc(labelsDim_ * sizeof(float));
    cdnnMean = mean;
    cdnnNormHeight = height;

    FILE * fpTable = NULL;
    if (NULL == (fpTable = fopen(tablePath, "r"))) {
      fprintf(stderr, "open table file error. \n");
      return -1;
    }
    tabelStrVec_.resize(labelsDim_ - 1);
    tabelLowStrVec_.resize(labelsDim_ - 1);
    for (int i = 0; i < labelsDim_-1; i++)
    {
        int code = 0;
        int index = 0;
        fscanf(fpTable, "%d", &code);
        fscanf(fpTable, "%d", &index);
       
        //full t0 half
        if (code >=0xFEE0  &&  code < 0xFEE0 + 0x80)
            code = code - 0xFEE0;
        cdnnTable[index] = (unsigned short)code;
      
        char utf8Str[6];
        SeqLUnicode2UTF8(code, utf8Str);
        if (code == SPACECODE) {
           strcpy(utf8Str, " ");  
        }
        tabelStrVec_[i] = utf8Str;
       
        if (true || strcmp(langType,"Euro")) {
            int lowCode = SeqLEuroUnicodeHigh2Low(code);
            SeqLUnicode2UTF8(lowCode, utf8Str);
            if (code == SPACECODE) {
               strcpy(utf8Str, " ");  
            }
            tabelLowStrVec_[i] = utf8Str;
        }
    }
    fclose(fpTable);
    blank_ = labelsDim_ - 1;
}

int CSeqLEngWordRegModel::seqLCDNNModelTableRelease( void **cdnnModel, unsigned short **cdnnTable){
    if (cdnnTable != NULL && *cdnnTable != NULL) {
        free(*cdnnTable);
        *cdnnTable = NULL;
    }

    /* Relase the model-related memory.
     **/
    return inference_seq::release(cdnnModel, _use_paddle);
}

void CSeqLEngWordReg::resizeImageToFixHeight(IplImage * & image, const int resizeHeight, float horResizeRate){
     if(resizeHeight>0){
         CvSize sz;
         sz.height = resizeHeight;
         sz.width = image->width * resizeHeight * 1.0/ image->height*horResizeRate;
         IplImage *dstImg = cvCreateImage(sz, image->depth, image->nChannels);
         cvResize(image,dstImg, CV_INTER_CUBIC);

         cvReleaseImage(& image);
         image = dstImg;
         dstImg = NULL;
     }
}

void CSeqLEngWordReg::extendSlimImage(IplImage * & image, float widthRate){
    int goalWidth = widthRate*image->height;
    if(image->width < goalWidth ){
        IplImage *dstImg = cvCreateImage(cvSize(goalWidth, image->height), image->depth, image->nChannels);
        int shiftProject = (dstImg->width - image->width)/2;
        for(int h=0; h<dstImg->height; h++){
          for(int w=0; w<dstImg->width; w++){
              for(int c=0; c<dstImg->nChannels; c++)
              {
                  int projectW = 0;
                  if(w<shiftProject){
                      projectW = 0;
                  }
                  else if( w>= (image->width + shiftProject)){
                      projectW = image->width - 1;
                  }
                  else
                  {
                      projectW = w-shiftProject;
                  }
                  CV_IMAGE_ELEM(dstImg, uchar, h, dstImg->nChannels*w + c)   = 
                            CV_IMAGE_ELEM(image, uchar, h, image->nChannels*projectW + c);
              }
          }
        }
            
         cvReleaseImage(& image);
         image = dstImg;
         dstImg = NULL;
    }
}

void CSeqLEngWordReg::extractDataOneRect(IplImage * image, SSeqLEngRect & tempRect, \
     float *& data, int & width, int & height, int & channel, bool isBlackWdFlag, float horResizeRate  )
{
    assert( tempRect.left_ >=0 && tempRect.top_ >=0);
    assert( (tempRect.left_+tempRect.wid_)<image->width );
    assert( (tempRect.top_+tempRect.hei_)<image->height );
    
    IplImage* subImage = cvCreateImage( cvSize(tempRect.wid_, tempRect.hei_), image->depth, image->nChannels );
    cvSetImageROI(image, cvRect(tempRect.left_, tempRect.top_, tempRect.wid_, tempRect.hei_));
    cvCopy(image,subImage);
    cvResetImageROI(image);
    IplImage* grayImage = cvCreateImage( cvSize(subImage->width, subImage->height), \
            subImage->depth, 1);
    if( subImage->nChannels !=1)
    {
      cvCvtColor( subImage, grayImage, CV_BGR2GRAY);
    }
    else
    {
      cvCopy(subImage, grayImage);
    }
    
    resizeImageToFixHeight(grayImage, seqLNormheight_, horResizeRate);  
    _m_norm_img_h = grayImage->height;
    _m_norm_img_w = grayImage->width;
    extendSlimImage(grayImage, 1.0);
    assert(grayImage->height == fixHeight);


    data = (float *)malloc( grayImage->width*grayImage->height*grayImage->nChannels*sizeof(float));
    int index = 0;
    if(isBlackWdFlag==1 ){
        for(int h=0; h<grayImage->height; h++){
            for(int w=0; w<grayImage->width; w++){
                for(int c=0; c<grayImage->nChannels; c++){
                     float value = cvGet2D(grayImage,h,w).val[c];
                    data[index] = value - seqLCdnnMean_;
                    index ++;
                }
            }
        }
    }
    else
    {
        for(int h=0; h<grayImage->height; h++){
            for(int w=0; w<grayImage->width; w++){
                for(int c=0; c<grayImage->nChannels; c++){
                     float value = cvGet2D(grayImage,h,w).val[c];
                    data[index] = (255-value) - seqLCdnnMean_;
                    index ++;
                }
            }
        }
    }

    width = grayImage->width;
    height = grayImage->height;
    channel = grayImage->nChannels;
    
    cvReleaseImage(&grayImage);
    cvReleaseImage(&subImage);
}

void CSeqLEngWordReg::extract_data_rect_asia(const IplImage *image, SSeqLEngRect & tempRect, \
     float *& data, int & width, int & height,\
     int & channel, bool isBlackWdFlag, float horResizeRate)
{
    IplImage tmp_image = IplImage(*image);
    IplImage* sub_image = cvCreateImage(cvSize(tempRect.wid_, tempRect.hei_),\
            image->depth, image->nChannels);
    cvSetImageROI(&tmp_image, cvRect(tempRect.left_, tempRect.top_, tempRect.wid_, tempRect.hei_));
    cvCopy(&tmp_image, sub_image);
    cvResetImageROI(&tmp_image);
    IplImage* gray_image = cvCreateImage(cvSize(sub_image->width, sub_image->height), \
            sub_image->depth, 1);
    if (sub_image->nChannels != 1) {
        cvCvtColor(sub_image, gray_image, CV_BGR2GRAY);
    } else {
        cvCopy(sub_image, gray_image);
    }
    
    resizeImageToFixHeight(gray_image, seqLNormheight_, horResizeRate);  
    _m_norm_img_h = gray_image->height;
    _m_norm_img_w = gray_image->width;
    extendSlimImage(gray_image, 1.0);
    
    int extract_data_flag = g_ini_file->get_int_value("DRAW_DATA_FOR_RECOG",
            IniFile::_s_ini_section_global);
    if (extract_data_flag == 1) {
        // save the image data to check
        char file_path[256];
        snprintf(file_path, 256, "%s/%s-%d-rect-norm-%d-%d.jpg",\
                g_psave_dir, g_pimage_title,\
                g_iline_index, g_irct_index, _m_en_ch_model);
        cvSaveImage(file_path, gray_image);
    }
    data = (float *)malloc(gray_image->width * gray_image->height \
            * gray_image->nChannels * sizeof(float));
    int index = 0;
    if (isBlackWdFlag == 1) {
        for (int h = 0; h < gray_image->height; h++) {
            for (int w = 0; w < gray_image->width; w++) {
                for (int c = 0; c < gray_image->nChannels; c++) {
                    float value = cvGet2D(gray_image, h, w).val[c];
                    data[index] = value - seqLCdnnMean_;
                    index++;
                }
            }
        }
    } else {
        for (int h = 0; h < gray_image->height; h++) {
            for (int w = 0; w < gray_image->width; w++) {
                for (int c = 0; c < gray_image->nChannels; c++) {
                    float value = cvGet2D(gray_image, h, w).val[c];
                    data[index] = (255 - value) - seqLCdnnMean_;
                    index++;
                }
            }
        }
    }

    width = gray_image->width;
    height = gray_image->height;
    channel = gray_image->nChannels;
    cvReleaseImage(&gray_image);
    cvReleaseImage(&sub_image);

    return ;
}

int CSeqLEngWordReg::argMaxActs(float* acts, int numClasses) {
     int  maxIdx = 0;
     float maxVal = acts[maxIdx];
     for (int i = 0; i < numClasses; ++i) {
         if (maxVal < acts[i]) {
             maxVal = acts[i];
             maxIdx = i;
         }
     }
     return maxIdx;
 }

void CSeqLEngWordReg::printInputData(float *data, int len){
    
    for (int i = 0; i < len; i++) {
        printf("(%d,%f)\t",i,data[i]);
    }
    printf("\n");

}
void CSeqLEngWordReg::printOneOutputMatrix(float * mat, int LabelDim, int len){
    int classNum = LabelDim ;
    printf("classNum = %d, totalTime = %d ,%d\n", classNum, len, (len %(classNum)));
    assert( (len %(classNum)) == 0);
    for(int i=0; i< len/classNum; i++){
       float * acts = mat + i*classNum;
       int actIndex = argMaxActs(acts, classNum);
       printf("Time %d, maxLabel %d\n", i, actIndex);
       for(int j=0; j<classNum; j++){
         printf("(%d,%f)\t", j, acts[j]);
       }
       printf("\n");
    }
}

void print_all_matrix(std::vector<float *> out_mat, int LabelDim, std::vector<int> out_len, 
                std::vector<int*> out_labels){
    int class_num = LabelDim ;
    int col_num = out_mat.size();
    printf("Mat column num: %d\n", col_num);
    for (int i = 0; i < col_num; i++) {
        float * mat = out_mat[i];
        int len = out_len[i];
        printf("col_id = %d, totalTime = %d\n", i, len);
        for (int j = 0; j < len; j++) {
            printf("(%d,%f)\t", j, mat[j]);
        }
        printf("\n");
    }
    for (int i = 0; i < out_labels.size(); i++) {
        int *label = out_labels[i];
        int j = 0;
        int res = 0;
        do {
            res = label[j];
            printf("%d %d\n", j, res);
            j++;
        } while (res > -1);
        printf("\n");
    }

}

std::vector<int> CSeqLEngWordReg::pathElimination( const std::vector<int> & path, int blank ){
  std::vector<int> str;
  str.clear();
  int prevLabel = -1;
  for (std::vector<int>::const_iterator label = path.begin(); label != path.end(); label++) {
      if (*label != blank && (str.empty() || *label != str.back() || prevLabel == blank)) {
          str.push_back(*label);
      }
      prevLabel = *label;
  }
  return str;
}

std::vector<int> CSeqLEngWordReg::path_elimination(std::vector<int> path, std::vector<int> & path_pos){
  std::vector<int> str;
  str.clear();
  path_pos.clear();
  int prevLabel = -1;
  for (int i = 0; i < path.size(); ++i) {
      int label = path[i];
      if (label != blank_ && (str.empty() || label != str.back() || prevLabel == blank_)) {
          str.push_back(label);
          path_pos.push_back(i);
      }
      prevLabel = label;
  }
  return str;
}

std::string CSeqLEngWordReg::pathToString(std::vector<int>& path ){
   std::string str = "";
   for(unsigned int i=0; i<path.size(); i++){
      str += tabelLowStrVec_[path[i]]; 
      //str += tabelStrVec_[path[i]]; 
   }
   return str;
}

std::string CSeqLEngWordReg::pathtoaccuratestring(std::vector<int>& path) {
    std::string str = "";
    for (unsigned int i = 0; i < path.size(); i++) {
      //str += tabelLowStrVec_[path[i]]; 
        str += tabelStrVec_[path[i]]; 
    }
    return str;
}

std::string CSeqLEngWordReg::pathToStringHaveBlank(std::vector<int>& path ){
   std::string str = "";
   for(unsigned int i=0; i<path.size(); i++){
       if(path[i]!=blank_){
        str += tabelLowStrVec_[path[i]]; 
       }
       else{
        str += "_"; 
       }
      //str += tabelStrVec_[path[i]]; 
   }
   return str;
}

std::vector<int>  CSeqLEngWordReg::stringToPath(const std::string & str) {
   std::vector<int> path;
   path.resize(str.length());
   for(unsigned int i=0; i< str.length(); i++){
      unsigned int index = 0;
      for(index=0; index<tabelStrVec_.size();index++){
        if( str[i] == tabelStrVec_[index][0]){
           break;
        }
      }
      assert( index != tabelStrVec_.size());
      path[i] = index;
   }
   return path;
}

std::vector<int>  CSeqLEngWordReg::stringutf8topath(const std::string & str) {
   std::vector<int> unicodeVec;
   SeqLUTF8Str2UnicodeVec( str.c_str(), unicodeVec);
   std::vector<int> path;
   path.resize(unicodeVec.size());
   for (unsigned int i=0; i<unicodeVec.size(); i++) {
       unsigned int index = 0;
       for (index=0; index< labelsDim_; index++) {
           if (seqLCdnnTable_[index] == unicodeVec[i]) {
              break;
           }    
       }
       assert(index != labelsDim_);
       path[i] = index;
   }
   return path;
}

float CSeqLEngWordReg :: onePrefixPathUpLowerLogProb(std::string str,\
        float * orgActs, int actLen, int & highLowType){

     transform( str.begin(), str.end(), str.begin(), ::tolower );
     
     float logLow =onePrefixPathLogProb(str, \
               orgActs, actLen);
     float maxProb = logLow;
     highLowType = 0;

    
     float logUpHead = safeLog(0);
     if(str.length()>1){
        transform( str.begin(), str.begin()+1, str.begin(), ::toupper );
        logUpHead =  onePrefixPathLogProb(str, \
               orgActs, actLen);
     if(logUpHead > maxProb){
        maxProb = logUpHead;
        highLowType = 1;
     }
     } 

     transform( str.begin(), str.end(), str.begin(), ::toupper );
     float logUp = onePrefixPathLogProb(str, \
               orgActs, actLen);

     if(logUp > maxProb){
        maxProb = logUp;
        highLowType = 2;
     }

     return logAdd(logUpHead, logAdd(logLow, logUp));

}

float CSeqLEngWordReg :: onePrefixPathLogProb(const std::string & str, float * orgActs, int actLen){
    std::vector<int> path = stringToPath(str);
    int timeNum = actLen/labelsDim_;
    PrefixPath tempPrefix = onePrefixPathInfor(path, orgActs, \
         labelsDim_, timeNum, blank_);
    return tempPrefix.pathProb;

}


float CSeqLEngWordReg :: onePrefixPathLogProbUTF8(const std::string & str, float * orgActs, int actLen){
    std::vector<int> path = stringutf8topath(str);
    int timeNum = actLen/labelsDim_;
    PrefixPath tempPrefix = onePrefixPathInfor(path, orgActs, \
         labelsDim_, timeNum, blank_);
    return tempPrefix.pathProb;

}

void CSeqLEngWordReg::pathPostionEstimation(std::vector<int> maxPath, std::vector<int> path,\
        std::vector<int> &startVec, std::vector<int> & endVec,\
        std::vector<float> & centVec, int blank) {
    std::vector<int> str;
    str.clear();
    startVec.clear();
    endVec.clear();
    centVec.clear();
    int startIndex = 0;
    int endIndex = 0;
    int prevLabel = -1;
    int index = 0;
    bool setFlag = false;
    for (std::vector<int>::const_iterator label = maxPath.begin(); label != maxPath.end(); label++) {
      if (*label != blank && (str.empty() || *label != str.back() || prevLabel == blank)) {
          str.push_back(*label);
          startVec.push_back(index);
          if(setFlag){
            endVec.push_back(index-1);
          }
          setFlag = true;
      }

      if( *label==blank && setFlag ){
         setFlag = false;
         endVec.push_back(index-1);
      }
      prevLabel = *label;
      index++;
    }
    if(setFlag==true){
      endVec.push_back(index-1);
    }
    assert(str.size() == startVec.size());
    assert(str.size() == endVec.size());
    centVec.resize(startVec.size());
    
    for(int i=0; i<centVec.size(); i++){
       centVec[i] = (startVec[i]+endVec[i])/2.0;
    }
}

int CSeqLEngWordReg::extractoutonewordinfor(float * matrix, int* output_labels, 
                int len, SSeqLEngWordInfor & outWord){

    int label_read_pos = 0;
    if (len <= 0) {
        return -1;
    }

    for (int i = 0; i < len; i++) {
        int label_len = output_labels[label_read_pos];
        //printf("label len: %d\n", label_len);
        if (label_len <= 0) {
            return -1;
        }
        std::vector<int> max_vec;   
        label_read_pos++;
        for (int j = 0; j < label_len; j++, label_read_pos++) {
            int label_id = output_labels[label_read_pos];
            assert(label_id > 0);
            if (label_id >= 2) {
                //printf("%d\t", label_id);
                max_vec.push_back(label_id - 2);   // skip eos sos
            }
            if (label_id == 1) {
                assert(j == lable_len - 1);  // ensure the eos
            }
        }
        if (i == 0) {   // the highest prob result
            outWord.maxVec_ = max_vec;
            outWord.maxStr_ = pathToString(max_vec);
            outWord.regLogProb_ = (double)matrix[i]; 
            outWord.wordStr_ = pathToString(max_vec);
            //printf("%f\t", outWord.regLogProb_);
            //printf("%s\n", outWord.maxStr_.c_str());
        }
        else {
            SSeqLEngWordInfor cand_word;
            cand_word.maxVec_ = max_vec;
            cand_word.maxStr_ = pathToString(max_vec);
            cand_word.regLogProb_ = (double)matrix[i];
            cand_word.wordStr_ = pathToString(max_vec);
            //printf("%f\t", cand_word.regLogProb_);
            //printf("%s\n", cand_word.maxStr_.c_str());
            outWord.cand_word_vec.push_back(cand_word);
        }
        assert(output_labels[label_read_pos] == -1);
        label_read_pos++;
    }
    std::vector<int> start_vec;
    std::vector<int> end_vec;
    std::vector<float> cent_vec;

    outWord.toBeDelete_ = false;

    outWord.centPosVec_ = cent_vec;
    outWord.startPosVec_ = start_vec;
    outWord.endPosVec_ = end_vec;
    return 0;
}

int CSeqLEngWordReg::extractoutonewordinfor(float * matrix, int* output_labels, 
                int labelsDim, int len,  SSeqLEngWordInfor & outWord){

    int label_read_pos = 0;
    int id_read_pos = 0;
    int old_read_pos = 0;
    int sum_timenum = len / labelsDim;

    if (len <= 0) {
        return -1;
    }

    int i = 0;

    while (id_read_pos < sum_timenum) {
        int label_len = output_labels[label_read_pos];
        //printf("label len: %d\n", label_len);
        if (label_len <= 0) {
            return -1;
        }

        std::vector<int> max_vec;   
        label_read_pos++;
        float logprob = 0.0;

        for (int j = 0; j < label_len; j++, label_read_pos++, id_read_pos++) {
            int label_id = output_labels[label_read_pos];
            assert(label_id > 0);
            if (label_id >= 2) {
                max_vec.push_back(label_id - 2);   // skip eos sos
                logprob += safeLog(matrix[id_read_pos * labelsDim + label_id]);
            }
            if (label_id == 1) {
                assert(j == lable_len - 1);  // ensure the eos
                //std::cout << "final prob: " << logprob + safeLog(matrix[id_read_pos * labelsDim + label_id]) << std::endl;
            }
            //std::cout << j << " " << label_id << " " << matrix[id_read_pos * labelsDim + label_id] << std::endl;
        }
        if (i == 0) {   // the highest prob result
            outWord.maxVec_ = max_vec;
            outWord.maxStr_ = pathToString(max_vec);
            outWord.regLogProb_ = (double)logprob; 
            outWord.wordStr_ = pathToString(max_vec);
            outWord.proMat_.resize(label_len*labelsDim);
            memcpy(&(outWord.proMat_[0]), matrix+old_read_pos*labelsDim, \
                            sizeof(float)*label_len*labelsDim);
            /*
            printf("%f\t", outWord.regLogProb_);
            printf("%s\n", outWord.maxStr_.c_str());
            for (int k = 0; k < maxVec.size(); k++) {
                std::cout << k << " " << maxVec[k] + 2 << " " << outWord.proMat_[maxVec[k] + 2 + k * labelsDim] << std::endl;
            }
            std::cout << maxVec.size() << " " << 1 << " " << outWord.proMat_[1 + maxVec.size() * labelsDim] << std::endl;*/
            i = 1;
        }
        else {
            SSeqLEngWordInfor cand_word;
            cand_word.maxVec_ = max_vec;
            cand_word.maxStr_ = pathToString(max_vec);
            cand_word.regLogProb_ = (double)logprob;
            cand_word.wordStr_ = pathToString(max_vec);
            cand_word.proMat_.resize(label_len*labelsDim);
            memcpy(&(cand_word.proMat_[0]), matrix+old_read_pos*labelsDim, \
                            sizeof(float)*label_len*labelsDim);
            /*
            printf("%f\t", cand_word.regLogProb_);
            printf("%s\n", cand_word.maxStr_.c_str());
            
            for (int k = 0; k < maxVec.size(); k++) {
                std::cout << k << " " << maxVec[k] + 2 << " " << cand_word.proMat_[maxVec[k] + 2 + k * labelsDim] << std::endl;
            }
            std::cout << maxVec.size() << " " << 1 << " " << cand_word.proMat_[1 + maxVec.size() * labelsDim] << std::endl;*/
            outWord.cand_word_vec.push_back(cand_word);
        }
        old_read_pos = id_read_pos;
        assert(output_labels[label_read_pos] == -1);
        label_read_pos++;
    }

    std::vector<int> startvec;
    std::vector<int> endvec;
    std::vector<float> centvec;

    outWord.toBeDelete_ = false;

    outWord.centPosVec_ = centvec;
    outWord.startPosVec_ = startvec;
    outWord.endPosVec_ = endvec;
}

int CSeqLEngWordReg::extractoutonewordinfor(float * matrix, int* output_labels, 
                int labelsDim, int len, float * weights_matrix, int weights_size,  
                SSeqLEngWordInfor & outWord){

    int label_read_pos = 0;
    int id_read_pos = 0;
    int old_read_pos = 0;
    int sum_timenum = len / labelsDim;
    //std::cout << "len: " << len << " " << labelsDim << " " << sum_timenum << " " << weights_size << std::endl;
    int encoder_fea_num = weights_size / sum_timenum;

    if (len <= 0) {
        return -1;
    }

    int i = 0;

    while (id_read_pos < sum_timenum) {
        int label_len = output_labels[label_read_pos];
        //printf("label len: %d\n", label_len);
        if (label_len <= 0) {
            return -1;
        }

        std::vector<int> max_vec;   
        label_read_pos++;
        float logprob = 0.0;

        for (int j = 0; j < label_len; j++, label_read_pos++, id_read_pos++) {
            int label_id = output_labels[label_read_pos];
            //assert(label_id > 0);
            if (j == 99 && label_id != 1) { // for the string without eos
                continue;
            }
            if (label_id >= 2) {
                max_vec.push_back(label_id - 2);   // skip eos sos
                logprob += safeLog(matrix[id_read_pos * labelsDim + label_id]);
            }
            /*if (label_id == 1) {
                assert(j == lable_len - 1);  // ensure the eos
            }*/
        }
        if (i == 0) {   // the highest prob result
            outWord.maxVec_ = max_vec;
            outWord.maxStr_ = pathToString(max_vec);
            outWord.regLogProb_ = (double)logprob; 
            outWord.wordStr_ = pathToString(max_vec);
            for (int prob_idx = 0; prob_idx <= label_len*labelsDim; prob_idx++){
                outWord.proMat_.push_back(matrix[old_read_pos*labelsDim + prob_idx]);
            }
            outWord.encoder_fea_num_ = encoder_fea_num;
            for (int att_idx = 0; att_idx < label_len * encoder_fea_num; att_idx++) {
                outWord.att_weights_.push_back(weights_matrix[old_read_pos * encoder_fea_num 
                                + att_idx]);
            }
            /*outWord.proMat_.resize(label_len*labelsDim);
            memcpy(&(outWord.proMat_[0]), matrix+old_read_pos*labelsDim, \
                            sizeof(float)*label_len*labelsDim);
            outWord.encoder_fea_num_ = encoder_fea_num;
            outWord.att_weights_.resize(label_len * encoder_fea_num);
            memcpy(&(outWord.att_weights_[0]), weights_matrix + old_read_pos * encoder_fea_num, \
                            sizeof(float) * label_len * encoder_fea_num);*/
            
            /*printf("%f\t", outWord.regLogProb_);
            printf("%s\n", outWord.maxStr_.c_str());
            for (int k = 0; k < max_vec.size(); k++) {
                std::cout << k << " " << max_vec[k] + 2 << " " << outWord.proMat_[max_vec[k] + 2 + k * labelsDim] << std::endl;
                float sum = 0.0;
                for (int kk = 0; kk < encoder_fea_num; kk++) {
                    std::cout << outWord.att_weights_[kk + k * encoder_fea_num] << " ";
                    sum += outWord.att_weights_[kk + k * encoder_fea_num];
                }
                std::cout << sum << std::endl;
            }
            
            std::cout << max_vec.size() << " " << 1 << " " << outWord.proMat_[1 + max_vec.size() * labelsDim] << std::endl;
            float sum = 0.0;
            for (int kk = 0; kk < encoder_fea_num; kk++) {
                std::cout << outWord.att_weights_[kk + (max_vec.size() - 1) * encoder_fea_num] << " ";
                sum += outWord.att_weights_[kk + (max_vec.size() - 1) * encoder_fea_num];
            }
            std::cout << sum << std::endl;*/
            i = 1;
        }
        else {
            SSeqLEngWordInfor cand_word;
            cand_word.maxVec_ = max_vec;
            cand_word.maxStr_ = pathToString(max_vec);
            cand_word.regLogProb_ = (double)logprob;
            cand_word.wordStr_ = pathToString(max_vec);

            for (int prob_idx = 0; prob_idx <= label_len*labelsDim; prob_idx++){
                cand_word.proMat_.push_back(matrix[old_read_pos*labelsDim + prob_idx]);
            }
            cand_word.encoder_fea_num_ = encoder_fea_num;
            for (int att_idx = 0; att_idx < label_len * encoder_fea_num; att_idx++) {
                cand_word.att_weights_.push_back(weights_matrix[old_read_pos * encoder_fea_num 
                                + att_idx]);
            }
            /*cand_word.proMat_.resize(label_len*labelsDim);
            cand_word.encoder_fea_num_ = encoder_fea_num;
            memcpy(&(cand_word.proMat_[0]), matrix+old_read_pos*labelsDim, \
                            sizeof(float)*label_len*labelsDim);
            cand_word.att_weights_.resize(label_len * encoder_fea_num);
            memcpy(&(cand_word.att_weights_[0]), weights_matrix + old_read_pos * encoder_fea_num, \
                            sizeof(float) * label_len * encoder_fea_num);*/
            /*
            printf("%f\t", cand_word.regLogProb_);
            printf("%s\n", cand_word.maxStr_.c_str());
            
            for (int k = 0; k < max_vec.size(); k++) {
                std::cout << k << " " << max_vec[k] + 2 << " " << cand_word.proMat_[max_vec[k] + 2 + k * labelsDim] << std::endl;
                float sum = 0.0;
                for (int kk = 0; kk < encoder_fea_num; kk++) {
                    std::cout << cand_word.att_weights_[kk + k * encoder_fea_num] << " ";
                    sum += cand_word.att_weights_[kk + k * encoder_fea_num];
                }
                std::cout << sum << std::endl;
            }
            std::cout << max_vec.size() << " " << 1 << " " << cand_word.proMat_[1 + max_vec.size() * labelsDim] << std::endl;
            float sum = 0.0;
            for (int kk = 0; kk < encoder_fea_num; kk++) {
                std::cout << cand_word.att_weights_[kk + (max_vec.size() - 1) * encoder_fea_num] << " ";
                sum += cand_word.att_weights_[kk + (max_vec.size() - 1) * encoder_fea_num];
            }
            std::cout << sum << std::endl;*/

            outWord.cand_word_vec.push_back(cand_word);
        }
        old_read_pos = id_read_pos;
        assert(output_labels[label_read_pos] == -1);
        label_read_pos++;
    }

    std::vector<int> startvec;
    std::vector<int> endvec;
    std::vector<float> centvec;

    outWord.toBeDelete_ = false;

    outWord.centPosVec_ = centvec;
    outWord.startPosVec_ = startvec;
    outWord.endPosVec_ = endvec;
}

int CSeqLEngWordReg::extractOutOneWordInfor(float * matrix, int labelsDim, int len,  SSeqLEngWordInfor & outWord){
   int timeNum = len/labelsDim_;
   outWord.proMat_.resize(len);
   memcpy(&(outWord.proMat_[0]) , matrix, sizeof(float)*len);

   std::vector<int> maxVec;
   maxVec.resize(timeNum);
   for(unsigned int i=0; i<timeNum; i++){
      float *acts = matrix + i*labelsDim_;
      int actIndex = argMaxActs(acts, labelsDim_);
      maxVec[i] = actIndex;
   }
   
   outWord.maxVec_ = maxVec;
   outWord.maxStr_ = pathToStringHaveBlank(maxVec);
    /*for(int i = 0; i<maxVec.size(); i++)
    {
        std::cout<<maxVec[i]<<" ";
    }
    std::cout<<std::endl;*/
   std::vector<int> path = pathElimination(maxVec, blank_);
   // PrefixPath outPrefix = onePrefixPathInfor( path , matrix, \
             labelsDim_, timeNum, labelsDim_-1 );
   // outWord.regLogProb_ = outPrefix.pathProb;
   outWord.regLogProb_ = 0.0;

   outWord.wordStr_ = pathToString(path);
   std::vector<int> startVec;
   std::vector<int> endVec;
   std::vector<float> centVec;

    outWord.toBeDelete_ = false;

   outWord.centPosVec_ = centVec;
   outWord.startPosVec_ = startVec;
   outWord.endPosVec_ = endVec;
}

//
int CSeqLEngWordReg::extract_word_infor_asia(float * matrix, int lablesDim,\
        int len,  SSeqLEngWordInfor & outWord)  {
    
    int time_num = len / labelsDim_;
    outWord.proMat_.resize(len);
    memcpy(&(outWord.proMat_[0]), matrix, sizeof(float) * len);

    std::vector<int> max_vec;
    max_vec.resize(time_num);

    for (unsigned int i = 0; i < time_num; i++) {
        float *acts = matrix + i * labelsDim_;
        max_vec[i] = argMaxActs(acts, labelsDim_);
    }
   
    outWord.maxVec_ = max_vec; 
    outWord.maxStr_ = pathToStringHaveBlank(max_vec);
   
    std::vector<int> path = pathElimination(max_vec, blank_);
    outWord.pathVec_ = path;
    outWord.regLogProb_ = 1.0;
    outWord.wordStr_ = pathToString(path);

    int show_recog_rst_flag = g_ini_file->get_int_value("SHOW_RECOG_RST",
            IniFile::_s_ini_section_global);
    if (show_recog_rst_flag == 1) {
        // xie shufu add   
        std::cout << "maxStr_:" << outWord.maxStr_
            << " wordStr_: " << outWord.wordStr_ \
            << " regLogProb: " << outWord.regLogProb_ 
            << " timeNum: " << time_num << std::endl;
    }

    std::vector<int> startVec;
    std::vector<int> endVec;
    std::vector<float> centVec;
    outWord.centPosVec_ = centVec;
    outWord.startPosVec_ = startVec;
    outWord.endPosVec_ = endVec;

    return 0;
}

 void CSeqLEngWordReg::extend_att_word(SSeqLEngLineInfor & outLineInfor){
    
    int word_num = outLineInfor.wordVec_.size();
    for (int i = 0; i < word_num; i++) {
        SSeqLEngWordInfor word = outLineInfor.wordVec_[i];
        int timeNum = word.maxVec_.size();
        for (unsigned int j = 0; j < timeNum; j++) {
            int index = word.maxVec_[j] + 2;
            int second_id = -1;
            float second_prob = -1.0;
            // find the cand char
            float first_prob = word.proMat_[j * labelsDim_ + index];
            if ((word.maxStr_[j] != '\'' && word.proMat_[j * labelsDim_ + index] < 0.8) ||
                ((j == timeNum - 1 || j == 0) && word.proMat_[j * labelsDim_ + index] < 0.8) ||
                (word.proMat_[j * labelsDim_ + index] < 0.4)) {
                for (int k = 0; k < labelsDim_; k++) {
                    if (word.proMat_[j * labelsDim_ + k] < first_prob - 0.001 && 
                        word.proMat_[j * labelsDim_ + k] > second_prob &&
                        word.proMat_[j * labelsDim_ + k] > 0.1) {
                            second_prob = word.proMat_[j * labelsDim_ + k];
                            second_id = k;
                    }
                }
            }
            if ((second_id > 1 && second_id < labelsDim_ -1)
                || (second_id == 1 && j == timeNum - 1)) {

                SSeqLEngWordInfor recog_res_cand;
                recog_res_cand.left_ = word.left_;
                recog_res_cand.top_  = word.top_;
                recog_res_cand.wid_  = word.wid_;
                recog_res_cand.hei_  = word.hei_;
                recog_res_cand.dectScore_  = word.dectScore_;
                recog_res_cand.mulRegScore_ = 0;
                recog_res_cand.compNum_ = word.compNum_;
                recog_res_cand.leftSpace_ = word.leftSpace_;
                recog_res_cand.rightSpace_ = word.rightSpace_;
                recog_res_cand.sigWordFlag_ = word.sigWordFlag_;
                recog_res_cand.isPunc_ = false;
                recog_res_cand.toBeDelete_ = false;
                recog_res_cand.toBeAdjustCandi_ = true;
                recog_res_cand.proMat_ = word.proMat_;
                recog_res_cand.highLowType_ = word.highLowType_;
                recog_res_cand.path_pos_.clear();

                recog_res_cand.encoder_fea_num_ = word.encoder_fea_num_;
                recog_res_cand.encoder_fea_length_ = word.encoder_fea_length_;
                recog_res_cand.att_pos_st_id_set_ = word.att_pos_st_id_set_;
                recog_res_cand.att_pos_en_id_set_ = word.att_pos_en_id_set_;
                
                if (second_id > 1) {
                    for (int k = 0; k < timeNum; k++) {
                        recog_res_cand.maxVec_.push_back(word.maxVec_[k]);
                        recog_res_cand.path_pos_.push_back(k);
                    }
                    recog_res_cand.maxVec_[j] = second_id - 2;
                }
                else {
                    for (int k = 0; k < timeNum - 1; k++) {
                        recog_res_cand.maxVec_.push_back(word.maxVec_[k]);
                        recog_res_cand.path_pos_.push_back(k);
                    }
                    recog_res_cand.word_flag_log_ = safeLog(second_prob);
                }
                recog_res_cand.maxStr_ = pathToString(recog_res_cand.maxVec_);
                recog_res_cand.wordStr_ = pathToString(recog_res_cand.maxVec_);
                recog_res_cand.regLogProb_ = word.regLogProb_;
                recog_res_cand.regLogProb_ -= safeLog(first_prob);
                recog_res_cand.regLogProb_ += safeLog(second_prob);
                /*std::cout << "extand word: " << recog_res_cand.wordStr_ << " "  
                        << second_id << " " << second_prob << " " 
                        << j << " " << first_prob << std::endl;*/
                outLineInfor.wordVec_.push_back(recog_res_cand);
            }
        }        
    }

 }

 void CSeqLEngWordReg::extendVariationWord(SSeqLEngLineInfor & outLineInfor, int index){
    
     int startIndex = outLineInfor.wordVec_.size();
     int timeNum = outLineInfor.wordVec_[index].proMat_.size()/labelsDim_;
     
     std::vector<int> maxVec;
     maxVec.resize(timeNum);
     for(unsigned int i=0; i<timeNum; i++){
        float *acts = &(outLineInfor.wordVec_[index].proMat_[0]) + i*labelsDim_;
        int actIndex = argMaxActs(acts, labelsDim_);
        maxVec[i] = actIndex;
     }
     
     int variationNum = 0;
     //float variationTh = 0.65;
     float variationTh = 0.7;
     for(int i=0; i<timeNum; i++){
        if(outLineInfor.wordVec_[index].proMat_[i*labelsDim_+maxVec[i]] <=variationTh ){
            variationNum++;
        }
     }
     
     for(int i=0; i<timeNum; i++){
        if(outLineInfor.wordVec_[index].proMat_[i*labelsDim_+maxVec[i]] >variationTh ){
            continue;
        }
        SSeqLEngWordInfor newWord;
        newWord = outLineInfor.wordVec_[index];
        std::vector<float> tempProMat = newWord.proMat_;
        tempProMat[i*labelsDim_+maxVec[i]] = EXP_MIN;
        extractOutOneWordInfor(&(tempProMat[0]), labelsDim_, newWord.proMat_.size(), newWord);
        newWord.toBeAdjustCandi_ = true;
        
        if(newWord.wordStr_.length()<1 || newWord.wordStr_ == outLineInfor.wordVec_[index].wordStr_){
            //std::cout << "Same newword " << newWord.wordStr_ << std::endl;
            continue;
        }
        if (newWord.wordStr_[newWord.wordStr_.size() - 1] == ' ') {
            newWord.wordStr_.erase(newWord.wordStr_.end() - 1);
        }
        if (newWord.wordStr_[0] == ' ') {
            newWord.wordStr_.erase(newWord.wordStr_.begin());
        }
        bool haveSameWord = false;

        for(int j=startIndex; j<outLineInfor.wordVec_.size(); j++){
            if(newWord.wordStr_ == outLineInfor.wordVec_[j].wordStr_){
         //       std::cout << "same "<<  outLineInfor.wordVec_[j].wordStr_ <<  std::endl;
                haveSameWord = true;
                break;
            }
        }
        if(haveSameWord){
            continue;
        }
        outLineInfor.wordVec_.push_back(newWord);
     }
 
 }

bool SeqLLineDecodeIsPunct(char c){
    if( c>='a' && c<='z'){
        return false;
    }
    if( c>='A' && c<='Z'){
        return false;
    }
    if( c>='0' && c<='9'){
        return false;
    }
    return true;
}

int CSeqLEngWordReg :: copyInforFormProjectWordTotal(SSeqLEngWordInfor & perWord, SSeqLEngRect & newRect,\
        SSeqLEngWordInfor & newWord, IplImage * image, int setStartTime, int setEndTime)    
{
        int extBoudary = extendRate_*perWord.hei_;
        int perImageLeft = perWord.left_;        
        int perImageWid  = perWord.wid_;
        //int newExtBoundary = extendRate_*perWord.hei_;
        int newExtBoundary = 0.05*perWord.hei_;
        //int newExtBoundary = 0*perWord.hei_;
        int thisImageLeft = std::max(perImageLeft, newRect.left_-newExtBoundary);
        int thisImageRight = std::min(newRect.left_+newRect.wid_+newExtBoundary-1, perImageLeft+perImageWid-1);
       
        std::vector<float> & perMat = perWord.proMat_;

        int perTimeNum = perMat.size()/labelsDim_;
        int newShiftX = thisImageLeft - perImageLeft;
        int newStartTime = perTimeNum * newShiftX / perImageWid;
        
        int newEndTime = perTimeNum - 1 - (std::max(perImageLeft+perImageWid-1 - thisImageRight,0))*perTimeNum/perImageWid;
    
        if(setStartTime!=-1 && setEndTime!=-1){
            newStartTime = setStartTime;
            newEndTime = setEndTime;

        }

        int newTimeNum = newEndTime - newStartTime + 1 ;

        if(newStartTime >= perTimeNum || newTimeNum< 1 ){
            std::cerr<<" newStartTime >= perTimeNum" << std::endl;
            newStartTime = perTimeNum-1;
            newTimeNum =1;
        }
        extractOutOneWordInfor(&(perMat[newStartTime*labelsDim_]), labelsDim_, labelsDim_*newTimeNum, newWord);
        
        newWord.left_ = newRect.left_;
        newWord.top_  = newRect.top_;
        newWord.wid_  = newRect.wid_;
        newWord.hei_  = newRect.hei_;
        newWord.dectScore_  = newRect.score_;
        newWord.mulRegScore_ = 0;
        newWord.compNum_ = newRect.compNum_;
        newWord.leftSpace_ = newRect.leftSpace_;
        newWord.rightSpace_ = newRect.rightSpace_;
        newWord.sigWordFlag_ = newRect.sigWordFlag_;
        newWord.isPunc_ = false;
        newWord.toBeAdjustCandi_ = true;

    return 0;
        
}

inline bool is_num(char c) {
    if (c <= '9' && c >= '0') {
        return true;
    } else {
        return false;
    }
}

inline bool is_punct(char c){
    if( c>='a' && c<='z'){
        return false;
    }
    if( c>='A' && c<='Z'){
        return false;
    }
    if( c>='0' && c<='9'){
        return false;
    }
    return true;
}
inline int num_of(std::vector<char> vec, char c) {
    int num = 0;
    std::vector<char>::iterator it;
    for (it = vec.begin(); it != vec.end(); ++it) {
        if (*it == c) {
            ++num;
        }
    }
    return num;
}

int CSeqLEngWordReg::gen_highlow_type(SSeqLEngWordInfor word) {
    std::string str = word.wordStr_;
    for (int i = 0; i < str.size(); i++) {
        if (!(str[i] >= 'a' && str[i] <= 'z') &&
               !(str[i] >= 'A' && str[i] <= 'Z')) {
            return -1;
        }
    }

    transform(str.begin(), str.end(), str.begin(), ::tolower);
    std::vector<int> lower_path = stringutf8topath(str);
    float lower_prob = 0.0;
    for (int i = 0; i < lower_path.size(); i++) {
        int index = i * labelsDim_ + (lower_path[i] + 2);
        lower_prob += safeLog(word.proMat_[index]);
    }

    transform(str.begin(), str.begin()+1, str.begin(), ::toupper);
    std::vector<int> headhigh_path = stringutf8topath(str);
    float headhigh_prob = 0.0;
    for (int i = 0; i < headhigh_path.size(); i++) {
        int index = i * labelsDim_ + (headhigh_path[i] + 2);
        headhigh_prob += safeLog(word.proMat_[index]);
    }

    transform(str.begin(), str.end(), str.begin(), ::toupper);
    std::vector<int> higher_path = stringutf8topath(str);
    float higher_prob = 0.0;
    for (int i = 0; i < higher_path.size(); i++) {
        int index = i * labelsDim_ + (higher_path[i] + 2);
        higher_prob += safeLog(word.proMat_[index]);
    }

    if (higher_prob > lower_prob && higher_prob > headhigh_prob) {
        return 2;
    }
    
    if (headhigh_prob > higher_prob && headhigh_prob > lower_prob) {
        return 1;
    }

    if (lower_prob > higher_prob && lower_prob > headhigh_prob) {
        return 0;
    }
}

//given a word, and the start, end of its subword to get the precise location of the word.
SSeqLEngWordInfor CSeqLEngWordReg::split_word(SSeqLEngWordInfor word,
        int start, int end, int next_start, bool need_adjust) {

    //std::vector<SSeqLEngWordInfor> new_word_vec;
    SSeqLEngWordInfor new_word;
    new_word.top_ = word.top_;
    new_word.hei_ = word.hei_;
    int time_num = word.maxVec_.size();
    //std::cout<<"time_num:"<<time_num<<std::endl;
    int end_with_blank = std::max(end, next_start - 1);
    //std::cout<<"end_with_blank:"<<end_with_blank<<std::endl;
    //int len = end_with_blank - start + 1;
    int real_end_time = next_start > end + 4 ? end + 4 : next_start;
    //std::cout<<"real_end_time:"<<real_end_time<<std::endl;
    int len =real_end_time - start;
    std::vector<int> this_max_vec;
    this_max_vec.resize(len);
    memcpy(&(this_max_vec[0]), &(word.maxVec_[start]), sizeof(int) * len);
    //std::cout << "split word: " << word.maxStr_ << std::endl;
 
    //need to be adjust
    new_word.left_ = word.left_ + word.wid_ * start / time_num;
    new_word.wid_  = word.left_ + word.wid_ * real_end_time / time_num - new_word.left_;
    new_word.mulRegScore_ = 0;
    new_word.toBeAdjustCandi_ = need_adjust;
    new_word.maxVec_ = this_max_vec;
    new_word.toBeDelete_ = false;
    //new_word.maxStr_ = word.maxStr_.substr(start, end - start + 1);
    new_word.maxStr_ = word.maxStr_.substr(start, real_end_time - start);
    new_word.highLowType_ = 0;
    new_word.word_flag_log_ = word.word_flag_log_;
    /*std::cout << "word_flag_log_: " << word.word_flag_log_ << std::endl;
    std::cout << "left: " << word.left_ << " " << new_word.left_ << std::endl;
    std::cout << "wid: " << word.wid_ << " " << new_word.wid_ << std::endl;
    std::cout << start << " " << time_num << " " << real_end_time << " " << end << " " << next_start << std::endl;*/

    //std::cout << "split word: " << new_word.maxStr_ << std::endl;
    if (_m_attention_decode) { // no path elimination

        /*push attention pos st en id
        std::vector<int> this_att_pos_st_id_set;
        std::vector<int> this_att_pos_en_id_set;
        this_att_pos_st_id_set.resize(len);
        this_att_pos_en_id_set.resize(len);
        memcpy(&(this_att_pos_st_id_set[0]), &(word.att_pos_st_id_set_[start]), sizeof(int) * len);
        memcpy(&(this_att_pos_en_id_set[0]), &(word.att_pos_en_id_set_[start]), sizeof(int) * len);*/

        for (int idx = 0; idx < len; idx++) {
            new_word.att_pos_st_id_set_.push_back(word.att_pos_st_id_set_[start + idx]);
            new_word.att_pos_en_id_set_.push_back(word.att_pos_en_id_set_[start + idx]);
        }

        new_word.encoder_fea_num_ = word.encoder_fea_num_;
        new_word.encoder_fea_length_ = word.encoder_fea_length_;
        //new_word.att_pos_st_id_set_ = this_att_pos_st_id_set;
        //new_word.att_pos_en_id_set_ = this_att_pos_en_id_set;

        //get the word left right id
        int att_st_id = word.encoder_fea_num_;
        int att_en_id = -1;
        for (int i = 0; i < len; i++) {
            if (new_word.att_pos_st_id_set_[i] < att_st_id && 
                    new_word.att_pos_st_id_set_[i] >= 0) {
                att_st_id = new_word.att_pos_st_id_set_[i];
            }
            if (new_word.att_pos_en_id_set_[i] > att_en_id && 
                    new_word.att_pos_en_id_set_[i] < word.encoder_fea_num_) {
                att_en_id = new_word.att_pos_en_id_set_[i];
            }
        }
        if (att_st_id <= att_en_id) {
            new_word.left_ = (int)(att_st_id * new_word.encoder_fea_length_);
            new_word.wid_ = (int)((att_en_id + 1)* new_word.encoder_fea_length_);
            new_word.left_ = std::max(word.left_, new_word.left_);
            new_word.wid_ = std::min(new_word.wid_, word.left_ + word.wid_);
            new_word.wid_ = new_word.wid_ - new_word.left_;
            //std::cout << "st en id: "<< att_st_id << " " << att_en_id << " " << new_word.left_ << " " << new_word.left_ + new_word.wid_ <<std::endl;
        }
        std::vector<int> path_pos;
        std::vector<int> path = this_max_vec;
        std::vector<float> this_prob_mat;

        this_prob_mat.resize((len + 1) * labelsDim_);
        memcpy(&(this_prob_mat[0]), &(word.proMat_[start * labelsDim_]),
            sizeof(float) * (len + 1) * labelsDim_);
        new_word.proMat_ = this_prob_mat;
        new_word.regLogProb_ = 0.0;
        for (int i = 0; i < path.size(); i++) {
            path_pos.push_back(i);
            new_word.regLogProb_ += safeLog(new_word.proMat_[i * labelsDim_ + (path[i] + 2)]);
        }
        new_word.wordStr_ = pathToString(path);
        new_word.path_pos_.clear();
        new_word.path_pos_ = path_pos;
        // add the split punc prob
        if (start > 0) {
            new_word.word_flag_log_ += safeLog(word.proMat_[(start - 1) * labelsDim_ + (word.maxVec_[start - 1] + 2)]);
        }
        if (end_with_blank < time_num - 1) {
            new_word.word_flag_log_ += safeLog(word.proMat_[(end_with_blank+1) * labelsDim_ + (word.maxVec_[end_with_blank+1] + 2)]);
        }
        new_word.regLogProb_ += new_word.word_flag_log_;
        //std::cout<<"new_word_beforehighlow:"<<new_word.wordStr_<<std::endl;
        new_word.highLowType_ = gen_highlow_type(new_word);

    } else {
        std::vector<float> this_prob_mat;
        this_prob_mat.resize(len * labelsDim_);
        memcpy(&(this_prob_mat[0]), &(word.proMat_[start * labelsDim_]),
            sizeof(float) * len * labelsDim_);
        new_word.proMat_ = this_prob_mat;

        std::vector<int> path_pos;
        std::vector<int> path = path_elimination(this_max_vec, path_pos);
        new_word.wordStr_ = pathToString(path);
        new_word.path_pos_ = path_pos;

        if (need_adjust) {
            PrefixPath this_prefix = onePrefixPathInfor(path , &(this_prob_mat[0]), labelsDim_, len, blank_);
            new_word.regLogProb_ = this_prefix.pathProb;
            onePrefixPathUpLowerLogProb(new_word.wordStr_, &(this_prob_mat[0]),
                        len * labelsDim_, new_word.highLowType_);
        } else {
            new_word.regLogProb_ = 0.0;
        }
    }

    // word splited by punc will add the acc score
    if (new_word.wordStr_.size() > 4) {
        new_word.acc_score_ = word.acc_score_;
        new_word.main_line_ = word.main_line_;
    }
    else {
        new_word.acc_score_ = 0.0;
        new_word.main_line_ = false;
    }
    //new_word_vec.push_back(new_word);

    //return new_word_vec;
    return new_word; 
}

//by the respond_position to refine the output word position 
SSeqLEngWordInfor CSeqLEngWordReg::split_word(SSeqLEngWordInfor word,
        int start, int end, int next_start, bool need_adjust, 
        std::vector<int> respond_position, SSeqLEngWordInfor original_word) {

    //std::vector<SSeqLEngWordInfor> new_word_vec;
    SSeqLEngWordInfor new_word;
    new_word.top_ = word.top_;
    new_word.hei_ = word.hei_;
    int time_num = word.maxVec_.size();
    int end_with_blank = std::max(end, next_start - 1);
    int len = end_with_blank - start + 1;
    /*std::vector<int> this_max_vec;
    this_max_vec.resize(len);
    memcpy(&(this_max_vec[0]), &(word.maxVec_[start]), sizeof(int) * len);*/
    for (int idx = 0; idx < len; idx++) {
        new_word.maxVec_.push_back(word.maxVec_[start + idx]);
    }
    
    //need to be adjust
    new_word.left_ = word.left_ + word.wid_ * start / time_num;
    int real_end_time = next_start > end + 4 ? end + 4 : next_start;
    new_word.wid_  = word.left_ + word.wid_ * real_end_time / time_num - new_word.left_;
    new_word.mulRegScore_ = 0;
    new_word.toBeAdjustCandi_ = need_adjust;
    //new_word.maxVec_ = this_max_vec;
    new_word.toBeDelete_ = false;
    new_word.maxStr_ = word.maxStr_.substr(start, end - start + 1);
    new_word.highLowType_ = 0;
    new_word.word_flag_log_ = word.word_flag_log_;

    if (_m_attention_decode) { // no path elimination

        /*push attention pos st en id
        std::vector<int> this_att_pos_st_id_set;
        std::vector<int> this_att_pos_en_id_set;
        this_att_pos_st_id_set.resize(len);
        this_att_pos_en_id_set.resize(len);
        memcpy(&(this_att_pos_st_id_set[0]), &(word.att_pos_st_id_set_[start]), sizeof(int) * len);
        memcpy(&(this_att_pos_en_id_set[0]), &(word.att_pos_en_id_set_[start]), sizeof(int) * len);*/
        for (int idx = 0; idx < len; idx++) {
            new_word.att_pos_st_id_set_.push_back(word.att_pos_st_id_set_[start + idx]);
            new_word.att_pos_en_id_set_.push_back(word.att_pos_en_id_set_[start + idx]);
        }

        new_word.encoder_fea_num_ = word.encoder_fea_num_;
        new_word.encoder_fea_length_ = word.encoder_fea_length_;
        //new_word.att_pos_st_id_set_ = this_att_pos_st_id_set;
        //new_word.att_pos_en_id_set_ = this_att_pos_en_id_set;

        //get the word left right id
        int att_st_id = word.encoder_fea_num_;
        int att_en_id = -1;
        for (int i = 0; i < len; i++) {
            if (new_word.att_pos_st_id_set_[i] < att_st_id && 
                    new_word.att_pos_st_id_set_[i] >= 0) {
                att_st_id = new_word.att_pos_st_id_set_[i];
            }
            if (new_word.att_pos_en_id_set_[i] > att_en_id && 
                    new_word.att_pos_en_id_set_[i] < word.encoder_fea_num_) {
                att_en_id = new_word.att_pos_en_id_set_[i];
            }
        }
        if (att_st_id <= att_en_id && att_st_id >= 0 && att_en_id < word.encoder_fea_num_) {
            new_word.left_ = (int)(att_st_id * new_word.encoder_fea_length_);
            new_word.wid_ = (int)((att_en_id + 1)* new_word.encoder_fea_length_);
            new_word.left_ = std::max(word.left_, new_word.left_);
            new_word.wid_ = std::min(new_word.wid_, word.left_ + word.wid_);
            new_word.wid_ = new_word.wid_ - new_word.left_;
            //std::cout << "st en id: "<< att_st_id << " " << att_en_id << " " << new_word.left_ << " " << new_word.left_ + new_word.wid_ <<std::endl;
        }
        
        //find the word (before space split)'s position
        int start_position = respond_position[start]; 
        int end_position = respond_position[end]; 
        int orignal_att_st_id = original_word.encoder_fea_num_;
        int orignal_att_en_id = -1;
        for (int i = start_position; i <= end_position; i++) {
            if (original_word.att_pos_st_id_set_[i] < orignal_att_st_id && 
                    original_word.att_pos_st_id_set_[i] >= 0) {
                orignal_att_st_id = original_word.att_pos_st_id_set_[i];
            }
            if (original_word.att_pos_en_id_set_[i] > orignal_att_en_id &&
                    original_word.att_pos_en_id_set_[i] < original_word.encoder_fea_num_) {
                orignal_att_en_id = original_word.att_pos_en_id_set_[i];
            }
        }
        //refine the position
        if (orignal_att_st_id <= orignal_att_en_id) {
            new_word.wid_ = std::min(new_word.left_ + new_word.wid_, \
                (int)((orignal_att_en_id + 1) * original_word.encoder_fea_length_));
            new_word.left_ = std::max(new_word.left_, (int)(orignal_att_st_id * \
                original_word.encoder_fea_length_));
            new_word.wid_ = new_word.wid_ - new_word.left_;
        }
        /*std::cout << "refine position: " << orignal_att_st_id << " " << orignal_att_en_id << " " << (int)(orignal_att_st_id * original_word.encoder_fea_length_)
                << " " << (int)((orignal_att_en_id + 1) * original_word.encoder_fea_length_) << std::endl;*/

        std::vector<int> path_pos;
        std::vector<int> path = new_word.maxVec_;
        /*std::vector<float> this_prob_mat;

        this_prob_mat.resize((len + 1) * labelsDim_);
        memcpy(&(this_prob_mat[0]), &(word.proMat_[start * labelsDim_]),
            sizeof(int) * (len + 1) * labelsDim_);
        new_word.proMat_ = this_prob_mat;*/
        for (int idx = 0; idx < (len + 1) * labelsDim_; idx++) {
            new_word.proMat_.push_back(word.proMat_[start * labelsDim_ + idx]);
        }
        new_word.regLogProb_ = 0.0;
        for (int i = 0; i < path.size(); i++) {
            path_pos.push_back(i);
            new_word.regLogProb_ += safeLog(new_word.proMat_[i * labelsDim_ + (path[i] + 2)]);
        }
        new_word.wordStr_ = pathToString(path);
        new_word.path_pos_.clear();
        new_word.path_pos_ = path_pos;
        // add the split punc prob
        if (start > 0) {
            new_word.word_flag_log_ += safeLog(word.proMat_[(start - 1) * labelsDim_ + (word.maxVec_[start - 1] + 2)]);
        }
        if (end_with_blank < time_num - 1) {
            new_word.word_flag_log_ += safeLog(word.proMat_[(end_with_blank+1) * labelsDim_ + (word.maxVec_[end_with_blank+1] + 2)]);
        }
        new_word.regLogProb_ += new_word.word_flag_log_;
        new_word.highLowType_ = gen_highlow_type(new_word);

    } else {
        std::vector<float> this_prob_mat;
        this_prob_mat.resize(len * labelsDim_);
        memcpy(&(this_prob_mat[0]), &(word.proMat_[start * labelsDim_]),
            sizeof(float) * len * labelsDim_);
        new_word.proMat_ = this_prob_mat;

        std::vector<int> path_pos;
        std::vector<int> path = path_elimination(new_word.maxVec_, path_pos);
        new_word.wordStr_ = pathToString(path);
        new_word.path_pos_ = path_pos;

        if (need_adjust) {
            PrefixPath this_prefix = onePrefixPathInfor(path , &(this_prob_mat[0]), labelsDim_, len, blank_);
            new_word.regLogProb_ = this_prefix.pathProb;
            onePrefixPathUpLowerLogProb(new_word.wordStr_, &(this_prob_mat[0]),
                        len * labelsDim_, new_word.highLowType_);
        } else {
            new_word.regLogProb_ = 0.0;
        }
    }

    // word splited by punc will add the acc score
    if (new_word.wordStr_.size() > 4) {
        new_word.acc_score_ = word.acc_score_;
        new_word.main_line_ = word.main_line_;
    }
    else {
        new_word.acc_score_ = 0.0;
        new_word.main_line_ = false;
    }
    //new_word_vec.push_back(new_word);

    //return new_word_vec;
    return new_word; 
}

int CSeqLEngWordReg::split_word_by_punct(SSeqLEngLineInfor & line) {
    SSeqLEngLineInfor out_line = line;
    int line_size = line.wordVec_.size();
    out_line.wordVec_.clear();
    std::vector<int> punct_pos;
    std::vector<char> punct;
    std::cout<<"line_size:"<<line_size<<std::endl;
    for (int i = 0; i < line_size; ++i) {
        SSeqLEngWordInfor word = line.wordVec_[i];
        bool flag_sp= false;
        for(int i =0; i<word.maxVec_.size();i++)
            if(word.maxVec_[i] == 96 || word.maxVec_[i] == 95) flag_sp = true;
        if(flag_sp)
        {
            out_line.wordVec_.push_back(word);
            continue;
        }
        //std::cout << "split punc for: " << word.wordStr_ << std::endl;
        punct_pos.clear();
        punct.clear();
        //std::vector<SSeqLEngWordInfor> new_word_vec;
        bool all_num = true;
        for (int j = 0; j < word.wordStr_.size(); ++j) {
            char c = word.wordStr_[j];
            if (is_punct(c)) {
                punct_pos.push_back(j);
                punct.push_back(c);
            } else { 
                all_num = all_num && is_num(c); 
            }
        }
        if (word.wordStr_.size() == punct.size()) all_num = false;

        if (punct_pos.empty()) { // no punct 
            word.toBeAdjustCandi_ = !all_num;
            out_line.wordVec_.push_back(word);
        } else if (word.wordStr_.size() == punct.size()) { //this word is all punct
            SSeqLEngWordInfor new_word;
            for (int k = 0; k < punct.size(); ++k) {
                // push this punct
                if (punct_pos[k] + 1 < word.wordStr_.size()) { // punct not last char in this word
                    new_word = split_word(word, word.path_pos_[punct_pos[k]], word.path_pos_[punct_pos[k]],
                            word.path_pos_[punct_pos[k] + 1] , false);
                    
                } else { // punct is last char in this word
                    new_word = split_word(word, word.path_pos_[punct_pos[k]], word.path_pos_[punct_pos[k]],
                            word.path_pos_[punct_pos[k]], false);
                    new_word.wid_ = word.left_ + word.wid_ - new_word.left_;
                    //std::cout<<"split_word:"<<new_word.wordStr_<<std::endl;
                }
                if (punct_pos[k] != 0) {
                    new_word.left_add_space_ = false;
                }
                    //std::cout<<"all punct:"<<new_word.wordStr_<<std::endl;
                out_line.wordVec_.push_back(word);
                out_line.wordVec_.push_back(new_word);
            }
        } else {
            int new_word_start = 0;
            bool need_adjust = true;
            SSeqLEngWordInfor new_word;
            for (int k = 0; k < punct.size(); ++k) {
                if (punct[k] == '-') {
                    if (punct_pos[k] != word.wordStr_.size() - 1 && punct_pos[k] != 0) {
                        need_adjust = false;
                        continue;
                    } else if (punct_pos[k] == 0 && is_num(word.wordStr_[punct_pos[k] + 1])) {
                        need_adjust = false;
                        continue;
                    }
                }
                if (punct[k] == '\'' && punct_pos[k] != 0) {
                    need_adjust = false;
                    continue;
                }
                if (all_num) {
                    need_adjust = false;
                    if ((punct[k] == '.' || punct[k] == ',' )
                            && punct_pos[k] != word.wordStr_.size() - 1) {
                        continue;
                    }
                    if (punct[k] == '%') {
                        continue;
                    }
                }
                if (punct[k] == '.' && num_of(punct, '.') >= 2
                        && num_of(punct, '.') == (word.wordStr_.size() / 2)) {
                    need_adjust = false;
                    continue;
                }
                
                // push the word before this punct
                if (punct_pos[k] != new_word_start) {
                    //std::cout << "punc pos: " << punct_pos[k] << " " << new_word_start << " " << word.path_pos_.size() << std::endl;
                    new_word = split_word(word, word.path_pos_[new_word_start], word.path_pos_[punct_pos[k] - 1],
                            word.path_pos_[punct_pos[k]], need_adjust && (!all_num));
                    if (new_word_start != 0) {
                        new_word.left_add_space_ = false;
                    }
                    out_line.wordVec_.push_back(new_word);
                }

                // push this punct
                //std::cout<<"path_pos:"<<std::endl;
                //for(unsigned int i = 0; i< word.path_pos_.size(); i++)
                //    std::cout<<word.path_pos_[i]<<std::endl;
         
                if (punct_pos[k] + 1 < word.wordStr_.size()) { // punct not last char in this word
                    new_word = split_word(word, word.path_pos_[punct_pos[k]], word.path_pos_[punct_pos[k]],
                            word.path_pos_[punct_pos[k] + 1], false);
                } else{ // punct is last char in this word
                    new_word = split_word(word, word.path_pos_[punct_pos[k]], word.path_pos_[punct_pos[k]],
                            word.maxVec_.size(), false);
                            //word.path_pos_[punct_pos[k]], false);
                    new_word.wid_ = word.left_ + word.wid_ - new_word.left_;
                }
                if (punct_pos[k] != 0) {
                    new_word.left_add_space_ = false;
                }
                out_line.wordVec_.push_back(new_word);
                new_word_start = punct_pos[k] + 1;

                need_adjust = true;
            }
            if (new_word_start < word.wordStr_.size()) { // deal with the last word if there are any left.
                new_word = split_word(word, word.path_pos_[new_word_start], word.path_pos_.back(),
                            word.maxVec_.size(), (!all_num) && need_adjust);
                            //word.path_pos_.back(), (!all_num) && need_adjust);
                new_word.wid_ = word.left_ + word.wid_ - new_word.left_;
                if (new_word_start != 0) {
                    new_word.left_add_space_ = false;
                }
                out_line.wordVec_.push_back(new_word);
            }
        }
    }

    line = out_line;
}

int CSeqLEngWordReg::extract_words(SSeqLEngWordInfor recog_res, SSeqLEngLineInfor & out_line)    
{
    int start_time = 0;
    int next_start_time = 0;
    int end_time = 0;
    //int time_num = recog_res.maxStr_.size();
    int time_num = recog_res.maxVec_.size();
    std::vector<int> max_vec = recog_res.maxVec_;

    while (1) {
        //_m_attention_decode =0
        if (_m_attention_decode) {
            // word starts at no space
            // max id is the labelsDim - 2 and space is th labelsDim_ - 3
            if (start_time < time_num && (max_vec[start_time] == labelsDim_ - 3)) {
                ++start_time;
                next_start_time = start_time;
                end_time = start_time;
                continue;
            }
            // word stops at last char before a space
            if (next_start_time < time_num && max_vec[next_start_time] != labelsDim_ - 3) {
                end_time = next_start_time;    
                ++next_start_time;
                continue;
            }
        }
        else {
              // std::cout<<"blank_:"<<blank_<<std::endl;
            // word starts at no space
            if (start_time < time_num && (max_vec[start_time] == blank_ - 3 || max_vec[start_time] == blank_)) {
                ++start_time;
                next_start_time = start_time;
                end_time = start_time;
                continue;
            }
            // word stops at last char before a space
            if (next_start_time < time_num && max_vec[next_start_time] != blank_ - 3) {
                if (max_vec[next_start_time] != blank_) {
                    end_time = next_start_time;    
                }
                ++next_start_time;
                continue;
            }
        }
        // something strange could happen
        if (end_time < start_time || end_time >= time_num) break;

        SSeqLEngWordInfor new_word = split_word(recog_res, start_time, end_time, next_start_time, true);
        //std::cout<<"new_word:"<<new_word.wordStr_<<std::endl;
        out_line.wordVec_.push_back(new_word);
        // go to next word
        start_time = next_start_time;
        end_time = start_time;

        if (start_time >= time_num - 1) break;
    }

    return 0;
}

int CSeqLEngWordReg::extract_words_att_cand(SSeqLEngWordInfor recog_res, SSeqLEngLineInfor & out_line)    
{
    for (int cand_id = 0; cand_id < recog_res.cand_word_vec.size(); cand_id++) {    

        SSeqLEngWordInfor recog_res_cand;
        recog_res_cand.left_ = recog_res.left_;
        recog_res_cand.top_  = recog_res.top_;
        recog_res_cand.wid_  = recog_res.wid_;
        recog_res_cand.hei_  = recog_res.hei_;
        recog_res_cand.dectScore_  = recog_res.dectScore_;
        recog_res_cand.mulRegScore_ = 0;
        recog_res_cand.compNum_ = recog_res.compNum_;
        recog_res_cand.leftSpace_ = recog_res.leftSpace_;
        recog_res_cand.rightSpace_ = recog_res.rightSpace_;
        recog_res_cand.sigWordFlag_ = recog_res.sigWordFlag_;
        recog_res_cand.isPunc_ = false;
        recog_res_cand.toBeAdjustCandi_ = true;
        recog_res_cand.maxVec_ = recog_res.cand_word_vec[cand_id].maxVec_;
        recog_res_cand.maxStr_ = recog_res.cand_word_vec[cand_id].maxStr_;
        recog_res_cand.regLogProb_ = recog_res.cand_word_vec[cand_id].regLogProb_;
        recog_res_cand.wordStr_ = recog_res.cand_word_vec[cand_id].wordStr_;

        int start_time = 0;
        int next_start_time = 0;
        int end_time = 0;
        int time_num = recog_res_cand.maxStr_.size();
        std::vector<int> max_vec = recog_res_cand.maxVec_;
        int valid_time_num = 0;
        for (int i = 0; i < time_num; i++) {
            if (max_vec[start_time] != 94 && max_vec[start_time] != 95) {
                valid_time_num += 1;
            }
        }

        while (1) {
            // word starts at no space
            if (start_time < time_num && (max_vec[start_time] == 94 || max_vec[start_time] == 95)) {
                ++start_time;
                next_start_time = start_time;
                end_time = start_time;
                continue;
            }
            // word stops at last char before a space
            if (next_start_time < time_num && max_vec[next_start_time] != 94) {
                if (max_vec[next_start_time] != 95) {
                    end_time = next_start_time;    
                }
                ++next_start_time;
                continue;
            }

            // something strange could happen
            if (end_time < start_time || end_time >= time_num) break;

            SSeqLEngWordInfor new_word = split_word(recog_res_cand, start_time, end_time, next_start_time, true);

            out_line.wordVec_.push_back(new_word);
            // go to next word
            start_time = next_start_time;
            end_time = start_time;

            if (start_time >= time_num - 1) break;
        }
    }

    return 0;
}

std::string remove_space(std::string input_str) {
    std::string res = "";
    for (int i = 0; i < input_str.size(); i++) {
        //std::cout << input_str << " " << res << " " << input_str[i] << std::endl;
        if (input_str[i] != ' ') {
            //std::cout << res << std::endl;
            res += input_str.substr(i, 1);
        }
    }
    return res;
}

int CSeqLEngWordReg::extract_words_att_space_cand(SSeqLEngWordInfor recog_res, SSeqLEngLineInfor & out_line)    
{
    std::string recog_res_str_without_space = "";
    recog_res_str_without_space = remove_space(recog_res.wordStr_);

    for (int cand_id = 0; cand_id < recog_res.cand_word_vec.size(); //std::min(1, (int)recog_res.cand_word_vec.size()); 
                    cand_id++) {    
        
        if (recog_res.cand_word_vec[cand_id].regLogProb_ < -3.5) {
            continue;
        }

        // ignore same line
        if (strcmp(recog_res.wordStr_.c_str(), 
                recog_res.cand_word_vec[cand_id].wordStr_.c_str()) == 0) {
            continue;
        }
        std::string cand_str_without_space = "";
        cand_str_without_space = remove_space(recog_res.cand_word_vec[cand_id].wordStr_);
        // ignore line with other diff
        if (strcmp(recog_res_str_without_space.c_str(),
                cand_str_without_space.c_str()) != 0) {
            continue;
        }

        /*bool low_prob_space = false;
        for (int id = 0; id < recog_res.cand_word_vec[cand_id].maxVec_.size(); id++) {
            if (recog_res.cand_word_vec[cand_id].maxVec_[id] == 94) {
                if (recog_res.cand_word_vec[cand_id].proMat_[id * 97 + 96] < 0.05) {
                    low_prob_space = true;
                    break;
                }
            }
        }
        if (low_prob_space) {
            continue;
        }*/

        SSeqLEngWordInfor recog_res_cand;
        recog_res_cand.left_ = recog_res.left_;
        recog_res_cand.top_  = recog_res.top_;
        recog_res_cand.wid_  = recog_res.wid_;
        recog_res_cand.hei_  = recog_res.hei_;
        recog_res_cand.dectScore_  = recog_res.dectScore_;
        recog_res_cand.mulRegScore_ = 0;
        recog_res_cand.compNum_ = recog_res.compNum_;
        recog_res_cand.leftSpace_ = recog_res.leftSpace_;
        recog_res_cand.rightSpace_ = recog_res.rightSpace_;
        recog_res_cand.sigWordFlag_ = recog_res.sigWordFlag_;
        recog_res_cand.isPunc_ = false;
        recog_res_cand.toBeAdjustCandi_ = true;
        recog_res_cand.maxVec_ = recog_res.cand_word_vec[cand_id].maxVec_;
        recog_res_cand.maxStr_ = recog_res.cand_word_vec[cand_id].maxStr_;
        recog_res_cand.regLogProb_ = recog_res.cand_word_vec[cand_id].regLogProb_;
        recog_res_cand.wordStr_ = recog_res.cand_word_vec[cand_id].wordStr_;
        recog_res_cand.proMat_ = recog_res.cand_word_vec[cand_id].proMat_;

        recog_res_cand.encoder_fea_num_ = recog_res.cand_word_vec[cand_id].encoder_fea_num_;
        recog_res_cand.encoder_fea_length_ = recog_res.cand_word_vec[cand_id].encoder_fea_length_;
        recog_res_cand.att_pos_st_id_set_ = recog_res.cand_word_vec[cand_id].att_pos_st_id_set_;
        recog_res_cand.att_pos_en_id_set_ = recog_res.cand_word_vec[cand_id].att_pos_en_id_set_;

        int start_time = 0;
        int next_start_time = 0;
        int end_time = 0;
        int time_num = recog_res_cand.maxStr_.size();
        std::vector<int> max_vec = recog_res_cand.maxVec_;
        int valid_time_num = 0;
        // for attention, labelsDim - 3 is the space id
        for (int i = 0; i < time_num; i++) {
            if (max_vec[start_time] != labelsDim_ - 3) {
                valid_time_num += 1;
            }
        }

        //build respond table from the cand str to recog str
        std::vector<int> respond_position;
        int last_position = 0;
        int r_i = 0;
        for (int i = 0; i < time_num; i++) {
            if (r_i < recog_res.maxVec_.size() && max_vec[i] == recog_res.maxVec_[r_i]) {
                last_position = r_i;   
                respond_position.push_back(last_position);
                r_i++;
                continue;
            }
            if (max_vec[i] == labelsDim_ - 3) {
                respond_position.push_back(last_position);
                continue;
            }
            while (r_i < recog_res.maxVec_.size() && recog_res.maxVec_[r_i] == labelsDim_ - 3) {
                r_i++;
                i--;
            }
        }

        while (1) {
            // word starts at no space
            if (start_time < time_num && max_vec[start_time] == labelsDim_ - 3) {
                ++start_time;
                next_start_time = start_time;
                end_time = start_time;
                continue;
            }
            // word stops at last char before a space
            if (next_start_time < time_num && max_vec[next_start_time] != labelsDim_ - 3) {
                end_time = next_start_time;    
                ++next_start_time;
                continue;
            }

            // something strange could happen
            if (end_time < start_time || end_time >= time_num) break;

            if (respond_position.size() == time_num && 
                respond_position[respond_position.size() - 1] + 1 == recog_res.maxVec_.size()) {
                SSeqLEngWordInfor new_word = split_word(recog_res_cand, start_time, end_time,
                                next_start_time, true, respond_position, recog_res);
                bool match = false;
                for (int i = 0; i < out_line.wordVec_.size(); i++) {
                    if (strcmp(new_word.wordStr_.c_str(), out_line.wordVec_[i].wordStr_.c_str()) == 0) {
                        match = true;
                        break;
                    }
                }
                if (!match) {
                    out_line.wordVec_.push_back(new_word);
                }
            } else {
                SSeqLEngWordInfor new_word = split_word(recog_res_cand, start_time, 
                                end_time, next_start_time, true);
                bool match = false;
                for (int i = 0; i < out_line.wordVec_.size(); i++) {
                    if (strcmp(new_word.wordStr_.c_str(), out_line.wordVec_[i].wordStr_.c_str()) == 0) {
                        match = true;
                        break;
                    }
                }
                if (!match) {
                    out_line.wordVec_.push_back(new_word);
                }
            }

            // go to next word
            start_time = next_start_time;
            end_time = start_time;

            if (start_time >= time_num - 1) break;
        }
    }

    return 0;
}

int CSeqLEngWordReg::extract_words_att_del_lowprob_cand(SSeqLEngWordInfor recog_res, SSeqLEngLineInfor & out_line)    
{
    int timeNum = recog_res.maxVec_.size();
    std::vector<int> del_lowprob_vec;
    bool has_lowprob = false;
    for (int time_id = 0; time_id < timeNum; time_id++) {
        int index = recog_res.maxVec_[time_id] + 2 + time_id * labelsDim_;
        if ((recog_res.proMat_[index] < 0.8 && recog_res.maxStr_[time_id] != '\'') ||
            ((time_id == timeNum - 1 || time_id == 0) && recog_res.proMat_[index] < 0.8) ||
            (recog_res.proMat_[index] < 0.4)){
            has_lowprob = true;
            continue;
        }
        del_lowprob_vec.push_back(recog_res.maxVec_[time_id]);
    }
    if (!has_lowprob) {
        return -1;
    }

    std::string recog_res_str_del_lowprob = pathToString(del_lowprob_vec);

    //std::cout << "low prob str " << recog_res.wordStr_ << " " << recog_res_str_del_lowprob << std::endl;
    for (int cand_id = 0; cand_id < recog_res.cand_word_vec.size(); //std::min(1, (int)recog_res.cand_word_vec.size()); 
                    cand_id++) {    
        
        if (recog_res.cand_word_vec[cand_id].regLogProb_ < -3.5) {
            continue;
        }

        std::string cand_str = recog_res.cand_word_vec[cand_id].wordStr_;
        //std::cout << "low prob compare: " <<  cand_str << " " << recog_res_str_del_lowprob << std::endl;
        if (strcmp(recog_res_str_del_lowprob.c_str(),
                cand_str.c_str()) != 0) {
            continue;
        }

        SSeqLEngWordInfor recog_res_cand;
        recog_res_cand.left_ = recog_res.left_;
        recog_res_cand.top_  = recog_res.top_;
        recog_res_cand.wid_  = recog_res.wid_;
        recog_res_cand.hei_  = recog_res.hei_;
        recog_res_cand.dectScore_  = recog_res.dectScore_;
        recog_res_cand.mulRegScore_ = 0;
        recog_res_cand.compNum_ = recog_res.compNum_;
        recog_res_cand.leftSpace_ = recog_res.leftSpace_;
        recog_res_cand.rightSpace_ = recog_res.rightSpace_;
        recog_res_cand.sigWordFlag_ = recog_res.sigWordFlag_;
        recog_res_cand.isPunc_ = false;
        recog_res_cand.toBeAdjustCandi_ = true;
        recog_res_cand.maxVec_ = recog_res.cand_word_vec[cand_id].maxVec_;
        recog_res_cand.maxStr_ = recog_res.cand_word_vec[cand_id].maxStr_;
        recog_res_cand.regLogProb_ = recog_res.cand_word_vec[cand_id].regLogProb_;
        recog_res_cand.wordStr_ = recog_res.cand_word_vec[cand_id].wordStr_;
        recog_res_cand.proMat_ = recog_res.cand_word_vec[cand_id].proMat_;

        recog_res_cand.encoder_fea_num_ = recog_res.cand_word_vec[cand_id].encoder_fea_num_;
        recog_res_cand.encoder_fea_length_ = recog_res.cand_word_vec[cand_id].encoder_fea_length_;
        recog_res_cand.att_pos_st_id_set_ = recog_res.cand_word_vec[cand_id].att_pos_st_id_set_;
        recog_res_cand.att_pos_en_id_set_ = recog_res.cand_word_vec[cand_id].att_pos_en_id_set_;

        int start_time = 0;
        int next_start_time = 0;
        int end_time = 0;
        int time_num = recog_res_cand.maxStr_.size();
        std::vector<int> max_vec = recog_res_cand.maxVec_;
        int valid_time_num = 0;
        // for attention, space id is the labelsDim_ - 3
        for (int i = 0; i < time_num; i++) {
            if (max_vec[start_time] != labelsDim_ - 3) {
                valid_time_num += 1;
            }
        }

        while (1) {
            // word starts at no space
            if (start_time < time_num && max_vec[start_time] == labelsDim_ - 3) {
                ++start_time;
                next_start_time = start_time;
                end_time = start_time;
                continue;
            }
            // word stops at last char before a space
            if (next_start_time < time_num && max_vec[next_start_time] != labelsDim_ -3) {
                end_time = next_start_time;    
                ++next_start_time;
                continue;
            }

            // something strange could happen
            if (end_time < start_time || end_time >= time_num) break;

            SSeqLEngWordInfor new_word = split_word(recog_res_cand, start_time, end_time, next_start_time, true);
            //std::cout << "wwwwwwwww " << recog_res_cand.maxStr_ << " " << new_word.maxStr_ << std::endl;
            bool match = false;
            for (int i = 0; i < out_line.wordVec_.size(); i++) {
                if (strcmp(new_word.wordStr_.c_str(), out_line.wordVec_[i].wordStr_.c_str()) == 0) {
                    match = true;
                    break;
                }
            }
            if (!match) {
                out_line.wordVec_.push_back(new_word);
            }
            // go to next word
            start_time = next_start_time;
            end_time = start_time;

            if (start_time >= time_num - 1) break;
        }
    }

    return 0;
}

int CSeqLEngWordReg::add_att_cand_score(SSeqLEngWordInfor recog_res, SSeqLEngLineInfor& out_line)    
{
    for (int cand_id = 0; cand_id < recog_res.cand_word_vec.size(); cand_id++) {    

        SSeqLEngWordInfor recog_res_cand;
        recog_res_cand.left_ = recog_res.left_;
        recog_res_cand.top_  = recog_res.top_;
        recog_res_cand.wid_  = recog_res.wid_;
        recog_res_cand.hei_  = recog_res.hei_;
        recog_res_cand.dectScore_  = recog_res.dectScore_;
        recog_res_cand.mulRegScore_ = 0;
        recog_res_cand.compNum_ = recog_res.compNum_;
        recog_res_cand.leftSpace_ = recog_res.leftSpace_;
        recog_res_cand.rightSpace_ = recog_res.rightSpace_;
        recog_res_cand.sigWordFlag_ = recog_res.sigWordFlag_;
        recog_res_cand.isPunc_ = false;
        recog_res_cand.toBeAdjustCandi_ = true;
        recog_res_cand.maxVec_ = recog_res.cand_word_vec[cand_id].maxVec_;
        recog_res_cand.maxStr_ = recog_res.cand_word_vec[cand_id].maxStr_;
        recog_res_cand.regLogProb_ = recog_res.cand_word_vec[cand_id].regLogProb_;
        recog_res_cand.wordStr_ = recog_res.cand_word_vec[cand_id].wordStr_;
        recog_res_cand.proMat_ = recog_res.cand_word_vec[cand_id].proMat_;

        recog_res_cand.encoder_fea_num_ = recog_res.cand_word_vec[cand_id].encoder_fea_num_;
        recog_res_cand.encoder_fea_length_ = recog_res.cand_word_vec[cand_id].encoder_fea_length_;
        recog_res_cand.att_pos_st_id_set_ = recog_res.cand_word_vec[cand_id].att_pos_st_id_set_;
        recog_res_cand.att_pos_en_id_set_ = recog_res.cand_word_vec[cand_id].att_pos_en_id_set_;

        int start_time = 0;
        int next_start_time = 0;
        int end_time = 0;
        int time_num = recog_res_cand.maxStr_.size();
        std::vector<int> max_vec = recog_res_cand.maxVec_;
        int valid_time_num = 0;
        //for attention, space id is the labelsDim_ -3
        for (int i = 0; i < time_num; i++) {
            if (max_vec[start_time] != labelsDim_ - 3) {
                valid_time_num += 1;
            }
        }

        while (1) {
            // word starts at no space
            if (start_time < time_num && max_vec[start_time] == labelsDim_ - 3) {
                ++start_time;
                next_start_time = start_time;
                end_time = start_time;
                continue;
            }
            // word stops at last char before a space
            if (next_start_time < time_num && max_vec[next_start_time] != labelsDim_ - 3) {
                end_time = next_start_time;    
                ++next_start_time;
                continue;
            }

            // something strange could happen
            if (end_time < start_time || end_time >= time_num) break;
            
            SSeqLEngWordInfor new_word = split_word(recog_res_cand, start_time, end_time, next_start_time, true);
            
            // every cand add 0.1
            std::vector<SSeqLEngWordInfor> &word_vec = out_line.wordVec_;
            if (new_word.wordStr_.size() > 3) {
                for (int i = 0; i < word_vec.size(); i++) {
                    float overlap_wid = (float)word_vec[i].wid_ / (std::max(new_word.left_ + new_word.wid_,
                                    word_vec[i].left_ + word_vec[i].wid_) -
                            std::min(new_word.left_, word_vec[i].left_ ));
                    if (strcmp(new_word.wordStr_.c_str(), word_vec[i].wordStr_.c_str()) == 0 && 
                                    overlap_wid > 0.7) {
                        word_vec[i].acc_score_ += 0.1;
                    }
                }
            }

            // go to next word
            start_time = next_start_time;
            end_time = start_time;

            if (start_time >= time_num - 1) break;
        }
    }

    return 0;
}

int CSeqLEngWordReg:: onceRectSeqLPredict( SSeqLEngRect &tempRect, SSeqLEngLineRect & lineRect, IplImage * image, \
        SSeqLEngWordInfor & tempWord, float extendRate, float horResizeRate, bool loadFlag ){

    float* data = NULL;
    int width = 0;
    int height = 0;
    int channel = 0;
    SSeqLEngRect ext_temp_rect = tempRect;
    int ext_boudary = extendRate*tempRect.hei_;
    int ext_boudary2 = -0.00*tempRect.hei_;
    ext_temp_rect.left_ = std::max(0, tempRect.left_-ext_boudary);
    ext_temp_rect.top_  = std::max(0, tempRect.top_ -  int(1.0*ext_boudary2));
    ext_temp_rect.wid_  =
            std::min(tempRect.left_+tempRect.wid_+ext_boudary-1, image->width-1)
            - ext_temp_rect.left_+1;
    ext_temp_rect.hei_  =
            std::min(tempRect.top_+tempRect.hei_+int(1.0*ext_boudary2) -1, image->height-1)
            - ext_temp_rect.top_+1;

    extractDataOneRect(image, ext_temp_rect, data, width, height, channel,
                       lineRect.isBlackWdFlag_, horResizeRate);

    int beam_size = 0;
    int weights_size = 0;
    int encoder_fea_num = 0;

    std::vector<float *> data_v;
    std::vector<float *> output_v;
    std::vector<float *> output_weights;
    std::vector<int*> output_labels;
    std::vector<int> img_width_v;
    std::vector<int> img_height_v;
    std::vector<int> img_channel_v;
    std::vector<int> out_len_v;
    data_v.push_back(data);
    img_width_v.push_back(width);
    img_height_v.push_back(height);
    img_channel_v.push_back(channel);

   // printInputData(data_v[0], img_width_v[0]*img_height_v[0]*img_channel_v[0]);
#ifdef COMPUT_TIME
    clock_t loct1, loct2;
    loct1 = clock();
#endif

    extern int g_predict_num;
    //////////////loadFlag = 0///////////
    if (loadFlag) { 
        float * matrix;
        int matrixLen;
        loadPredictMatrix(g_predict_num,  matrix, matrixLen);
        output_v.push_back(matrix);
        out_len_v.push_back(matrixLen);
    } else {
        ////////////_m_attention_decode = 0///////////
        if (_m_attention_decode) {
            beam_size = 0;
            inference_seq::predict(/* model */ seqLCdnnModel_,
                           /* batch_size */ 1,
                           /* inputs */ data_v,
                           /* widths */ img_width_v,
                           /* heights */ img_height_v,
                           /* channels */ img_channel_v,
                           /* beam_size */ beam_size,
                           /* weights_size */ weights_size,
                           /* output_probs */ output_v,
                           /* output_weights */ output_weights,
                           /* output_labels */ output_labels,
                           /* output_lengths */ out_len_v,
                           /* use_paddle */ _use_paddle,
                           /* att_modle */ true);
        }
        else {
            beam_size = 0;
            std::vector<int*> output_labels;
        //////////问题定位到这里，预测结果有误差/////
            inference_seq::predict(/* model */ seqLCdnnModel_,
                           /* batch_size */ _batch_size,
                           /* inputs */ data_v,
                           /* widths */ img_width_v,
                           /* heights */ img_height_v,
                           /* channels */ img_channel_v,
                           /* beam_size */ beam_size,
                           /* output_probs */ output_v,
                           /* output_labels */ output_labels,
                           /* output_lengths */ out_len_v,
                           /* use_paddle */ _use_paddle);
            for (unsigned int i = 0; i < output_labels.size(); i++){  
                if (output_labels[i]) {
                     free(output_labels[i]);
                    output_labels[i] = NULL;
                }
            }
        }
    }
    g_predict_num++;
    
//  std::cout <<"g_predict_num=" << g_predict_num <<"predict output : " << std::endl;
//  printOneOutputMatrix(output_v[0], labelsDim_, out_len_v[0]);

#ifdef COMPUT_TIME
    loct2 = clock();
    extern clock_t onlyRecogPredictClock;
    onlyRecogPredictClock += loct2 - loct1;
#endif

    if (_m_attention_decode) {
        int beam_len = 5;
        //extractoutonewordinfor(output_v[0], output_labels[0], labelsDim_, out_len_v[0], tempWord);
        extractoutonewordinfor(output_v[0], output_labels[0], labelsDim_, out_len_v[0], 
                         output_weights[0], weights_size, tempWord);
    }
    else {
        extractOutOneWordInfor(output_v[0], labelsDim_, out_len_v[0], tempWord);
    }
    tempWord.left_ = tempRect.left_;
    tempWord.top_  = tempRect.top_;
    tempWord.wid_  = tempRect.wid_;
    tempWord.hei_  = tempRect.hei_;
    tempWord.dectScore_  = tempRect.score_;
    tempWord.mulRegScore_ = 0;
    tempWord.compNum_ = tempRect.compNum_;
    tempWord.leftSpace_ = tempRect.leftSpace_;
    tempWord.rightSpace_ = tempRect.rightSpace_;
    tempWord.sigWordFlag_ = tempRect.sigWordFlag_;
    tempWord.isPunc_ = false;
    tempWord.toBeAdjustCandi_ = true;

    for (unsigned int i = 0; i < output_v.size(); i++) {
        if (output_v[i]) {
            free(output_v[i]);
            output_v[i] = NULL;
        }
    }
    for (unsigned int i = 0; i < data_v.size(); i++){
        if (data_v[i]) {
            free(data_v[i]);
            data_v[i] = NULL;
        }
    }
    for (unsigned int i = 0; i < output_labels.size(); i++) {
        if (output_labels[i]) {
            free(output_labels[i]);
            output_labels[i] = NULL;
        }
    }
}
 
int CSeqLEngWordReg::rect_seql_predict_asia(SSeqLEngRect &tempRect, SSeqLEngLineRect &lineRect,\
    const IplImage *image, SSeqLEngWordInfor &tempWord, \
    float horResizeRate) {

    float *data = NULL;
    int width = 0;
    int height = 0;
    int channel = 0;
    
    extract_data_rect_asia(image, tempRect, data, width, height, channel,\
            lineRect.isBlackWdFlag_, horResizeRate);

    std::vector<float *> data_v, output_v;
    std::vector<int>  img_width_v, img_height_v, img_channel_v,  out_len_v;

    data_v.push_back(data);
    img_width_v.push_back(width);
    img_height_v.push_back(height);
    img_channel_v.push_back(channel);
    int beam_size = 0;
    std::vector<int*> output_labels;
    inference_seq::predict(/* model */ seqLCdnnModel_,
                       /* batch_size */ 1,
                       /* inputs */ data_v,
                       /* widths */ img_width_v,
                       /* heights */ img_height_v,
                       /* channels */ img_channel_v,
                       /* beam_size */ beam_size,
                       /* output_probs */ output_v,
                       /* output_labels */ output_labels,
                       /* output_lengths */ out_len_v,
                       /* use_paddle */ _use_paddle);
    
    extract_word_infor_asia(output_v[0], labelsDim_, out_len_v[0], tempWord);
    tempWord.left_ = tempRect.left_;
    tempWord.top_  = tempRect.top_;
    tempWord.wid_  = tempRect.wid_;
    tempWord.hei_  = tempRect.hei_;
    tempWord.isPunc_ = false;
    tempWord.toBeAdjustCandi_ = true;
    tempWord.iNormWidth_ = _m_norm_img_w;
    tempWord.iNormHeight_ = _m_norm_img_h;
    
    for (unsigned int i = 0; i < output_v.size(); i++) {
        if (output_v[i]) {
            free(output_v[i]);
            output_v[i] = NULL;
        }
    }
    for (unsigned int i = 0; i < data_v.size(); i++){
        if (data_v[i]) {
            free(data_v[i]);
            data_v[i] = NULL;
        }
    }
    for (unsigned int i = 0; i < output_labels.size(); i++) {
        if (output_labels[i]) {
            free(output_labels[i]);
            output_labels[i] = NULL;
        }
    }

    return 0;
}
    
int CSeqLEngWordReg::recognize_eng_word_whole_line(const IplImage *orgImage,\
        SSeqLEngLineRect &lineRect, SSeqLEngLineInfor &outLineInfor,\
        SSeqLEngSpaceConf &spaceConf, bool quick_recognize) {
    if (lineRect.rectVec_.size() < 1) {
        return -1;
    }
    if (orgImage->width < SEQL_MIN_IMAGE_WIDTH || orgImage->height < SEQL_MIN_IMAGE_HEIGHT \
            || orgImage->width > SEQL_MAX_IMAGE_WIDTH || orgImage->height > SEQL_MAX_IMAGE_HEIGHT) {
        return -1;
    }

    quick_recognize = true;

    IplImage * image = cvCreateImage(cvSize(orgImage->width, orgImage->height), \
            orgImage->depth, orgImage->nChannels);
    cvCopy(orgImage, image, NULL);

    outLineInfor.wordVec_.clear();
    outLineInfor.left_ = image->width;
    outLineInfor.wid_  = 0;
    outLineInfor.top_  = image->height;
    outLineInfor.hei_  = 0;
    outLineInfor.imgWidth_ = image->width;
    outLineInfor.imgHeight_ = image->height;
    _m_en_ch_model = 1;

    // extract the line box 
    int line_left = lineRect.rectVec_[0].left_;
    int line_top = lineRect.rectVec_[0].top_;
    int line_right = lineRect.rectVec_[0].left_ + lineRect.rectVec_[0].wid_;
    int line_bottom = lineRect.rectVec_[0].top_ + lineRect.rectVec_[0].hei_;
    for (unsigned int i=1; i<lineRect.rectVec_.size(); i++){
        SSeqLEngRect & tempRect = lineRect.rectVec_[i];
        line_left = std::min(line_left, tempRect.left_);
        line_top = std::min(line_top, tempRect.top_);
        line_right = std::max(line_right, tempRect.left_ + tempRect.wid_);
        line_bottom = std::max(line_bottom, tempRect.top_ + tempRect.hei_);
    }

    int extBoudary = extendRate_*(line_bottom - line_top);
    int extBoudary2 = -0.05*(line_bottom - line_top);
    SSeqLEngRect tempLineRect = lineRect.rectVec_[0];
    tempLineRect.left_ = std::max(0, line_left - extBoudary);
    tempLineRect.top_ = std::max(0, line_top - int(1.0*extBoudary2));
    tempLineRect.wid_ = std::min(line_right+extBoudary, image->width) - tempLineRect.left_;
    tempLineRect.hei_ = std::min(line_bottom+ int(1.0*extBoudary2), image->height) - tempLineRect.top_;

    SSeqLEngWordInfor tempLineWord;
    //////////模型输出位置，tempLineWord///////////
    onceRectSeqLPredict(tempLineRect, lineRect, image, tempLineWord, 0, 1, false);
    //std::cout<<"tempLineWord.wordStr_:"<<tempLineWord.wordStr_<<"  tempLineWord.isPunc:"<<tempLineWord.isPunc_<<std::endl;

    if ((tempLineWord.wordStr_.length())>0) {
        spaceConf.avgCharWidth_ = tempLineRect.wid_/(tempLineWord.wordStr_.length());
    } else {
        spaceConf.avgCharWidth_ = tempLineRect.wid_;
    }
    //std::cout<<"tempLineWord.wordStr_:"<<tempLineWord.wordStr_<<std::endl;
    extract_words(tempLineWord, outLineInfor);

    /*std::cout<<"1.outLineInfor.wordVec_[i].wordStr_:";
    std::cout<<outLineInfor.wordVec_.size()<<std::endl;
    for(unsigned int i = 0; i<outLineInfor.wordVec_.size();++i){
        std::cout<<outLineInfor.wordVec_[i].wordStr_<<" ";
    }
    std::cout<<std::endl;*/

    if (lineRect.rectVec_.size() < 5 * tempLineWord.wordStr_.length() && !quick_recognize ){ 
        for (unsigned int i=0; i<lineRect.rectVec_.size(); i++) {
            SSeqLEngRect & tempRect = lineRect.rectVec_[i];
            SSeqLEngWordInfor tempWord;
            if (tempRect.compNum_ == 1) {
                bool loadFlag = true;
                onceRectSeqLPredict(tempRect,  lineRect,  image, tempWord, 0.1, 1, false);
                //outLineInfor.wordVec_.push_back(tempWord);
                extract_words(tempWord, outLineInfor);
            }
        }
    }

    /*
    int lineTimeNum = tempLineWord.proMat_.size() / labelsDim_;
    int saveWordSize = outLineInfor.wordVec_.size();
    for (unsigned int i = 0; i < lineTimeNum; i++) {
        for (unsigned int j = i; j < lineTimeNum; j++) {
            SSeqLEngRect tempRect = tempLineRect;
            int adExtBoundary = -0.0 * tempLineWord.hei_;
            int newExtBoundary = 0.05 * tempLineWord.hei_;
            tempRect.left_ = tempLineWord.left_ + i * tempLineWord.wid_/lineTimeNum + adExtBoundary ;
            tempRect.wid_ = tempLineWord.wid_ * (j - i + 1) /lineTimeNum - 2 * adExtBoundary;
            
            bool haveCorrespondRect = false;
            int correspondIndex = 0;
            int perImageLeft = tempLineWord.left_;        
            int perImageWid  = tempLineWord.wid_;
            int outImageLeft = std::max(perImageLeft, tempRect.left_-newExtBoundary);
            int outImageRight = std::min(tempRect.left_+tempRect.wid_+newExtBoundary-1, perImageLeft+perImageWid-1);
            int adI = std::max(int((outImageLeft - tempLineWord.left_)*lineTimeNum/tempLineWord.wid_), 0);
            int adJ = std::min(int((outImageRight- \
                        tempLineWord.left_+1)*lineTimeNum/tempLineWord.wid_), lineTimeNum-1);

            if (!(i==0 || tempLineWord.proMat_[(i-1)*labelsDim_+(labelsDim_-2)] > 0.1)) {
                continue;
            }

            if (!(j==lineTimeNum-1  || tempLineWord.proMat_[(j+1)*labelsDim_+(labelsDim_-2)]>0.1 )) {
                continue;
            }
            
            bool deleteFlag = false;
            for (unsigned k=i; k<=j; k++) {
                if (tempLineWord.proMat_[(k)*labelsDim_+(labelsDim_-2)] > 0.3) {
                    deleteFlag = true;
                    break;
                }
            }
            if (deleteFlag) {
                continue;
            }
            
            SSeqLEngWordInfor tempWord;
            int wordFlagType =  copyInforFormProjectWordTotal(tempLineWord, tempRect, tempWord, image, i, j);
            if (tempWord.wordStr_.length() < 1) {
                continue;
            }

            tempWord.compNum_ = 0;
            outLineInfor.wordVec_.push_back(tempWord);
        }
    }
    */
    
    //if ((outLineInfor.wordVec_.size()-saveWordSize) > lineTimeNum) {
    //   outLineInfor.wordVec_.resize(saveWordSize);
    //}
    outLineInfor.left_ = line_left;
    outLineInfor.top_  = line_top;
    outLineInfor.wid_ = line_right - line_left;
    outLineInfor.hei_ = line_bottom - line_top;

    std::vector<SSeqLEngWordInfor>::iterator ita = outLineInfor.wordVec_.begin();
    while (ita != outLineInfor.wordVec_.end()) {
        if ((ita->wordStr_.length()) == 0 || ita->toBeDelete_) {
            ita = outLineInfor.wordVec_.erase(ita);
        } else {
            ita++;
        }
    }

    int wordVecSize = outLineInfor.wordVec_.size();
    SSeqLEngLineInfor extend_line = outLineInfor; 

    for (unsigned int i = 0; i < wordVecSize; i++){
        extendVariationWord(extend_line, i);
    }

    // dealing with newly added words
    int new_size = extend_line.wordVec_.size();
    for (int i = wordVecSize; i < new_size; ++i) {
        extract_words(extend_line.wordVec_[i], outLineInfor);
    }

    /*std::cout<<"4.outLineInfor.wordVec_[i].wordStr_:";
    std::cout<<outLineInfor.wordVec_.size()<<std::endl;
    for(unsigned int i = 0; i<outLineInfor.wordVec_.size();++i){
        std::cout<<outLineInfor.wordVec_[i].wordStr_<<" "<<std::endl;
    }
    std::cout<<std::endl;*/

    //sunwanqi add trick to avoid new punct
    std::vector<int> unicode_vec;
    for(unsigned int j = 0; j<outLineInfor.wordVec_.size(); j++){
        unicode_vec.clear();
        const char * ps = outLineInfor.wordVec_[j].wordStr_.c_str();
        seq_lutf8_str_2_unicode_vec(ps, unicode_vec);
        std::vector<int> :: iterator iter = unicode_vec.begin();
   
        while(iter!=unicode_vec.end())
        {
            if (*iter == 164 ||*iter == 264302 || *iter == 262148)
            {
             *(iter - 1) = 46;
             unicode_vec.erase(iter); 
           }
            else
                iter++;
        }
        //std::cout<<"new unicode vector:"<<std::endl;
        std::string new_wordStr_ = seq_lunicode_vec_2_utf8_str(unicode_vec);
        //std::cout<<"new_wordStr_:"<<new_wordStr_<<std::endl;
        outLineInfor.wordVec_[j].wordStr_ = new_wordStr_;   
    }

    split_word_by_punct(outLineInfor);

    //refine the maxVec to make it correspond to the word
    for (int i = 0; i < outLineInfor.wordVec_.size(); i++) {
        std::vector<int> path_pos;
        std::vector<int> path = path_elimination(outLineInfor.wordVec_[i].maxVec_, path_pos);
        outLineInfor.wordVec_[i].maxVec_.clear();
        for (int k = 0; k < path.size(); k++) {
            outLineInfor.wordVec_[i].maxVec_.push_back(path[k]);
        }
    }

    //std::cout<<"fix maxvec2:"<<std::endl;
    for(unsigned int i = 0; i<outLineInfor.wordVec_.size();++i){
        for(unsigned int j = 0; j < outLineInfor.wordVec_[i].maxVec_.size();j++)
        { 
            if(outLineInfor.wordVec_[i].maxVec_[j] == 95)
            {   
                std::vector<int> replace_vec;
                replace_vec.push_back(183);
                std::string replaceStr = seq_lunicode_vec_2_utf8_str(replace_vec);
                //std::cout<<"replaceStr:"<<replaceStr<<std::endl;
                outLineInfor.wordVec_[i].wordStr_ = replaceStr;
            }
            if(outLineInfor.wordVec_[i].maxVec_[j] == 96)
            {
                std::vector<int> replace_vec;
                 replace_vec.push_back(9679);
                 std::string replaceStr = seq_lunicode_vec_2_utf8_str(replace_vec);
                 //std::cout<<"replaceStr:"<<replaceStr<<std::endl;
                 outLineInfor.wordVec_[i].wordStr_ = replaceStr;
            }
        }
    }

    if (outLineInfor.wordVec_.size() > 300) {
       outLineInfor.wordVec_.resize(wordVecSize); 
    }
    sort(outLineInfor.wordVec_.begin(), outLineInfor.wordVec_.end(), leftSeqLWordRectFirst);
    cvReleaseImage(&image);
    image = NULL;
    return 0;
}

// recognize the english row in batch mode
int CSeqLEngWordReg::recognize_eng_word_whole_line_batch(const IplImage *orgImage,\
    SSeqLEngLineRect &lineRect, SSeqLEngLineInfor &outLineInfor,\
    SSeqLEngSpaceConf &spaceConf, bool quick_recognize) {
    
    if (lineRect.rectVec_.size() < 1) {
        return -1;
    }
    if (orgImage->width < SEQL_MIN_IMAGE_WIDTH || orgImage->height < SEQL_MIN_IMAGE_HEIGHT \
            || orgImage->width > SEQL_MAX_IMAGE_WIDTH || orgImage->height > SEQL_MAX_IMAGE_HEIGHT) {
        return -1;
    }

    IplImage * image = cvCreateImage(cvSize(orgImage->width, orgImage->height), \
            orgImage->depth, orgImage->nChannels);
    cvCopy(orgImage, image, NULL);

    outLineInfor.wordVec_.clear();
    outLineInfor.left_ = image->width;
    outLineInfor.wid_  = 0;
    outLineInfor.top_  = image->height;
    outLineInfor.hei_  = 0;
    outLineInfor.imgWidth_ = image->width;
    outLineInfor.imgHeight_ = image->height;
    _m_en_ch_model = 1;

    // extract the line box 
    int line_left = lineRect.rectVec_[0].left_;
    int line_top = lineRect.rectVec_[0].top_;
    int line_right = lineRect.rectVec_[0].left_ + lineRect.rectVec_[0].wid_;
    int line_bottom = lineRect.rectVec_[0].top_ + lineRect.rectVec_[0].hei_;
    for (unsigned int i = 1; i < lineRect.rectVec_.size(); i++){
        SSeqLEngRect & tempRect = lineRect.rectVec_[i];
        line_left = std::min(line_left, tempRect.left_);
        line_top = std::min(line_top, tempRect.top_);
        line_right = std::max(line_right, tempRect.left_ + tempRect.wid_);
        line_bottom = std::max(line_bottom, tempRect.top_ + tempRect.hei_);
    }

    int extBoudary = extendRate_*(line_bottom - line_top);
    int extBoudary2 = -0.05*(line_bottom - line_top);
    SSeqLEngRect tempLineRect = lineRect.rectVec_[0];
    tempLineRect.left_ = std::max(0, line_left - extBoudary);
    tempLineRect.top_ = std::max(0, line_top - int(1.0*extBoudary2));
    tempLineRect.wid_ = std::min(line_right + extBoudary, image->width) - tempLineRect.left_;
    tempLineRect.hei_ = std::min(line_bottom + int(1.0*extBoudary2), image->height)
        - tempLineRect.top_;

    SSeqLEngWordInfor tempLineWord;
    predict_rect(tempLineRect, lineRect, image, tempLineWord, 0, 1);

    if ((tempLineWord.wordStr_.length()) > 0) {
        spaceConf.avgCharWidth_ = tempLineRect.wid_ / (tempLineWord.wordStr_.length());
    } else {
        spaceConf.avgCharWidth_ = tempLineRect.wid_;
    }

    if (lineRect.rectVec_.size() < 5 * tempLineWord.wordStr_.length() \
        && !quick_recognize) { 
        for (unsigned int i = 0; i<lineRect.rectVec_.size(); i++) {
            SSeqLEngRect & tempRect = lineRect.rectVec_[i];
            SSeqLEngWordInfor tempWord;
            if (tempRect.compNum_ == 1) {
                bool loadFlag = true;
                predict_rect(tempRect,  lineRect,  image, tempWord, 0.1, 1);
                outLineInfor.wordVec_.push_back(tempWord);
            }
        }
    }

    int lineTimeNum = tempLineWord.proMat_.size() / labelsDim_;
    int saveWordSize = outLineInfor.wordVec_.size();
    for (unsigned int i = 0; i < lineTimeNum; i++) {
        for (unsigned int j = i; j < lineTimeNum; j++) {
            SSeqLEngRect tempRect = tempLineRect;
            int adExtBoundary = -0.0 * tempLineWord.hei_;
            int newExtBoundary = 0.05 * tempLineWord.hei_;
            tempRect.left_ = tempLineWord.left_ + 
                i * tempLineWord.wid_ / lineTimeNum + adExtBoundary;
            tempRect.wid_ = tempLineWord.wid_ * (j - i + 1) / lineTimeNum
                - 2 * adExtBoundary;
            
            bool haveCorrespondRect = false;
            int correspondIndex = 0;
            int perImageLeft = tempLineWord.left_;        
            int perImageWid  = tempLineWord.wid_;
            int outImageLeft = std::max(perImageLeft, tempRect.left_ - newExtBoundary);
            int outImageRight = std::min(tempRect.left_ + tempRect.wid_ + newExtBoundary - 1,
                perImageLeft+perImageWid-1);
            int adI = std::max(int((outImageLeft - tempLineWord.left_) * 
                lineTimeNum / tempLineWord.wid_), 0);
            int adJ = std::min(int((outImageRight- tempLineWord.left_ + 1) 
                * lineTimeNum / tempLineWord.wid_), lineTimeNum - 1);

            if (!(i == 0 || 
                tempLineWord.proMat_[(i - 1) * labelsDim_ + (labelsDim_ - 2)] > 0.1)) {
                continue;
            }

            if (!(j == lineTimeNum - 1  
                || tempLineWord.proMat_[(j + 1) * labelsDim_ + (labelsDim_ - 2)] > 0.1)) {
                continue;
            }
            
            bool deleteFlag = false;
            for (unsigned k = i; k <= j; k++) {
                if (tempLineWord.proMat_[(k) * labelsDim_ + (labelsDim_ - 2)] > 0.3) {
                    deleteFlag = true;
                    break;
                }
            }
            if (deleteFlag) {
                continue;
            }
            
            SSeqLEngWordInfor tempWord;
            int wordFlagType =  copyInforFormProjectWordTotal(tempLineWord, 
                tempRect, tempWord, image, i, j);
            if (tempWord.wordStr_.length() < 1) {
                continue;
            }

            tempWord.compNum_ = 0;
            outLineInfor.wordVec_.push_back(tempWord);
        }
    }
    
    if ((outLineInfor.wordVec_.size() - saveWordSize) > lineTimeNum) {
        outLineInfor.wordVec_.resize(saveWordSize);
    }
    outLineInfor.left_ = line_left;
    outLineInfor.top_  = line_top;
    outLineInfor.wid_ = line_right - line_left;
    outLineInfor.hei_ = line_bottom - line_top;

    std::vector<SSeqLEngWordInfor>::iterator ita = outLineInfor.wordVec_.begin();
    while (ita != outLineInfor.wordVec_.end()) {
        if ((ita->wordStr_.length()) == 0 || ita->toBeDelete_) {
            ita = outLineInfor.wordVec_.erase(ita);
        } else {
            ita++;
        }
    }    

    int wordVecSize = outLineInfor.wordVec_.size();
    for (unsigned int i = 0; i < wordVecSize; i++) {
        SSeqLEngWordInfor &tempWord = outLineInfor.wordVec_[i];
    }
    
    saveWordSize = outLineInfor.wordVec_.size();
    for (unsigned int i = 0; i < wordVecSize; i++){
        SSeqLEngWordInfor &tempWord = outLineInfor.wordVec_[i];
        extendVariationWord(outLineInfor, i);
    }
    if (outLineInfor.wordVec_.size() > 300) {
        outLineInfor.wordVec_.resize(saveWordSize); 
    }
    sort(outLineInfor.wordVec_.begin(), outLineInfor.wordVec_.end(), leftSeqLWordRectFirst);
    cvReleaseImage(&image);
    image = NULL;

    return 0;
}

int CSeqLEngWordReg::predict_rect(SSeqLEngRect &tempRect,
    SSeqLEngLineRect & lineRect, IplImage * image, \
    SSeqLEngWordInfor &tempWord, float extendRate, float horResizeRate) {

    float* data = NULL;
    int width = 0;
    int height = 0;
    int channel = 0;
    SSeqLEngRect ext_temp_rect = tempRect;
    int ext_boudary = extendRate*tempRect.hei_;
    int ext_boudary2 = -0.00*tempRect.hei_;
    
    ext_temp_rect.left_ = std::max(0, tempRect.left_-ext_boudary);
    ext_temp_rect.top_  = std::max(0, tempRect.top_ -  int(1.0*ext_boudary2));
    ext_temp_rect.wid_  =
            std::min(tempRect.left_+tempRect.wid_+ext_boudary-1, image->width-1)
            - ext_temp_rect.left_+1;
    ext_temp_rect.hei_  =
            std::min(tempRect.top_+tempRect.hei_+int(1.0*ext_boudary2) -1, image->height-1)
            - ext_temp_rect.top_+1;

    extractDataOneRect(image, ext_temp_rect, data, width, height, channel,
                       lineRect.isBlackWdFlag_, horResizeRate);

    std::vector<float *> data_v;
    std::vector<float *> output_v;
    std::vector<int> img_width_v;
    std::vector<int> img_height_v;
    std::vector<int> img_channel_v;
    std::vector<int> out_len_v;
    data_v.push_back(data);
    img_width_v.push_back(width);
    img_height_v.push_back(height);
    img_channel_v.push_back(channel);

    int beam_size = 0;
    std::vector<int*> output_labels;
    inference_seq::predict(/* model */ seqLCdnnModel_,
    /* batch_size */ 1,
    /* inputs */ data_v,
    /* widths */ img_width_v,
    /* heights */ img_height_v,
    /* channels */ img_channel_v,
    /* beam_size */ beam_size,
    /* output_probs */ output_v,
    /* output_labels */ output_labels,
    /* output_lengths */ out_len_v,
    /* use_paddle */ _use_paddle);

    extractOutOneWordInfor(output_v[0], labelsDim_, out_len_v[0], tempWord);
    tempWord.left_ = tempRect.left_;
    tempWord.top_  = tempRect.top_;
    tempWord.wid_  = tempRect.wid_;
    tempWord.hei_  = tempRect.hei_;
    tempWord.dectScore_  = tempRect.score_;
    tempWord.mulRegScore_ = 0;
    tempWord.compNum_ = tempRect.compNum_;
    tempWord.leftSpace_ = tempRect.leftSpace_;
    tempWord.rightSpace_ = tempRect.rightSpace_;
    tempWord.sigWordFlag_ = tempRect.sigWordFlag_;
    tempWord.isPunc_ = false;
    tempWord.toBeAdjustCandi_ = true;

    for (unsigned int i = 0; i < output_v.size(); i++) {
        if (output_v[i]) {
            free(output_v[i]);
            output_v[i] = NULL;
        }
    }
    for (unsigned int i = 0; i < data_v.size(); i++){
        if (data_v[i]) {
            free(data_v[i]);
            data_v[i] = NULL;
        }
    }
    for (unsigned int i = 0; i < output_labels.size(); i++) {
        if (output_labels[i]) {
            free(output_labels[i]);
            output_labels[i] = NULL;
        }
    }

    return 0;
}

bool  leftSeqLWordRectFirst( const SSeqLEngWordInfor & r1, const SSeqLEngWordInfor & r2 ){
    return r1.left_ < r2.left_;
}

int CSeqLEngWordReg::extract_single_char_infor(float *matrix, int lablesDim, int len,\
    SSeqLEngWordInfor &outWord) {

    int time_num = len / labelsDim_;
    outWord.proMat_.resize(len);
    memcpy(&(outWord.proMat_[0]), matrix, sizeof(float) * len);

    std::vector<int> max_vec;
    float max_value = 0;
    int char_idx = -1;

    max_vec.resize(time_num);
    for (unsigned int i = 0; i < time_num; i++) {
        float *acts = matrix + i * labelsDim_;
        max_vec[i] = argMaxActs(acts, labelsDim_);
        if (max_vec[i] == blank_) {
            continue ;
        }
        
        if (max_value < acts[max_vec[i]]) {
            max_value = acts[max_vec[i]];
            char_idx = max_vec[i];
        }
    }
    outWord.maxVec_ = max_vec; 
    outWord.maxStr_ = pathToStringHaveBlank(max_vec);
    outWord.pathVec_.push_back(char_idx);
    outWord.regLogProb_ = safeLog(max_value);
    outWord.reg_prob = max_value;
    outWord.wordStr_ = "";
    if (char_idx != blank_ && char_idx != -1) {
        outWord.wordStr_ = tabelStrVec_[char_idx];
    }

    int show_recog_rst_flag = g_ini_file->get_int_value("SHOW_RECOG_RST",
            IniFile::_s_ini_section_global);
    if (show_recog_rst_flag == 1) {
        // xie shufu add   
        std::cout << "maxStr_:" << outWord.maxStr_
            << " wordStr_: " << outWord.wordStr_ \
            << " regLogProb: " << outWord.regLogProb_ 
            << " timeNum: " << time_num << std::endl;
    }

    return 0;
}

int CSeqLEngWordReg::rect_seql_predict_single_char(SSeqLEngRect &tempRect, int black_bg_flag,\
    IplImage *image, SSeqLEngWordInfor &tempWord) {
    using std::vector;
    
    float *data = NULL;
    int width = 0;
    int height = 0;
    int channel = 0;
    extract_data_single_char(image, tempRect, data, width, height, channel, black_bg_flag);

    vector<float *> data_v, output_v;
    vector<int>  img_width_v, img_height_v, img_channel_v,  out_len_v;
    data_v.push_back(data);
    img_width_v.push_back(width);
    img_height_v.push_back(height);
    img_channel_v.push_back(channel);

    int beam_size = 0;
    std::vector<int*> output_labels;
    inference_seq::predict(/* model */ seqLCdnnModel_,
                       /* batch_size */ 1,
                       /* inputs */ data_v,
                       /* widths */ img_width_v,
                       /* heights */ img_height_v,
                       /* channels */ img_channel_v,
                       /* beam_size */ beam_size,
                       /* output_probs */ output_v,
                       /* output_labels */ output_labels,
                       /* output_lengths */ out_len_v,
                       /* use_paddle */ _use_paddle);
 
    extract_single_char_infor(output_v[0], labelsDim_, out_len_v[0], tempWord);
    tempWord.left_ = tempRect.left_;
    tempWord.top_  = tempRect.top_;
    tempWord.wid_  = tempRect.wid_;
    tempWord.hei_  = tempRect.hei_;
    tempWord.iNormWidth_ = _m_norm_img_w;
    tempWord.iNormHeight_ = _m_norm_img_h;
    
    for (unsigned int i = 0; i < output_v.size(); i++) {
        if (output_v[i]) {
            free(output_v[i]);
            output_v[i] = NULL;
        }
    }
    for (unsigned int i = 0; i < data_v.size(); i++){
        if (data_v[i]) {
            free(data_v[i]);
            data_v[i] = NULL;
        }
    }
    for (unsigned int i = 0; i < output_labels.size(); i++) {
        if (output_labels[i]) {
            free(output_labels[i]);
            output_labels[i] = NULL;
        }
    }

    return 0;
}

void CSeqLEngWordReg::extract_data_single_char(IplImage *image, SSeqLEngRect &tempRect, \
    float *&data, int & width, int & height, int & channel, \
    bool isBlackWdFlag) {
   
    // image-->	
    IplImage tmp_image = IplImage(*image);
    IplImage* sub_image = cvCreateImage(cvSize(tempRect.wid_, tempRect.hei_),\
            image->depth, image->nChannels);
    cvSetImageROI(&tmp_image, cvRect(tempRect.left_, tempRect.top_, tempRect.wid_, tempRect.hei_));
    cvCopy(&tmp_image, sub_image);
    cvResetImageROI(&tmp_image);
    IplImage* gray_image = cvCreateImage(cvSize(sub_image->width, sub_image->height), \
            sub_image->depth, 1);
    if (sub_image->nChannels != 1) {
        cvCvtColor(sub_image, gray_image, CV_BGR2GRAY);
    } else {
        cvCopy(sub_image, gray_image);
    }
    
    CvSize sz;
    sz.height = seqLNormheight_;
    sz.width = gray_image->width * sz.height * 1.0 / gray_image->height;
    if (sz.width < sz.height) {
        sz.width = sz.height;
    }
    IplImage *dstImg = cvCreateImage(sz, gray_image->depth, gray_image->nChannels);
    cvResize(gray_image, dstImg, CV_INTER_CUBIC);
    cvReleaseImage(&gray_image);
    gray_image = dstImg;
   
    _m_norm_img_h = gray_image->height;
    _m_norm_img_w = gray_image->width;
    extendSlimImage(gray_image, 1.0);
    
    int extract_data_flag = g_ini_file->get_int_value("DRAW_DATA_FOR_RECOG",
            IniFile::_s_ini_section_global);
    if (extract_data_flag == 1) {
        // save the image data to check
        char file_path[256];
        snprintf(file_path, 256, "%s/%s-%d-rect-norm-%d-%d.jpg",\
                g_psave_dir, g_pimage_title,\
                g_iline_index, g_irct_index, _m_en_ch_model);
        cvSaveImage(file_path, gray_image);
    }
    data = (float *)malloc(gray_image->width * gray_image->height \
            * gray_image->nChannels * sizeof(float));
    int index = 0;
    for (int h = 0; h < gray_image->height; h++) {
        for (int w = 0; w < gray_image->width; w++) {
            for (int c = 0; c < gray_image->nChannels; c++) {
                float value = cvGet2D(gray_image, h, w).val[c];
                data[index] = value - seqLCdnnMean_;
                index++;
            }
        }
    }

    width = gray_image->width;
    height = gray_image->height;
    channel = gray_image->nChannels;
    cvReleaseImage(&gray_image);
    cvReleaseImage(&sub_image);

    return ;
}

void CSeqLEngWordReg::set_candidate_num(int candidate_num) {
    if (candidate_num > 0 && candidate_num <= 20) {
        _m_cand_num = candidate_num;
    }

    return ;
}

void CSeqLEngWordReg::set_enhance_recog_model_row(
    /*enhance_recg_model*/CSeqLEngWordRegModel *p_recog_model,
    /*threshold*/float recog_thresh) {
    _m_enhance_model = p_recog_model;
    _m_recog_thresh = recog_thresh;
    return ;  
}
void CSeqLEngWordReg::set_openmp_thread_num(
    /*openmp_thread_num*/int thread_num) {
    _m_openmp_thread_num = thread_num;
    return ;
}

void CSeqLEngWordReg::set_recog_model_type(
        /*model_type:0,1*/int model_type) {
    _m_recog_model_type = model_type;
    return ; 
}

// recognize for euro language
int CSeqLEngWordReg::recognizeWordLineRectTotalPerTime(const IplImage * orgImage,\
        SSeqLEngLineRect & lineRect, SSeqLEngLineInfor & outLineInfor,\
        SSeqLEngSpaceConf & spaceConf) {

    if (lineRect.rectVec_.size() < 1) {
        return -1;
    }

    IplImage * image = cvCreateImage( cvSize(orgImage->width, orgImage->height), \
            orgImage->depth, orgImage->nChannels );
    cvCopy(orgImage, image, NULL);

    outLineInfor.wordVec_.clear();
    outLineInfor.left_ = image->width;
    outLineInfor.wid_  = 0;
    outLineInfor.top_  = image->height;
    outLineInfor.hei_  = 0;
    outLineInfor.imgWidth_ = image->width;
    outLineInfor.imgHeight_ = image->height;

    if (orgImage->width<SEQL_MIN_IMAGE_WIDTH || orgImage->height<SEQL_MIN_IMAGE_HEIGHT \
            || orgImage->width>SEQL_MAX_IMAGE_WIDTH || orgImage->height>SEQL_MAX_IMAGE_HEIGHT) {
        cvReleaseImage(& image);
        image=NULL;
        return -1;
    }
    _m_en_ch_model = 1;

    // extract the line box 
    int lineLeft = lineRect.rectVec_[0].left_;
    int lineTop = lineRect.rectVec_[0].top_;
    int lineRight = lineRect.rectVec_[0].left_ + lineRect.rectVec_[0].wid_;
    int lineBottom = lineRect.rectVec_[0].top_ + lineRect.rectVec_[0].hei_;
    for (unsigned int i = 1; i < lineRect.rectVec_.size(); i++) {
        SSeqLEngRect & tempRect = lineRect.rectVec_[i];
        lineLeft = std::min(lineLeft, tempRect.left_);
        lineTop = std::min(lineTop, tempRect.top_);
        lineRight = std::max(lineRight, tempRect.left_ + tempRect.wid_);
        lineBottom = std::max(lineBottom, tempRect.top_ + tempRect.hei_);
    }

    int extBoudary = extendRate_*(lineBottom - lineTop);
    int extBoudary2 = -0.05*(lineBottom - lineTop);
    SSeqLEngRect tempLineRect = lineRect.rectVec_[0];
    tempLineRect.left_ = std::max(0, lineLeft - extBoudary);
    tempLineRect.top_ = std::max(0, lineTop - int(1.0*extBoudary2));
    tempLineRect.wid_ = std::min(lineRight+extBoudary, image->width) - tempLineRect.left_;
    tempLineRect.hei_ = std::min(lineBottom+ int(1.0*extBoudary2), image->height) - tempLineRect.top_;

    SSeqLEngWordInfor tempLineWord;
    onceRectSeqLPredict(tempLineRect, lineRect, image, tempLineWord, 0, 1, /*true */false);

    if ((tempLineWord.wordStr_.length()) > 0) {
        spaceConf.avgCharWidth_ = tempLineRect.wid_/(tempLineWord.wordStr_.length());
    }
    else{
        spaceConf.avgCharWidth_ = tempLineRect.wid_;
    }

    if (lineRect.rectVec_.size()< 5*tempLineWord.wordStr_.length()) { 
        for (unsigned int i=0; i<lineRect.rectVec_.size(); i++){
            SSeqLEngRect & tempRect = lineRect.rectVec_[i];
            SSeqLEngWordInfor tempWord;
            if (tempRect.compNum_ == 1) {
                bool loadFlag = true;
                onceRectSeqLPredict(tempRect,  lineRect,  image, tempWord, 0.1, 1, /*loadFlag*/false);
                outLineInfor.wordVec_.push_back(tempWord);
                /*
                if (loadFlag==false){
                   std::cout << "load false Str:" <<tempWord.wordStr_ << std::endl;
                }*/
            }
        }
    }
  
    int lineTimeNum = tempLineWord.proMat_.size() / labelsDim_;
    int saveWordSize = outLineInfor.wordVec_.size();
    for (unsigned int i = 0; i < lineTimeNum; i++) {
        for (unsigned int j = i; j < lineTimeNum; j++){

            SSeqLEngRect tempRect = tempLineRect;
            
            int adExtBoundary = -0.0*tempLineWord.hei_;
            int newExtBoundary = 0.05*tempLineWord.hei_;
            tempRect.left_ = tempLineWord.left_ + 
                i * tempLineWord.wid_ / lineTimeNum + adExtBoundary ;
            tempRect.wid_ = tempLineWord.wid_ * (j - i + 1) / lineTimeNum - 2 * adExtBoundary;
            
            bool haveCorrespondRect = false;
            int correspondIndex = 0;
            int perImageLeft = tempLineWord.left_;        
            int perImageWid  = tempLineWord.wid_;
            int outImageLeft = std::max(perImageLeft, tempRect.left_ - newExtBoundary);
            int outImageRight = std::min(tempRect.left_+tempRect.wid_ + newExtBoundary - 1,
			 	perImageLeft+perImageWid-1);
            int adI = std::max(int((outImageLeft - tempLineWord.left_) * lineTimeNum / tempLineWord.wid_), 0);
            int adJ = (std::min(int((outImageRight- \
                        tempLineWord.left_+1)*lineTimeNum/tempLineWord.wid_), lineTimeNum-1));

            if (!(i==0 || tempLineWord.proMat_[(i-1)*labelsDim_+(labelsDim_-2)]>0.1)) {
                continue;
            }

            if (!(j == lineTimeNum - 1 || 
                tempLineWord.proMat_[(j + 1) * labelsDim_+(labelsDim_-2)]>0.1)){
                continue;
            }
            
            bool deleteFlag = false;
            for (unsigned k = i; k <= j; k++) {
                if (tempLineWord.proMat_[(k)*labelsDim_ + (labelsDim_ - 2)] > 0.3) {
                    deleteFlag = true;
                    break;
                }
            }
            if (deleteFlag) {
                continue;
            }
            SSeqLEngWordInfor tempWord;
            int wordFlagType = copyInforFormProjectWordTotal(tempLineWord, tempRect, tempWord, image, i, j);
            if (tempWord.wordStr_.length() < 1){
                continue;
            }

            tempWord.compNum_ = 0;
            outLineInfor.wordVec_.push_back(tempWord);
        }
    }
    
    if ((outLineInfor.wordVec_.size() - saveWordSize) > lineTimeNum) {
       outLineInfor.wordVec_.resize(saveWordSize);
    }
    outLineInfor.left_ = lineLeft;
    outLineInfor.top_  = lineTop;
    outLineInfor.wid_ = lineRight - lineLeft;
    outLineInfor.hei_ = lineBottom - lineTop;

    std::vector<SSeqLEngWordInfor>::iterator ita = outLineInfor.wordVec_.begin();
    while (ita != outLineInfor.wordVec_.end())
    {
        if ((ita->wordStr_.length()) == 0 || ita->toBeDelete_)
            ita = outLineInfor.wordVec_.erase(ita);
        else
            ita++;
    }    

    int wordVecSize = outLineInfor.wordVec_.size();
    saveWordSize = outLineInfor.wordVec_.size();
    for (unsigned int i = 0; i < wordVecSize; i++){
        SSeqLEngWordInfor &tempWord = outLineInfor.wordVec_[i];
        extendVariationWord(outLineInfor, i);
    }
    if (outLineInfor.wordVec_.size() > 300) {
       outLineInfor.wordVec_.resize(saveWordSize); 
    }
    sort(outLineInfor.wordVec_.begin(), 
        outLineInfor.wordVec_.end(), leftSeqLWordRectFirst);

    cvReleaseImage(&image);
    image = NULL;

    return 0;
}

int CSeqLEngWordReg::extract_attention_posistion(SSeqLEngWordInfor& LineWord) {
    
    //for the main word 
    LineWord.att_pos_st_id_set_.clear();
    LineWord.att_pos_en_id_set_.clear();
    for (int i = 0; i < LineWord.maxVec_.size(); i++) {
        int st_id = -1;
        int en_id = LineWord.maxVec_.size() + 1;
        float max_value = -1;
        LineWord.encoder_fea_length_ = (float)(LineWord.wid_) / LineWord.encoder_fea_num_;
        for (int j = 0; j < LineWord.encoder_fea_num_; j++) {
            if (LineWord.att_weights_[j + i * LineWord.encoder_fea_num_] > max_value) {
                max_value = LineWord.att_weights_[j + i * LineWord.encoder_fea_num_];
                st_id = j;
            }
        }
        if (st_id >= 0) {
            for (en_id = st_id; en_id < LineWord.encoder_fea_num_; en_id++) {
                if (LineWord.att_weights_[en_id + i * LineWord.encoder_fea_num_] < 0.001) {
                    break;
                }
            }
            en_id -= 1;
            en_id = std::min(LineWord.encoder_fea_num_ - 1, en_id);
        }
        LineWord.att_pos_st_id_set_.push_back(st_id);
        LineWord.att_pos_en_id_set_.push_back(en_id);
    }
    assert(LineWord.att_pos_st_id_set_.size() == LineWord.att_pos_en_id_set_.size());
    assert(LineWord.att_pos_st_id_set_.size() == LineWord.maxVec_.size());
    //refine the start_id
    if (LineWord.att_pos_st_id_set_.size() > 4) {
        for (int i = 1; i < LineWord.att_pos_st_id_set_.size() - 2; i++) {
            if (LineWord.att_pos_st_id_set_[i] > LineWord.att_pos_st_id_set_[i - 1] &&
                    LineWord.att_pos_st_id_set_[i + 2] > LineWord.att_pos_st_id_set_[i + 1] &&
                    LineWord.att_pos_st_id_set_[i] >= LineWord.att_pos_st_id_set_[i + 1]) {
                LineWord.att_pos_st_id_set_[i] = LineWord.att_pos_st_id_set_[i + 1] - 1;
            }
        }
    }
    //refine the end_id
    for (int i = 0; i < LineWord.att_pos_en_id_set_.size() - 1; i++) {
        if (LineWord.att_pos_st_id_set_[i+1] < 0) {
            continue;
        }
        LineWord.att_pos_en_id_set_[i] = std::min(LineWord.att_pos_en_id_set_[i],
            LineWord.att_pos_st_id_set_[i+1] - 1);
    }
    //if the next char is space and att weigtht > 0.01, then exlarge the word position
    for (int i = 0; i < LineWord.att_pos_en_id_set_.size() - 1; i++) {
        int exlarge_pos = LineWord.att_pos_st_id_set_[i + 1] - LineWord.att_pos_en_id_set_[i];
        int cur_att_pos_en_id = LineWord.att_pos_en_id_set_[i];
        bool is_exlarge = false;
        while (i + 1 < LineWord.att_pos_en_id_set_.size() && 
            exlarge_pos < 2 &&
            LineWord.maxVec_[i] != 94 &&
            LineWord.maxVec_[i + 1] == 94 && 
            cur_att_pos_en_id + exlarge_pos < LineWord.att_pos_en_id_set_[i + 1] &&
            LineWord.att_weights_[cur_att_pos_en_id + exlarge_pos + 
                i * LineWord.encoder_fea_num_] > 0.01) {
            exlarge_pos += 1;
            is_exlarge = true;
        }
        if (is_exlarge) {
            LineWord.att_pos_en_id_set_[i] = LineWord.att_pos_en_id_set_[i] + exlarge_pos - 1;
            //std::cout << "expand " << i << " 's en id " << cur_att_pos_en_id << " --> " << LineWord.att_pos_en_id_set_[i] << std::endl;
        }
    }
    //if the before char is space and att weigtht > 0.01, then exlarge the word position
    for (int i = 1; i < LineWord.att_pos_en_id_set_.size(); i++) {
        int exlarge_pos = LineWord.att_pos_st_id_set_[i] - LineWord.att_pos_en_id_set_[i - 1];
        int cur_att_pos_st_id = LineWord.att_pos_st_id_set_[i];
        bool is_exlarge = false;
        while (i - 1 < LineWord.att_pos_en_id_set_.size() && 
            exlarge_pos < 2 &&
            LineWord.maxVec_[i] != 94 &&
            LineWord.maxVec_[i - 1] == 94 && 
            cur_att_pos_st_id - exlarge_pos > LineWord.att_pos_st_id_set_[i - 1] &&
            LineWord.att_weights_[cur_att_pos_st_id - exlarge_pos + 
                i * LineWord.encoder_fea_num_] > 0.01) {
            exlarge_pos += 1;
            is_exlarge = true;
        }
        if (is_exlarge) {
            LineWord.att_pos_st_id_set_[i] = LineWord.att_pos_st_id_set_[i] - exlarge_pos + 1;
            //std::cout << "expand " << i << " 's st id " << cur_att_pos_st_id << " --> " << LineWord.att_pos_st_id_set_[i] << std::endl;
        }
    }

    //std::cout << LineWord.att_pos_st_id_set_.size() << " " << LineWord.att_pos_en_id_set_.size() << std::endl;

    //for the cand word
    for (int cand_id = 0; cand_id < LineWord.cand_word_vec.size(); cand_id++) {
        SSeqLEngWordInfor& cand_word = LineWord.cand_word_vec[cand_id];
        cand_word.encoder_fea_length_ = (float)(LineWord.wid_) / cand_word.encoder_fea_num_;
        cand_word.att_pos_st_id_set_.clear();
        cand_word.att_pos_en_id_set_.clear();
        for (int i = 0; i < cand_word.maxVec_.size(); i++) {
            int st_id = -1;
            int en_id = cand_word.maxVec_.size() + 1;
            float max_value = -1;
            for (int j = 0; j < cand_word.encoder_fea_num_; j++) {
                if (cand_word.att_weights_[j + i * cand_word.encoder_fea_num_] > max_value) {
                    max_value = cand_word.att_weights_[j + i * cand_word.encoder_fea_num_];
                    st_id = j;
                }
            }
            if (st_id >= 0) {
                for (en_id = st_id; en_id < cand_word.encoder_fea_num_; en_id++) {
                    if (cand_word.att_weights_[en_id + i * cand_word.encoder_fea_num_] < 0.001) {
                        break;
                    }
                }
                en_id -= 1;
                en_id = std::min(cand_word.encoder_fea_num_ - 1, en_id);
            }
            cand_word.att_pos_st_id_set_.push_back(st_id);
            cand_word.att_pos_en_id_set_.push_back(en_id);
        }
        //refine the start_id
        if (cand_word.att_pos_st_id_set_.size() > 4) {
            for (int i = 1; i < cand_word.att_pos_st_id_set_.size() - 2; i++) {
                if (cand_word.att_pos_st_id_set_[i] > cand_word.att_pos_st_id_set_[i - 1] &&
                       cand_word.att_pos_st_id_set_[i + 2] > cand_word.att_pos_st_id_set_[i + 1] &&
                       cand_word.att_pos_st_id_set_[i] >= cand_word.att_pos_st_id_set_[i + 1]) {
                    cand_word.att_pos_st_id_set_[i] = cand_word.att_pos_st_id_set_[i + 1] - 1;
                }
            }
        }
        //refine the end_id
        for (int i = 0; i < cand_word.att_pos_en_id_set_.size() - 1; i++) {
            if (cand_word.att_pos_st_id_set_[i+1] < 0) {
                continue;
            }
            cand_word.att_pos_en_id_set_[i] = std::min(cand_word.att_pos_en_id_set_[i],
                cand_word.att_pos_st_id_set_[i+1] - 1);
        }

        //if the next char is space and att weigtht > 0.01, then exlarge the word position
        for (int i = 0; i < cand_word.att_pos_en_id_set_.size() - 1; i++) {
            int exlarge_pos = cand_word.att_pos_st_id_set_[i + 1] - cand_word.att_pos_en_id_set_[i];
            int cur_att_pos_en_id = cand_word.att_pos_en_id_set_[i];
            bool is_exlarge = false;
            while (i + 1 < cand_word.att_pos_en_id_set_.size() && 
                exlarge_pos < 2 &&
                cand_word.maxVec_[i] != 94 &&
                cand_word.maxVec_[i + 1] == 94 && 
                cur_att_pos_en_id + exlarge_pos < cand_word.att_pos_en_id_set_[i + 1] &&
                cand_word.att_weights_[cur_att_pos_en_id + exlarge_pos + 
                    i * cand_word.encoder_fea_num_] > 0.01) {
                exlarge_pos += 1;
                is_exlarge = true;
            }
            if (is_exlarge) {
                cand_word.att_pos_en_id_set_[i] = cand_word.att_pos_en_id_set_[i] + exlarge_pos - 1;
                //std::cout << "expand " << i << " 's en id " << cur_att_pos_en_id << " --> " << cand_word.att_pos_en_id_set_[i] << std::endl;
            }
        }
        //if the before char is space and att weigtht > 0.01, then exlarge the word position
        for (int i = 1; i < cand_word.att_pos_en_id_set_.size(); i++) {
            int exlarge_pos = cand_word.att_pos_st_id_set_[i] - cand_word.att_pos_en_id_set_[i - 1];
            int cur_att_pos_st_id = cand_word.att_pos_st_id_set_[i];
            bool is_exlarge = false;
            while (i - 1 < cand_word.att_pos_en_id_set_.size() && 
                exlarge_pos < 2 &&
                cand_word.maxVec_[i] != 94 &&
                cand_word.maxVec_[i - 1] == 94 && 
                cur_att_pos_st_id - exlarge_pos > cand_word.att_pos_st_id_set_[i - 1] &&
                cand_word.att_weights_[cur_att_pos_st_id - exlarge_pos + 
                    i * cand_word.encoder_fea_num_] > 0.01) {
                exlarge_pos += 1;
                is_exlarge = true;
            }
            if (is_exlarge) {
                cand_word.att_pos_st_id_set_[i] = cand_word.att_pos_st_id_set_[i] - exlarge_pos + 1;
                //std::cout << "expand " << i << " 's st id " << cur_att_pos_st_id << " --> " << cand_word.att_pos_st_id_set_[i] << std::endl;
            }
        }

        assert(cand_word.att_pos_st_id_set_.size() == cand_word.att_pos_en_id_set_.size());
        assert(cand_word.att_pos_st_id_set_.size() == cand_word.maxVec_.size());
    }

    return 0;
}

int CSeqLEngWordReg::recognize_whole_line_att(const IplImage *orgImage,\
        SSeqLEngLineRect &lineRect, SSeqLEngLineInfor &outLineInfor,\
        SSeqLEngSpaceConf &spaceConf) {
    if (lineRect.rectVec_.size() < 1) {
        return -1;
    }
    if (orgImage->width < SEQL_MIN_IMAGE_WIDTH || orgImage->height < SEQL_MIN_IMAGE_HEIGHT \
            || orgImage->width > SEQL_MAX_IMAGE_WIDTH || orgImage->height > SEQL_MAX_IMAGE_HEIGHT) {
        return -1;
    }


    IplImage * image = cvCreateImage(cvSize(orgImage->width, orgImage->height), \
            orgImage->depth, orgImage->nChannels);
    cvCopy(orgImage, image, NULL);

    outLineInfor.wordVec_.clear();
    outLineInfor.left_ = image->width;
    outLineInfor.wid_  = 0;
    outLineInfor.top_  = image->height;
    outLineInfor.hei_  = 0;
    outLineInfor.imgWidth_ = image->width;
    outLineInfor.imgHeight_ = image->height;

    SSeqLEngRect tempLineRect;
    tempLineRect.left_ = 0;
    tempLineRect.top_ = 0;
    tempLineRect.wid_ = image->width;
    tempLineRect.hei_ = image->height;

    SSeqLEngWordInfor tempLineWord;
    onceRectSeqLPredict(tempLineRect, lineRect, image, tempLineWord, 0, 1, false);
    std::cout << "label dim: " << labelsDim_ << std::endl;
    std::cout << "blank: " << blank_ << std::endl;
    
    //empty res
    if (tempLineWord.wordStr_.length() <= 0) {
        cvReleaseImage(&image);
        image = NULL;
        return 0;
    }

    std::vector<SSeqLEngWordInfor>::iterator itaa = tempLineWord.cand_word_vec.begin();
    while (itaa != tempLineWord.cand_word_vec.end()) {
        if ((itaa->wordStr_.length()) == 0) {
            itaa = tempLineWord.cand_word_vec.erase(itaa);
        } else {
            itaa++;
        }
    }

    bool all_high_type = true;
    for (int id = 0; id < tempLineWord.maxVec_.size(); id++) {
        if (tempLineWord.maxVec_[id] != labelsDim_ -3 &&
            (tempLineWord.maxVec_[id] < 32 ||
             tempLineWord.maxVec_[id] > 57)) {
            all_high_type = false;
            break;
        }
    }
    float line_overlap_area_ratio = (float)tempLineRect.wid_ * tempLineRect.hei_ / (image->width * image->height);

    if ((tempLineWord.wordStr_.length()) > 0) {
        spaceConf.avgCharWidth_ = tempLineRect.wid_/(tempLineWord.wordStr_.length());
    } else {
        spaceConf.avgCharWidth_ = tempLineRect.wid_;
    }

    //get the st/en att weight fea id
    extract_attention_posistion(tempLineWord);
    extract_words(tempLineWord, outLineInfor);
    //extract_words_att_cand(tempLineWord, outLineInfor);

    //main line
    if (line_overlap_area_ratio > 0.5 || 
        (line_overlap_area_ratio > 0.2 && all_high_type)) {
        for (int id = 0; id < outLineInfor.wordVec_.size(); id++) {
            outLineInfor.wordVec_[id].main_line_ = true;
            //std::cout << "main line word: " << outLineInfor.wordVec_[id].wordStr_ << std::endl;
        }
    }

    // add acc score according to att cand
    extend_att_word(outLineInfor);

    add_att_cand_score(tempLineWord, outLineInfor);
    extract_words_att_space_cand(tempLineWord, outLineInfor);
    extract_words_att_del_lowprob_cand(tempLineWord, outLineInfor);
   
    outLineInfor.left_ = 0;
    outLineInfor.top_  = 0;
    outLineInfor.wid_ = image->width;
    outLineInfor.hei_ = image->height;

    split_word_by_punct(outLineInfor);
   
    std::vector<SSeqLEngWordInfor>::iterator ita = outLineInfor.wordVec_.begin();
    while (ita != outLineInfor.wordVec_.end()) {
        if ((ita->wordStr_.length()) == 0 || ita->toBeDelete_ || ita->regLogProb_ < -3.5 ||
                (ita->regLogProb_ < -2.5 && (ita->wordStr_.length()) < 3)) {
            ita = outLineInfor.wordVec_.erase(ita);
        } else {
            ita++;
        }
    }

    /*for (int id = 0; id < outLineInfor.wordVec_.size(); id++) {
        std::cout << "word info "
                << outLineInfor.wordVec_[id].left_ << " "
                << outLineInfor.wordVec_[id].top_ << " " 
                << outLineInfor.wordVec_[id].wid_ << " " 
                << outLineInfor.wordVec_[id].hei_ << " " 
                << outLineInfor.wordVec_[id].regLogProb_ << " " 
                << outLineInfor.wordVec_[id].wordStr_.size() << " " 
                << outLineInfor.wordVec_[id].path_pos_.size() << " " 
                << outLineInfor.wordVec_[id].acc_score_ << " "
                << outLineInfor.wordVec_[id].highLowType_ << " "
                << outLineInfor.wordVec_[id].wordStr_ << " " 
                << outLineInfor.wordVec_[id].encoder_fea_num_ << " "
                << outLineInfor.wordVec_[id].encoder_fea_length_ << " "
                << outLineInfor.wordVec_[id].att_pos_st_id_set_.size() << " "
                << outLineInfor.wordVec_[id].att_pos_en_id_set_.size() << " " << std::endl; 
    }*/

    sort(outLineInfor.wordVec_.begin(), outLineInfor.wordVec_.end(), leftSeqLWordRectFirst);
    cvReleaseImage(&image);
    image = NULL;

    return 0;
}

