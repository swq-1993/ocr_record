/***************************************************************************
 * 
 * Copyright (c) 2017 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file seql_asia_att_recog.cpp
 * @author sunyipeng(com@baidu.com)
 * @date 2017/11/17 14:44:28
 * @brief 
 *  
 **/

#include "seql_asia_att_recog.h"
#include "SeqLEngWordRecog.h"
#include "predictor/seql_predictor.h"
#include "iconv.h"
#include "common_func.h"
#include "dll_main.h"

void CSeqLAsiaAttReg::extract_data_from_rect(IplImage *image, SSeqLEngRect &tempRect, 
        float * &data, int &width, int &height, int &channel,
        bool isBlackWdFlag, float horResizeRate)
{
    assert(tempRect.left_ >= 0 && tempRect.top_ >= 0);
    assert((tempRect.left_ + tempRect.wid_) < image->width);
    assert((tempRect.top_ + tempRect.hei_) < image->height);
    
    IplImage *sub_image = cvCreateImage(cvSize(tempRect.wid_, tempRect.hei_),
            image->depth, image->nChannels);
    cvSetImageROI(image, cvRect(tempRect.left_, tempRect.top_, tempRect.wid_, 
            tempRect.hei_));
    cvCopy(image, sub_image);
    cvResetImageROI(image);
    IplImage* gray_image = cvCreateImage(cvSize(sub_image->width, sub_image->height),
            sub_image->depth, 1);
    if (sub_image->nChannels != 1){
        cvCvtColor(sub_image, gray_image, CV_BGR2GRAY);
    }
    else{
        cvCopy(sub_image, gray_image);
    }
    
    resize_image_to_fix_height(gray_image, seqLNormheight_, horResizeRate);  
    _m_norm_img_h = gray_image->height;
    _m_norm_img_w = gray_image->width;
    extend_slim_image(gray_image, 1.0);
    assert(gray_image->height == fixHeight);

    data = (float *)malloc(gray_image->width*gray_image->height*
            gray_image->nChannels*sizeof(float));
    int index = 0;
    if (isBlackWdFlag == 1){
        for (unsigned int h = 0; h < gray_image->height; h++){
            for (unsigned int w = 0; w < gray_image->width; w++){
                for (unsigned int c = 0; c < gray_image->nChannels; c++){
                    float value = cvGet2D(gray_image, h, w).val[c];
                    data[index] = value - seqLCdnnMean_;
                    index++;
                }
            }
        }
    }
    else {
        for (unsigned int h = 0; h < gray_image->height; h++){
            for (unsigned int w = 0; w < gray_image->width; w++){
                for (unsigned int c = 0; c < gray_image->nChannels; c++){
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
}

// extract words from predicted results
int CSeqLAsiaAttReg::extract_words(SSeqLEngWordInfor recog_res, SSeqLEngLineInfor & out_line)    
{
    int start_time = 0;
    int next_start_time = 0;
    int end_time = 0;
    int time_num = recog_res.maxStr_.size();
    std::vector<int> max_vec = recog_res.maxVec_;
    
    /*
    std::cout << "time_num = " << time_num << std::endl;
    for (size_t i = 0; i < max_vec.size(); i++) {
        std::cout << "index " << i << ": "<< max_vec.at(i) << std::endl;
    }
    */ 
    out_line.wordVec_.push_back(recog_res);
    /*
    while (1) {
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

        // something strange could happen
        if (end_time < start_time || end_time >= time_num) break;

        SSeqLEngWordInfor new_word = split_word(recog_res, start_time, end_time, next_start_time, true);
        out_line.wordVec_.push_back(new_word);
        // go to next word
        start_time = next_start_time;
        end_time = start_time;

        if (start_time >= time_num - 1) break;
    }
    */
    return 0;
}

SSeqLEngWordInfor CSeqLAsiaAttReg::split_word(SSeqLEngWordInfor word,
        int start, int end, int next_start, bool need_adjust) {

    //std::vector<SSeqLEngWordInfor> new_word_vec;
    SSeqLEngWordInfor new_word;
    new_word.top_ = word.top_;
    new_word.hei_ = word.hei_;
    int time_num = word.maxVec_.size();
    int end_with_blank = std::max(end, next_start - 1);
    int len = end_with_blank - start + 1;
    std::vector<int> this_max_vec;
    this_max_vec.resize(len);
    memcpy(&(this_max_vec[0]), &(word.maxVec_[start]), sizeof(int) * len);
 
    //need to be adjust
    new_word.left_ = word.left_ + word.wid_ * start / time_num;
    int real_end_time = next_start > end + 4 ? end + 4 : next_start;
    new_word.wid_  = word.left_ + word.wid_ * real_end_time / time_num - new_word.left_;
    new_word.mulRegScore_ = 0;
    new_word.toBeAdjustCandi_ = need_adjust;
    new_word.maxVec_ = this_max_vec;
    new_word.toBeDelete_ = false;
    new_word.maxStr_ = word.maxStr_.substr(start, end - start + 1);
    new_word.highLowType_ = 0;
    new_word.word_flag_log_ = word.word_flag_log_;
    //std::cout << "word_flag_log_: " << word.word_flag_log_ << std::endl;

    //std::cout << "split word: " << new_word.maxStr_ << std::endl;
    if (_m_attention_decode) { // no path elimination
        std::vector<int> path_pos;
        std::vector<int> path = this_max_vec;
        std::vector<float> this_prob_mat;

        this_prob_mat.resize((len + 1) * labelsDim_);
        memcpy(&(this_prob_mat[0]), &(word.proMat_[start * labelsDim_]),
            sizeof(int) * (len + 1) * labelsDim_);
        new_word.proMat_ = this_prob_mat;
        new_word.regLogProb_ = 0.0;
        for (int i = 0; i < path.size(); i++) {
            path_pos.push_back(i);
            new_word.regLogProb_ += safeLog(new_word.proMat_[i * labelsDim_ + (path[i] + 2)]);
        }
        new_word.wordStr_ = path_to_string(path);
        //std::cout << "wordStr_: " << new_word.wordStr_ << std::endl;
        new_word.path_pos_.clear();
        new_word.path_pos_ = path_pos;
        // add the split punc prob
        if (start > 0) {
            new_word.word_flag_log_ += safeLog(word.proMat_[(start - 1) * labelsDim_ 
                + (word.maxVec_[start - 1] + 2)]);
        }
        if (end_with_blank < time_num - 1) {
            new_word.word_flag_log_ += safeLog(word.proMat_[(end_with_blank+1) * labelsDim_ 
                + (word.maxVec_[end_with_blank+1] + 2)]);
        }
        new_word.regLogProb_ += new_word.word_flag_log_;
        /* string convertion
        new_word.highLowType_ = gen_highlow_type(new_word);
        */
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

int CSeqLAsiaAttReg::extract_word_infor_att(float * matrix, int* output_labels, 
                int labelsDim, int len, SSeqLEngWordInfor & outWord){

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
            outWord.maxStr_ = path_to_string(max_vec);
            outWord.regLogProb_ = (double)logprob; 
            outWord.wordStr_ = path_to_string(max_vec);
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
            cand_word.maxStr_ = path_to_string(max_vec);
            cand_word.regLogProb_ = (double)logprob;
            cand_word.wordStr_ = path_to_string(max_vec);
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
    return 0;
}

// predict lines results using paddle
int CSeqLAsiaAttReg::predict_lines(IplImage * image, 
        SSeqLEngRect &inTempRect,
        SSeqLEngWordInfor & outTempInfor,
        float extendRate, float horResizeRate, bool loadFlag) {

    float* data = NULL;
    int width = 0;
    int height = 0;
    int channel = 0;
    SSeqLEngRect ext_temp_rect = inTempRect;
    int ext_boudary_w = extendRate*inTempRect.hei_;
    int ext_boudary_h = -0.00*inTempRect.hei_;
    ext_temp_rect.left_ = std::max(0, inTempRect.left_  - ext_boudary_w);
    ext_temp_rect.top_  = std::max(0, inTempRect.top_ - int(1.0*ext_boudary_h));
    ext_temp_rect.wid_  =
        std::min(inTempRect.left_ + inTempRect.wid_ + ext_boudary_w - 1, image->width-1)
        - ext_temp_rect.left_ + 1;
    ext_temp_rect.hei_  =
        std::min(inTempRect.top_ + inTempRect.hei_ + int(1.0*ext_boudary_h) -1, image->height - 1)
      - ext_temp_rect.top_ + 1;

    extract_data_from_rect(image, ext_temp_rect, data, width, height, channel,
                       1, horResizeRate);

    std::vector<float *> data_v;
    std::vector<float *> output_v;
    std::vector<int*> output_labels;
    std::vector<int> img_width_v;
    std::vector<int> img_height_v;
    std::vector<int> img_channel_v;
    std::vector<int> out_len_v;
    data_v.push_back(data);
    img_width_v.push_back(width);
    img_height_v.push_back(height);
    img_channel_v.push_back(channel);

    //printInputData(data_v[0], img_width_v[0]*img_height_v[0]*img_channel_v[0]);
#ifdef COMPUT_TIME
    clock_t loct1
    clock_t loct2;
    loct1 = clock();
#endif

    extern int g_predict_num;
    if (loadFlag) { 
        float * matrix = NULL;
        int matrix_len = 0;
        load_predict_matrix(g_predict_num, matrix, matrix_len);
        output_v.push_back(matrix);
        out_len_v.push_back(matrix_len);
    } else {
        if (_m_attention_decode) {
            int beam_size = 0;
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
                           /* use_paddle */ _use_paddle,
                           /* att_modle */ true);
        }
        else {
            // assert _m_attention_decode == true 
            return -1; 
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
        extract_word_infor_att(output_v[0], output_labels[0], labelsDim_,
            out_len_v[0], outTempInfor);
    }
    else {
        // assert _m_attention_decode == true 
        return -1; 
        //extract_word_infor(output_v[0], labelsDim_, out_len_v[0], 
        //    outTempInfor);
    }
    outTempInfor.left_ = inTempRect.left_;
    outTempInfor.top_  = inTempRect.top_;
    outTempInfor.wid_  = inTempRect.wid_;
    outTempInfor.hei_  = inTempRect.hei_;
    outTempInfor.dectScore_  = inTempRect.score_;
    outTempInfor.mulRegScore_ = 0;
    outTempInfor.compNum_ = inTempRect.compNum_;
    outTempInfor.leftSpace_ = inTempRect.leftSpace_;
    outTempInfor.rightSpace_ = inTempRect.rightSpace_;
    outTempInfor.sigWordFlag_ = inTempRect.sigWordFlag_;
    outTempInfor.isPunc_ = false;
    outTempInfor.toBeAdjustCandi_ = true;

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
 
// recognize lines using spatial attention
int CSeqLAsiaAttReg::recognize_lines(const IplImage *orgImage,
        SSeqLEngLineInfor &outLineInfor,
        SSeqLEngSpaceConf &spaceConf)
{
    if (orgImage->width < SEQL_MIN_IMAGE_WIDTH
            || orgImage->height < SEQL_MIN_IMAGE_HEIGHT
            || orgImage->width > SEQL_MAX_IMAGE_WIDTH
            || orgImage->height > SEQL_MAX_IMAGE_HEIGHT) {
        return -1;
    }

    IplImage * image = cvCreateImage(cvSize(orgImage->width, orgImage->height),
            orgImage->depth, orgImage->nChannels);
    
    cvCopy(orgImage, image, NULL);

    outLineInfor.wordVec_.clear();
    outLineInfor.left_ = image->width;
    outLineInfor.wid_  = 0;
    outLineInfor.top_  = image->height;
    outLineInfor.hei_  = 0;
    outLineInfor.imgWidth_ = image->width;
    outLineInfor.imgHeight_ = image->height;

    SSeqLEngRect temp_line_rect;
    temp_line_rect.left_ = 0;
    temp_line_rect.top_ = 0;
    temp_line_rect.wid_ = image->width;
    temp_line_rect.hei_ = image->height;

    SSeqLEngWordInfor temp_line_word;
    predict_lines(image, temp_line_rect, temp_line_word, 0, 1, false);
/*
    for (size_t i = 0; i < temp_line_word.maxVec_.size(); i++) {
        int max_id = temp_line_word.maxVec_.at(i);
        std::cout << i << ": " << max_id << std::endl;
        std::cout << this->tabelLowStrVec_[max_id] << std::endl; 
        //std::cout << i << ": " << temp_line_word.charVec_.at(i).charStr_ << std::endl;
        //std::cout << i << ": " << temp_line_word.adStrVec_.at(i) << std::endl;
    }

    std::cout << "out_str: " << temp_line_word.wordStr_ << std::endl; 
*/

/* 
    bool all_high_type = true;
    for (int id = 0; id < temp_line_word.maxVec_.size(); id++) {
        if (temp_line_word.maxVec_[id] != labelsDim_ -3 &&
            (temp_line_word.maxVec_[id] < 32 || temp_line_word.maxVec_[id] > 57)) {
            all_high_type = false;
            break;
        }
    }
    float line_overlap_area_ratio = (float)temp_line_rect.wid_ * temp_line_rect.hei_ /
            (image->width * image->height);
    if ((temp_line_word.wordStr_.length()) > 0) {
        spaceConf.avgCharWidth_ = temp_line_rect.wid_ / (temp_line_word.wordStr_.length());
    } else {
        spaceConf.avgCharWidth_ = temp_line_rect.wid_;
    }
*/
    extract_words(temp_line_word, outLineInfor);
/*
    for (int id = 0; id < outLineInfor.wordVec_.size(); id++) {
        std::cout << "word: " << outLineInfor.wordVec_[id].wordStr_ << std::endl;
    }
*/

/*
    for (int id = 0; id < outLineInfor.wordVec_.size(); id++) {
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
                << outLineInfor.wordVec_[id].wordStr_ << std::endl; 
    }
*/
    cvReleaseImage(&image);
    image = NULL;

    return 0;
}

int CSeqLAsiaAttRegModel::seql_cdnn_model_table_init(char *modelPath, char *tablePath,
        float mean, int height, void *& cdnnModel, unsigned short *&cdnnTable, 
        float &cdnnMean, int &cdnnNormHeight)
{
    /* Initialize the recognizing model and input, output dims.
     **/
    
    //std::cout << "paddle init. start." << std::endl;
    inference_seq::init(/* model_path */ modelPath,
                    /* model */cdnnModel,
                    /* use_gpu */_use_gpu,
                    /* use_paddle */ _use_paddle);
    dataDim_ = inference_seq::get_input_dim(cdnnModel, _use_paddle);
    labelsDim_ = inference_seq::get_output_dim(cdnnModel, _use_paddle);
    
    //std::cout << "paddle init. success." << std::endl;

    cdnnTable = (unsigned short *)malloc(labelsDim_*sizeof(float));
    cdnnMean = mean;
    cdnnNormHeight = height;

    FILE * fpTable = NULL;
    if (NULL == (fpTable = fopen(tablePath, "r"))){
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
        if (code >= 0xFEE0 && code < 0xFEE0 + 0x80){
            code = code - 0xFEE0;
        }
        cdnnTable[index] = (unsigned short)code;
        
        char gbkStr[4];
        SeqLUnicode2GBK(code, gbkStr);
        
        if (code == SPACECODE) {
            strcpy(gbkStr, " ");
        }

        tabelStrVec_[i] = gbkStr;
        
        for (unsigned int j = 0; j < strlen(gbkStr); j++){
            if (gbkStr[j] >= 'A' && gbkStr[j] <= 'Z'){
                gbkStr[j] = gbkStr[j] - 'A' + 'a';
            }
        }
        tabelLowStrVec_[i] = gbkStr;

    }
    fclose(fpTable);
    blank_ = labelsDim_ - 1;
}

int CSeqLAsiaAttRegModel::seql_cdnn_model_table_init_utf8(char *modelPath, char *tablePath,
        float mean, int height, void *& cdnnModel, unsigned short *&cdnnTable, 
        float &cdnnMean, int &cdnnNormHeight, char * langType){
    /* Initialize the recognizing model and input, output dims.
     * Set the default initialize parameter.
     **/
    inference_seq::init(/* modelPath */ modelPath,
                    /* model */ cdnnModel,
                    /* use_gpu */ _use_gpu,
                    /* ue_paddle */ _use_paddle);
    dataDim_ = inference_seq::get_input_dim(cdnnModel, _use_paddle);
    labelsDim_ = inference_seq::get_output_dim(cdnnModel, _use_paddle);

    cdnnTable = (unsigned short *)malloc(labelsDim_*sizeof(float));
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
        if (code >= 0xFEE0 && code < 0xFEE0 + 0x80){
            code = code - 0xFEE0;
        }
        cdnnTable[index] = (unsigned short)code;
      
        char utf8Str[6];
        SeqLUnicode2UTF8(code, utf8Str);
        if (code == SPACECODE) {
            strcpy(utf8Str, " ");  
        }
        tabelStrVec_[i] = utf8Str;
       
        if (true || strcmp(langType, "Euro")) {
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

int CSeqLAsiaAttRegModel::seql_cdnn_model_table_release(void **cdnnModel, 
            unsigned short **cdnnTable)
{
    if (cdnnTable != NULL && *cdnnTable != NULL) {
        free(*cdnnTable);
        *cdnnTable = NULL;
    }

    /* Relase the model-related memory.
     **/
    return inference_seq::release(cdnnModel, _use_paddle);
}

void CSeqLAsiaAttReg::resize_image_to_fix_height(IplImage * & image,
            const int resizeHeight, float horResizeRate)
{
    if (resizeHeight > 0){
        CvSize sz;
        sz.height = resizeHeight;
        sz.width = image->width*resizeHeight*1.0 / image->height*horResizeRate;
        IplImage *dst_img = cvCreateImage(sz, image->depth, image->nChannels);
        cvResize(image, dst_img, CV_INTER_CUBIC);
        cvReleaseImage(&image);
        image = dst_img;
        dst_img = NULL;
    }
}

void CSeqLAsiaAttReg::extend_slim_image(IplImage * & image, float widthRate)
{
    int goal_width = widthRate*image->height;
    if (image->width < goal_width){
        IplImage *dst_img = cvCreateImage(cvSize(goal_width, image->height), 
            image->depth, image->nChannels);
        int shift_project = (dst_img->width - image->width) / 2;
        for (int h = 0; h < dst_img->height; h++){
            for (int w = 0; w < dst_img->width; w++){
                for (int c = 0; c < dst_img->nChannels; c++){
                    int project_w = 0;
                    if (w < shift_project) {
                        project_w = 0;
                    }
                    else if (w >= (image->width + shift_project)) {
                        project_w = image->width - 1;
                    }
                    else{
                        project_w = w - shift_project;
                    }
                    CV_IMAGE_ELEM(dst_img, uchar, h, dst_img->nChannels*w + c) = 
                    CV_IMAGE_ELEM(image, uchar, h, image->nChannels*project_w + c);
                }
            }
        }
        cvReleaseImage(&image);
        image = dst_img;
        dst_img = NULL;
    }
}

std::string CSeqLAsiaAttReg::path_to_string(std::vector<int>& path)
{
    std::string str = "";
    for (unsigned int i = 0; i < path.size(); i++){
        str += tabelLowStrVec_[path[i]]; 
        //str += tabelStrVec_[path[i]]; 
    }
    return str;
}

void load_predict_matrix(int predict_num, float* & matrix, int & len)
{
    char loadName[256];
    sprintf(loadName, "./loadPredictDir/%d.txt", predict_num);
    //std::cout<<" loaded..." << loadName << std::endl;
    FILE *fd = fopen(loadName, "rb");
    if (fd == NULL){
        std::cerr << " Can not open " << loadName << std::endl;
    }
   
    fread(&len, sizeof(int), 1, fd);
    matrix = new float[len];
    fread(matrix, sizeof(float), len, fd);

    fclose(fd);
    return;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
