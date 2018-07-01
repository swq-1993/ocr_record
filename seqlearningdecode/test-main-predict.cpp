/**
 * test-main-predict.cpp
 * Author: hanjunyu(hanjunyu@baidu.com)
 * Create on: 2014-07-01
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 *  
 **/

#include "predictor/seql_predictor.h"
#include <cv.h>
#include <highgui.h>

void * G_seqLcdnnModel;
unsigned short * G_seqLcdnnTable;
float G_seqLcdnnMean;
int G_seqLheight;

int SeqLCDNNModelTableInit(char *modelPath, char *tablePath, float mean,
        void *& cdnnModel, unsigned short *&cdnnTable, float &cdnnMean){
    /* Initialize the recognizing model and input, output dims.
     **/
    inference::init(/* model_path */ modelPath,
                    /* model */ cdnnModel,
                    /* use_gpu */ false);
    int data_dim = inference::get_input_dim(cdnnModel);
    int labels_dim = inference::get_output_dim(cdnnModel);

    cdnnTable = (unsigned short *) malloc(labels_dim * sizeof(float));
    cdnnMean = mean;

    FILE * fpTable = NULL;
    if (NULL == (fpTable = fopen(tablePath, "r"))) {
        fprintf(stderr, "open table file error. \n");
        return -1;
    }
    for (int i = 0; i < labels_dim; i++)
    {
        int code;
        int index;
        fscanf(fpTable, "%d", &code);
        fscanf(fpTable, "%d", &index);
        cdnnTable[index] = (unsigned short)code;
    }
    fclose(fpTable);
}

int SeqLCDNNModelTableRelease(void **cdnnModel, unsigned short **cdnnTable) {
    if (cdnnTable != NULL && *cdnnTable != NULL) {
        free(*cdnnTable);
        *cdnnTable = NULL;
    }

    /* Release the model-related memory.
     **/
    return inference::release(cdnnModel);
}

void ResizeImageToFixHeight(IplImage * & image, const int resizeHeight){
    if (resizeHeight > 0) {
        CvSize sz;
        sz.height = resizeHeight;
        sz.width = image->width * resizeHeight * 1.0 / image->height;
        IplImage *des_img = cvCreateImage(sz, image->depth, image->nChannels);
        cvResize(image, des_img, CV_INTER_CUBIC);
        cvReleaseImage(&image);
        image = des_img;
        des_img = NULL;
    }
}

void ReadImageData(char * imageName, float mean,int fixHeight,  float* & data, int& width, int& height, int& channel){
    IplImage * image = NULL;
    IplImage * gray_image = NULL;
    if ((image = cvLoadImage(imageName, CV_LOAD_IMAGE_COLOR)) != 0) {
        gray_image = cvCreateImage(cvSize(image->width, image->height), image->depth, 1);
        cvCvtColor(image, gray_image, CV_BGR2GRAY);
        ResizeImageToFixHeight(gray_image, fixHeight);  
        assert(gray_image->height == fixHeight);
        data = (float *) malloc(
                gray_image->width*gray_image->height*gray_image->nChannels*sizeof(float));
        int index = 0;
        for (int h = 0; h < gray_image->height; h++) {
            for (int w = 0; w < gray_image->width; w++) {
                for (int c = 0; c < gray_image->nChannels; c++) {
                    float value = cvGet2D(gray_image, h, w).val[c];
                    data[index] = value - mean;
                    index++;
                }
            }
        }
        width = gray_image->width;
        height = gray_image->height;
        channel = gray_image->nChannels;

        cvReleaseImage(&image);
        cvReleaseImage(&gray_image);
    } else {
        fprintf(stderr, "Load Image %s Failed.\n", imageName);
    }
}

int argMaxActs(float* acts, int numClasses) {
    int  max_idx = 0;
    float max_val = acts[max_idx];
    for (int i = 0; i < numClasses; ++i) {
        if (max_val < acts[i]) {
            max_val = acts[i];
            max_idx = i;
        }
    }
    return max_idx;
}

void printOneOutputMatrix(float * mat, int LabelDim, int len){
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

void SeqLSimulateData(float * & data, int& width, int & height, int & channel, int &data_num ){
    data_num = 1;
    width = 480;
    height = 48;
    channel = 1;
    int data_dim = width * height;
    data = (float *)malloc(data_num * data_dim * sizeof(float));
    for (int i = 0; i < data_num*data_dim; i++) {
        //fread(data+i*data_dim, sizeof(float), data_dim, fR);
        data[i] = i % 255;
    }
}


int main(){
    SeqLCDNNModelTableInit("./data/assist_data/seqL.model.bin",
                           "./data/assist_data/table_52", 112.5,
                           G_seqLcdnnModel, G_seqLcdnnTable, G_seqLcdnnMean);
    G_seqLheight = 48;

    float *data;
    int width, height, channel, data_num;
    
    //ReadImageData("test.jpg",G_seqLcdnnMean, G_seqLheight, data, width, height, channel);
    
    SeqLSimulateData(data, width, height, channel, data_num );
  
   /* data_num = 1;
    width = 480;
    height = 48;
    channel = 1;
    int data_dim = width * height;
    data = (float *)malloc(data_num * data_dim * sizeof(float));
    for (int i = 0; i < data_num*data_dim; i++) {
        //fread(data+i*data_dim, sizeof(float), data_dim, fR);
        data[i] = i % 255;
    }
*/

    printf("width %d, height %d, channel %d \n", width, height, channel); 
    for (int i = 0; i < width*height*channel; i++) {
        printf("(%d,%f)\t",i,data[i]);
    }
    printf("\n");

    vector<float *> dataV;
    vector<float *> outputV;
    vector<int>  imgWidthV;
    vector<int>  imgHeightV;
    vector<int>  imgChannelV;
    vector<int>  outLenV;
    dataV.push_back(data);
    imgWidthV.push_back(width);
    imgHeightV.push_back(height);
    imgChannelV.push_back(channel);

    int beam_size = 0;
    std::vector<int*> output_labels;
    inference::predict(/* model */ seqLCdnnModel_,
                       /* batch_size */ 1,
                       /* inputs */ dataV,
                       /* widths */ imgWidthV,
                       /* heights */ imgHeightV,
                       /* channels */ imgChannelV,
                       /* beam_size */ beam_size,
                       /* output_probs */ outputV,
                       /* output_labels */ output_labels,
                       /* output_lengths */ outLenV);

    printf("dataV.size() %d outputV size  %d, outLenV size %d \n",dataV.size(), outputV.size(), outLenV.size()); 
 
    printOneOutputMatrix(outputV[0], cdnnGetLabelsDim(G_seqLcdnnModel), outLenV[0]);

    for (int i = 0; i < outputV.size(); i++) {
        free(outputV[i]);
        outputV[i] = NULL;
    }
    for (int i=0; i< dataV.size(); i++){
        free(dataV[i]);
        dataV[i] = NULL;
    }
    
    SeqLCDNNModelTableRelease(& G_seqLcdnnModel, & G_seqLcdnnTable);

    return 0;
}
