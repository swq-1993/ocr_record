/***************************************************************************
 * 
 * Copyright (c) 2016 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
/**
 * @file seq_ocr_chn_batch.cpp
 * @author xieshufu(com@baidu.com)
 * @date 2016/12/23 13:28:08
 * @brief 
 *  
 **/
#include "seq_ocr_chn_batch.h"
#include "common_func.h"
#include "SeqLEngLineBMSSWordDecoder.h"
#include "seql_asia_line_bmss_word_decoder.h"
#include "seql_asia_word_recog.h"
#include "seql_asia_word_seg_processor.h"
#include "reform_en_rst.h"
#include "SeqLEngWordSegProcessor.h"
#include <omp.h>

extern char g_pimage_title[256];
namespace nsseqocrchn
{
/*sunwanqi add 0503 remove the chn_model,only through eng model*/
int CSeqModelChBatch::recognize_image_batch_only_eng(const std::vector<st_batch_img> vec_image,\
    int model_type,
    bool fast_flag,
    std::vector<SSeqLEngLineInfor> &out_line_vec) {

    int ret =0;
    if (vec_image.size() <= 0 || 
            (model_type != 0 && model_type != 1 && model_type != 2)) {
          ret = -1;
          return ret;
      }
    out_line_vec.resize(vec_image.size());
    recognize_line_eng_batch_whole(vec_image, _m_recog_model_eng,\
              _m_table_flag, out_line_vec);

    int out_line_vec_size = out_line_vec.size();
      for (unsigned int i = 0; i < out_line_vec_size; i++) {
          std::string str_line_rst = "";
          for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
              if (out_line_vec[i].wordVec_[j].left_add_space_) {
                  str_line_rst += out_line_vec[i].wordVec_[j].wordStr_ + " ";
              } else {
                  str_line_rst += out_line_vec[i].wordVec_[j].wordStr_;
              }
          }
          out_line_vec[i].lineStr_ = str_line_rst;
      }

      return ret;
}
int CSeqModelChBatch::recognize_image_batch(const std::vector<st_batch_img> vec_image,\
    int model_type,
    bool fast_flag,
    std::vector<SSeqLEngLineInfor> &out_line_vec) {
   
    int ret = 0;
    if (vec_image.size() <= 0 || 
          (model_type != 0 && model_type != 1 && model_type != 2)) {
        ret = -1;
        return ret;
    }

    recognize_line_chn(vec_image, \
        _m_recog_model_row_chn, _m_recog_model_col_chn, out_line_vec); 
        
    for(unsigned int i = 0; i < out_line_vec.size(); ++i){
        std::cout<<"after_chnModel_out_line_vec_chn"<<i<<":"<<out_line_vec[i].lineStr_<<std::endl;        
    }
    // call second english recognition
    if (model_type == 0 || model_type == 2) {
        recognize_line_eng_batch(vec_image, _m_recog_model_eng,\
            _m_table_flag, out_line_vec);
    }
    for(unsigned int i = 0; i < out_line_vec.size(); ++i){
        std::cout<<"after_engModel_out_line_vec_chn"<<i<<":"<<out_line_vec[i].lineStr_<<std::endl;
    }
    // merge the charcters which are overlapped
    // the small english model is called here
    int out_line_vec_size = out_line_vec.size();
    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        std::string str_line_rst = "";
        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            if (out_line_vec[i].wordVec_[j].left_add_space_) {
                str_line_rst += out_line_vec[i].wordVec_[j].wordStr_ + " ";
            } else {
                str_line_rst += out_line_vec[i].wordVec_[j].wordStr_;
            }
        }
        out_line_vec[i].lineStr_ = str_line_rst;
    }
    return ret;
}

// sunwanqi add the recognize the row with whole english OCR engine
void CSeqModelChBatch::recognize_line_eng_batch_whole(const std::vector<st_batch_img> &vec_imgs,
     CSeqLEngWordReg *recog_processor_en,\
    int *ptable_flag, std::vector<SSeqLEngLineInfor> &out_line_vec){
    SSeqLEngLineInfor out_line;
    for(unsigned int i = 0; i<vec_imgs.size(); ++i){
        recognize_row_whole_eng_batch_v3(vec_imgs[i].img,
                 recog_processor_en, true, out_line);
        out_line_vec[i] = out_line;
        /*std::cout<<"checkout_line:";
        for(int i=0;i<out_line.wordVec_.size();i++)
        {
            std::cout<<out_line.wordVec_[i].wordStr_<<" ";
        }
        std::cout<<std::endl;*/
    }
    //std::cout<<"_m_resnet_strategy_flag = 1:"<<_m_resnet_strategy_flag<<std::endl;
    if (_m_formulate_output_flag) {
         // formulate the output information for chinese/english output
         // english: word as the units
         std::vector<SSeqLEngLineInfor> new_line_vec;
         if (_m_resnet_strategy_flag) {
             formulate_output_resnet(out_line_vec, new_line_vec);
         } else {
             formulate_output(out_line_vec, new_line_vec);
         }
         out_line_vec = new_line_vec;
     }
    /*std::cout<<"checkout_line_vec:";
    for(int i = 0;i<out_line_vec.size(); i++)
    {
        for(int j = 0; j < out_line_vec[i].wordVec_.size(); j++)
        {   
            std::cout<<out_line_vec[i].wordVec_[j].wordStr_<<" ";
        }
    }
    std::cout<<std::endl;*/
     return ;
}

// recognize the row with English OCR engine
void CSeqModelChBatch::recognize_line_eng_batch(const std::vector<st_batch_img> &vec_imgs,
     CSeqLEngWordReg *recog_processor_en,\
     int *ptable_flag, std::vector<SSeqLEngLineInfor> &out_line_vec) {

    // threshold for the whole row using english ocr
    const float row_whole_eng_thresh = 0.95;
    // threhsold for piecewise english ocr
    const int row_piece_eng_thresh = 5;
    const double vertical_aspect_ratio = 2.0;

    int out_line_vec_size = out_line_vec.size();
    
    for (int i = 0; i < out_line_vec_size; i++) {   
        int word_num = out_line_vec[i].wordVec_.size();
        int line_w = out_line_vec[i].wid_;
        int line_h = out_line_vec[i].hei_;
        double ver_line_thresh = line_w * vertical_aspect_ratio;
        if (word_num <= 1 || line_h > ver_line_thresh) {
            continue ;
        }

        int eng_char_cnt = 0;
        double d_num_en_ratio = compute_en_char_ratio(out_line_vec[i], ptable_flag, eng_char_cnt);
        SSeqLEngLineInfor out_line;
        if (d_num_en_ratio >= row_whole_eng_thresh) {
            // call the whole row english model recognition
            // true: the region will be extend
            recognize_row_whole_eng_batch(vec_imgs[i].img, 
                recog_processor_en, true, out_line_vec[i], out_line);
            out_line_vec[i] = out_line;
        } else if (eng_char_cnt >= row_piece_eng_thresh) {
            // call the piecewise english model recognition
            // false: the region will not be extended
            recognize_row_piece_eng_batch(vec_imgs[i].img, 
                recog_processor_en, false, out_line_vec[i], out_line);
            out_line_vec[i] = out_line;
        }
    }
   
    //std::cout<<"_m_formulate_output_flag； "<<_m_formulate_output_flag<<std::endl; #1
    //std::cout<<"_m_resnet_strategy_flag: "<<_m_resnet_strategy_flag<<std::endl;  #0
    if (_m_formulate_output_flag) {
        // formulate the output information for chinese/english output
        // english: word as the units
        std::vector<SSeqLEngLineInfor> new_line_vec;
        if (_m_resnet_strategy_flag) {
            formulate_output_resnet(out_line_vec, new_line_vec);
        } else {
            formulate_output(out_line_vec, new_line_vec);
        }
        out_line_vec = new_line_vec;
    }
    
    return ;
}

void CSeqModelChBatch::recognize_row_whole_eng_batch_v3(const IplImage *image,
     CSeqLEngWordReg *recog_processor_en,
     bool extend_flag,
     SSeqLEngLineInfor &out_line){
    std::vector<SSeqLESegDecodeSScanWindow> in_eng_line_dect_vec;
    SSeqLESegDecodeSScanWindow temp_wind;
    temp_wind.left = 0;
    temp_wind.right = image->width - 1;
    temp_wind.top = 0;
    temp_wind.bottom = image->height - 1;
    temp_wind.isBlackWdFlag = 1;
    temp_wind.detectConf = 0;
    in_eng_line_dect_vec.push_back(temp_wind);
    std::vector<SSeqLEngLineInfor> out_line_vec_en;
    recognize_eng_row_batch(image, in_eng_line_dect_vec, 
         extend_flag, recog_processor_en, out_line_vec_en);
    std::vector<SSeqLEngLineInfor> out_line_vec_en_new;
    reform_line_infor_vec_split_char(out_line_vec_en, image->width,out_line_vec_en_new);
    /*for(int i = 0; i<out_line_vec_en_new.size();i++)
    {
        for(int j = 0; j < out_line_vec_en_new[i].wordVec_.size(); j++)
        {
            std::cout<<out_line_vec_en_new[i].wordVec_[j].wordStr_<<" ";
        }
        std::cout<<std::endl;
    }*/
    if (_m_formulate_output_flag) {
         for (unsigned int i = 0; i < out_line_vec_en_new.size(); i++) {
             for (unsigned int j = 0; j < out_line_vec_en_new[i].wordVec_.size(); j++) {
                 out_line_vec_en_new[i].wordVec_[j].left_add_space_ = true;
             }
         }
     }
 
    out_line = out_line_vec_en_new[0];  
}

void CSeqModelChBatch::recognize_row_whole_eng_batch(const IplImage *image,
    CSeqLEngWordReg *recog_processor_en,
    bool extend_flag,
    const SSeqLEngLineInfor &out_line_chn, SSeqLEngLineInfor &out_line) {
    
    std::vector<SSeqLESegDecodeSScanWindow> in_eng_line_dect_vec;
    SSeqLESegDecodeSScanWindow temp_wind;
    temp_wind.left = out_line_chn.left_;
    temp_wind.right =  out_line_chn.left_ + out_line_chn.wid_ - 1;
    temp_wind.top = out_line_chn.top_;
    temp_wind.bottom = out_line_chn.top_ + out_line_chn.hei_ - 1;
    temp_wind.isBlackWdFlag = out_line_chn.isBlackWdFlag_;
    temp_wind.detectConf = 0;
    in_eng_line_dect_vec.push_back(temp_wind);
    for (int i = 0; i < in_eng_line_dect_vec.size(); i++)
    {
        std::cout << "in_eng_line_dect_vec:" << in_eng_line_dect_vec[i].left << in_eng_line_dect_vec[i].right << std::endl;
    }
    std::vector<SSeqLEngLineInfor> out_line_vec_en;
    recognize_eng_row_batch(image, in_eng_line_dect_vec, 
        extend_flag, recog_processor_en, out_line_vec_en);

    std::vector<SSeqLEngLineInfor> out_line_vec_en_new;
    reform_line_infor_vec_split_char(out_line_vec_en, image->width, out_line_vec_en_new);
    std::cout<<"out_line_vec_en_new.size():"<<out_line_vec_en.size()<<std::endl;
    //std::cout<<"out_line_vec_en_new.size():"<<out_line_vec_en_new.size()<<std::endl;
    //std::cout<<"out_line_vec_en_new[i].wordVec_.size()"<<out_line_vec_en_new[0].wordVec_.size()<<std::endl; 
    //std::cout<<"_m_formulate_output_flag:"<<_m_formulate_output_flag<<std::endl;
    if (_m_formulate_output_flag) {
        for (unsigned int i = 0; i < out_line_vec_en_new.size(); i++) {
            for (unsigned int j = 0; j < out_line_vec_en_new[i].wordVec_.size(); j++) {
                out_line_vec_en_new[i].wordVec_[j].left_add_space_ = true;
            }
        }
    }

    int line_vec_ch_size = out_line_chn.wordVec_.size();
    int line_eng_row_size = out_line_vec_en_new.size();

    out_line = out_line_chn;
    if (line_eng_row_size > 0) {
        int line_vec_en_size = 0;             
        for (unsigned int j = 0; j < out_line_vec_en_new[0].wordVec_.size(); j++) {
            line_vec_en_size += out_line_vec_en_new[0].wordVec_[j].charVec_.size();
        }
        if (line_vec_en_size > line_vec_ch_size / 4.0) {
            out_line  = out_line_vec_en_new[0];
        }
    } 

    return ;
}

void CSeqModelChBatch::recognize_row_piece_eng_batch(const IplImage *image, 
    CSeqLEngWordReg *recog_processor_en, bool extend_flag, 
    const SSeqLEngLineInfor &out_line_chn,
    SSeqLEngLineInfor &out_line) {

    int ret_val = 0;
    std::vector<SSeqLESegDecodeSScanWindow> in_eng_line_dect_vec;
    std::vector<int> start_vec;
    std::vector<int> end_vec;
    out_line = out_line_chn;
    ret_val = locate_eng_pos(out_line_chn, in_eng_line_dect_vec, start_vec, end_vec);
    if (ret_val != 0) {
        return ;
    }

    // recognize the row with english ocr
    std::vector<SSeqLEngLineInfor> out_line_vec_en;
    ret_val = recognize_eng_row_batch(image, in_eng_line_dect_vec, extend_flag,
        recog_processor_en, out_line_vec_en);
    if (ret_val != 0) {
        return ;
    }

    // get the position of each character
    std::vector<SSeqLEngLineInfor> out_vec_new;
    reform_line_infor_vec_split_char(out_line_vec_en, image->width, out_vec_new);

    SSeqLEngLineInfor merge_line;
    ret_val = merge_two_results(out_line, out_vec_new, start_vec, end_vec, merge_line);
    if (ret_val == 0) {
        out_line = merge_line;
    }

    return ;
}

int CSeqModelChBatch::merge_seg_line(std::vector<SSeqLEngLineRect> line_rect_vec, 
        std::vector<SSeqLEngLineInfor> in_line_vec, 
        std::vector<SSeqLEngLineInfor>& out_line_vec) {

    if (line_rect_vec.size() != in_line_vec.size()) {
        return -1;
    }
    if (in_line_vec.size() <= 0) {
        return -1;
    }

    out_line_vec.clear();
    SSeqLEngLineInfor out_line = in_line_vec[0];
    out_line.wordVec_.clear();
    int last_line_right = 0;

    for (int i = 0; i < in_line_vec.size(); i++) {
        SSeqLEngLineInfor line = in_line_vec[i];

        int next_line_left = 0;
        if (i < in_line_vec.size() - 1) {
            next_line_left = line_rect_vec[i + 1].left_;
        } else {
            next_line_left = line_rect_vec[i].left_ + line_rect_vec[i].wid_;
        }
        last_line_right = std::max(last_line_right, line_rect_vec[i].left_);
        //std::cout << next_line_left  << " " << last_line_right << std::endl;

        int word_right = 0;
        for (int j = 0; j < line.wordVec_.size(); j++) {
            SSeqLEngWordInfor word = line.wordVec_[j];
            if (word.left_ > next_line_left) {
                continue;
            } else if (word.left_ < last_line_right) {
                continue;
            } else {
                out_line.wordVec_.push_back(word);
                word_right = word.left_ + word.wid_;
            }
        }
        last_line_right = word_right;
        out_line.wid_ = next_line_left;
    }
    out_line_vec.push_back(out_line);

    return 0;
}

// recognize the english rows
int CSeqModelChBatch::recognize_eng_row_batch(const IplImage *image, 
    std::vector<SSeqLESegDecodeSScanWindow> in_eng_line_dect_vec,
    bool extend_flag,
    CSeqLEngWordReg *recog_processor_en,
    std::vector<SSeqLEngLineInfor>& out_line_vec_en) {
    
    int ret_val = 0;
    int max_long_line = 900;
    int max_seg_long = 900;
    int max_overlap_seg = 200;
    
    CSeqLEngWordSegProcessor seg_processor_en;
    std::vector<SSeqLEngLineRect> line_rect_vec_en;
    std::vector<SSeqLEngLineInfor> out_line_vec_tmp;
   
    ////////_m_resnet_strategy_flag==1
    if (_m_resnet_strategy_flag) {
        std::vector<SSeqLEngLineRect> line_rect_vec;
        if (image->width <= max_long_line) {
            SSeqLEngLineRect line_rect;
            line_rect.left_ = 0;
            line_rect.top_ = 0;
            line_rect.wid_ = image->width;
            line_rect.hei_ = image->height;
            line_rect.isBlackWdFlag_ = 1;
            SSeqLEngRect win_rect;
            win_rect.left_ = 0;
            win_rect.top_ = 0;
            win_rect.wid_ = image->width;
            win_rect.hei_ = image->height;
            line_rect.rectVec_.push_back(win_rect);
            line_rect_vec.push_back(line_rect);
        } else {
            int st_pos = 0;
            while (st_pos + max_overlap_seg < image->width) {
                int seg_long = std::min(max_seg_long, (int)(image->width - st_pos));
                //to overcome the last part too short
                if (st_pos + seg_long + max_overlap_seg >= image->width) {
                    seg_long = image->width - st_pos;
                }
                SSeqLEngLineRect line_rect;
                line_rect.left_ = st_pos;
                line_rect.top_ = 0;
                line_rect.wid_ = seg_long;
                line_rect.hei_ = image->height;
                line_rect.isBlackWdFlag_ = 1;
                SSeqLEngRect win_rect;
                win_rect.left_ = st_pos;
                win_rect.top_ = 0;
                win_rect.wid_ = seg_long;
                win_rect.hei_ = image->height;
                line_rect.rectVec_.push_back(win_rect);
                line_rect_vec.push_back(line_rect);
                //std::cout << st_pos << " " << image->width << std::endl;
                st_pos += seg_long - max_overlap_seg;
            }
        }
        
        int line_rect_vec_size_t =  line_rect_vec.size();
        for (int j = 0; j < line_rect_vec_size_t; j++) {
            line_rect_vec_en.push_back(line_rect_vec[j]);
        }
    } else {
        //std::vector<SSeqLEngLineRect> line_rect_vec_en;
        seg_processor_en.set_exd_parameter(extend_flag);
        seg_processor_en.process_image_eng_ocr(image, in_eng_line_dect_vec);
        seg_processor_en.wordSegProcessGroupNearby(4);
        line_rect_vec_en = seg_processor_en.getSegLineRectVec();
    }
    int line_rect_vec_size = line_rect_vec_en.size();
    
    std::vector<SSeqLEngLineInfor> line_infor_vec_en;
    CSeqLEngLineDecoder * line_decoder = new CSeqLEngLineBMSSWordDecoder;
 
    for (unsigned int i = 0; i < line_rect_vec_size; i++) {
        // true: do not utilize the 2nd recognition function
        // false: utilize the 2nd recognition function
        SSeqLEngLineInfor temp_line; 
        bool fast_recg_flag = true;
        SSeqLEngSpaceConf space_conf; // = seg_processor_en.getSegSpaceConf(i);
        ///////////_m_resnet_strategy_flag=0/////
        if (!_m_resnet_strategy_flag) {
            space_conf = seg_processor_en.getSegSpaceConf(i);
        }

        recog_processor_en->recognize_eng_word_whole_line(image, line_rect_vec_en[i], \
                temp_line, space_conf, fast_recg_flag);

        /*std::cout<<"after_engModel_lineStr_:";
        for (unsigned int i = 0; i < temp_line.wordVec_.size(); ++i){
            std::cout<<temp_line.wordVec_[i].wordStr_<<" ";    
        }
        std::cout<<std::endl;*/

        line_infor_vec_en.push_back(temp_line);

        line_decoder->setWordLmModel(_m_eng_search_recog_eng, _m_inter_lm_eng);
        line_decoder->setSegSpaceConf(space_conf);
        line_decoder->set_acc_eng_highlowtype_flag(_m_acc_eng_highlowtype_flag);
        ((CSeqLEngLineBMSSWordDecoder*)line_decoder)->\
            set_resnet_strategy_flag(_m_resnet_strategy_flag);
        SSeqLEngLineInfor out_line = line_decoder->decodeLine(\
                recog_processor_en, temp_line);

        /*std::cout<<"after_decodePart_outline:";
        for (unsigned int i = 0; i < out_line.wordVec_.size(); i++){
            std::cout<<out_line.wordVec_[i].wordStr_<<" ";
        }
        std::cout<<std::endl;*/
        /*std::string str_line = "";
        for (unsigned int j = 0; j < out_line.wordVec_.size(); j++) {
            std::cout << out_line.left_ << " " << out_line.wid_ << std::endl;
            std::cout << out_line.wordVec_[j].wordStr_ << " " << out_line.wordVec_[j].left_ << " " << out_line.wordVec_[j].left_ + out_line.wordVec_[j].wid_ << std::endl;
            if (out_line.wordVec_[j].left_add_space_) {
                str_line += out_line.wordVec_[j].wordStr_ + " ";
            } else {
                str_line += out_line.wordVec_[j].wordStr_;
            }
        }
        std::cout << str_line << std::endl;*/
        out_line_vec_tmp.push_back(out_line);

    }
    delete line_decoder;
    line_decoder = NULL;
    
    if (out_line_vec_tmp.size() > 1 && _m_resnet_strategy_flag) {
        merge_seg_line(line_rect_vec_en, out_line_vec_tmp, out_line_vec_en);
    } else {
        out_line_vec_en = out_line_vec_tmp;
    }

    return ret_val;
}

int CSeqModelChBatch::divide_dect_imgs(const std::vector<st_batch_img> &indectVec,
    std::vector<st_batch_img> &vec_row_imgs,
    std::vector<st_batch_img> &vec_column_imgs,
    std::vector<int> &vec_row_idxs,
    std::vector<int> &vec_column_idxs) {
    
    int ret = 0;
    const double vertical_aspect_ratio = 2.0;
    int dect_img_num = indectVec.size();
    for (unsigned int iline_idx = 0; iline_idx < dect_img_num; iline_idx++) {
        int line_height = indectVec[iline_idx].img->height;
        int line_width = indectVec[iline_idx].img->width;
        double vertical_height_thresh =  vertical_aspect_ratio*line_width;
        // classify whether the input line image belongs to vertical column
        if (line_height > vertical_height_thresh) { 
            // vertical images
            vec_column_imgs.push_back(indectVec[iline_idx]);
            vec_column_idxs.push_back(iline_idx);
        } else {
            // horizontal images
            vec_row_imgs.push_back(indectVec[iline_idx]);
            vec_row_idxs.push_back(iline_idx);
        }
    }

    return ret;
}

void CSeqModelChBatch::recognize_line_chn(const std::vector<st_batch_img> vec_imgs,\
    CSeqLAsiaWordReg *row_recog_processor, 
    CSeqLAsiaWordReg *col_recog_processor, 
    std::vector<SSeqLEngLineInfor> &out_line_vec)  {
   
    // Here, the relationship between input and output should be preserved
    // 1.divide the image into row_image and col_image 
    std::vector<st_batch_img> vec_row_imgs;
    std::vector<st_batch_img> vec_column_imgs;
    std::vector<int> vec_row_idxs;
    std::vector<int> vec_column_idxs;
    divide_dect_imgs(vec_imgs, vec_row_imgs, vec_column_imgs, \
        vec_row_idxs, vec_column_idxs);
    
    // 2. recognize the row rectangle set
    std::vector<SSeqLEngLineInfor> out_row_line_vec;
    recognize_line_row_chn(vec_row_imgs, row_recog_processor, out_row_line_vec);

    out_line_vec.resize(vec_imgs.size());
    for (int i = 0; i < out_row_line_vec.size(); i++) {
        int row_idx = vec_row_idxs[i];
        out_line_vec[row_idx] = out_row_line_vec[i];
    }

    // 3. recognize the column rectangle set
    if (col_recog_processor != NULL) {
        std::vector<SSeqLEngLineInfor> out_col_line_vec;
        recognize_line_col_chn(vec_column_imgs, col_recog_processor, out_col_line_vec);
        for (int i = 0; i < out_col_line_vec.size(); i++) {
            int row_idx = vec_column_idxs[i];
            out_line_vec[row_idx] = out_col_line_vec[i];
        }
    }

    return ;
}

void CSeqModelChBatch::recognize_line_seg(\
    const std::vector<st_batch_img> vec_row_imgs,\
    std::vector<SSeqLEngLineRect> line_rect_vec,\
    CSeqLAsiaWordReg *recog_processor,\
    std::vector<SSeqLEngLineInfor>& out_line_vec) {

    // 1. recognize using batch
    std::vector<SSeqLEngLineInfor> line_infor_vec;
    line_infor_vec.resize(line_rect_vec.size());
    std::vector<SSeqLEngLineInfor> out_line_vec_tmp;

    ((CSeqLAsiaWordReg*)recog_processor)->set_candidate_num(_m_recg_cand_num);
    ((CSeqLAsiaWordReg*)recog_processor)->set_resnet_strategy_flag(_m_resnet_strategy_flag);
    ((CSeqLAsiaWordReg*)recog_processor)->recognize_line_batch(vec_row_imgs,\
        line_rect_vec, line_infor_vec);
    
    // 2. decode row by row
    out_line_vec_tmp.clear();
    for (unsigned int i = 0; i < line_infor_vec.size(); i++) {
        if (_m_debug_line_index != -1 && _m_debug_line_index != i) {
            continue ;
        }

        CSeqLAsiaLineBMSSWordDecoder * line_decoder = new CSeqLAsiaLineBMSSWordDecoder; 
        //line_decoder->setWordLmModel(_m_eng_search_recog_chn, _m_inter_lm_chn);
        SSeqLEngLineInfor out_line; 
        line_decoder->set_save_low_prob_flag(_m_save_low_prob_flag);
        line_decoder->set_resnet_strategy_flag(_m_resnet_strategy_flag);
        out_line = line_decoder->decode_line(recog_processor, line_infor_vec[i]);
        delete line_decoder;
        out_line_vec_tmp.push_back(out_line);
        /*std::string res = "";
        for (int k = 0; k < out_line.wordVec_.size(); k++) {
            res += out_line.wordVec_[k].wordStr_;
        }
        std::cout << res << std::endl;*/
    }
    // 3. merge seg line
    merge_seg_line(line_rect_vec, out_line_vec_tmp, out_line_vec);
}

// recognize the row image
void CSeqModelChBatch::recognize_line_row_chn(\
    const std::vector<st_batch_img> &vec_row_imgs,\
    CSeqLAsiaWordReg *recog_processor, std::vector<SSeqLEngLineInfor> &out_line_vec) {

    int max_long_line = 900;
    int max_seg_long = 900;
    int max_overlap_seg = 200;
    std::vector<SSeqLEngLineInfor> out_line_vec_tmp;
    std::vector<int> long_line_id_vec;
    std::vector<SSeqLEngLineInfor> long_line_res_vec;
    std::vector<SSeqLEngLineInfor> short_line_vec;
    std::vector<st_batch_img> vec_short_row_imgs;
    long_line_res_vec.clear();
    long_line_id_vec.clear();
    short_line_vec.clear();
    vec_short_row_imgs.clear();

    // 1. row word segmentation
    std::vector<SSeqLEngLineRect> merge_rect_vec;
    for (int i = 0; i < vec_row_imgs.size(); i++) {

        if (vec_row_imgs[i].img->width > max_long_line) {

            int st_pos = 0;
            std::vector<SSeqLEngLineRect> line_rect_vec;
            line_rect_vec.clear();

            std::vector<st_batch_img> vec_long_row_img;
            vec_long_row_img.clear();

            std::vector<SSeqLEngLineInfor> out_long_line_vec;
            out_long_line_vec.clear();

            while (st_pos + max_overlap_seg < vec_row_imgs[i].img->width) {
                int seg_long = std::min(max_seg_long, 
                        (int)(vec_row_imgs[i].img->width - st_pos));

                //to overcome the last part too short
                if (st_pos + seg_long + max_overlap_seg >= \
                        vec_row_imgs[i].img->width) {
                    seg_long = vec_row_imgs[i].img->width - st_pos;
                }
                SSeqLEngLineRect line_rect;
                line_rect.left_ = st_pos;
                line_rect.top_ = 0;
                line_rect.wid_ = seg_long;
                line_rect.hei_ = vec_row_imgs[i].img->height;
                line_rect.isBlackWdFlag_ = 1;
                SSeqLEngRect win_rect;
                win_rect.left_ = st_pos;
                win_rect.top_ = 0;
                win_rect.wid_ = seg_long;
                win_rect.hei_ = vec_row_imgs[i].img->height;
                line_rect.rectVec_.push_back(win_rect);
                line_rect_vec.push_back(line_rect);
                vec_long_row_img.push_back(vec_row_imgs[i]);
                //std::cout << st_pos << " " << vec_row_imgs[i].img->width << " " << seg_long << std::endl;
                st_pos += seg_long - max_overlap_seg;
            }
            recognize_line_seg(vec_long_row_img, line_rect_vec,
                    recog_processor, out_long_line_vec);
            if (out_long_line_vec.size() == 1) {
                /*std::cout << "long id " << i << std::endl;
                std::string res = "";
                for (int k = 0; k < out_long_line_vec[0].wordVec_.size(); k++) {
                    res += out_long_line_vec[0].wordVec_[k].wordStr_;
                }
                std::cout << "out res: " << res << std::endl;*/
                long_line_id_vec.push_back(i);
                long_line_res_vec.push_back(out_long_line_vec[0]);
            } 
        } 
        else {
            std::vector<SSeqLEngLineRect> line_rect_vec;
        
            SSeqLEngLineRect line_rect;
            line_rect.left_ = 0;
            line_rect.top_ = 0;
            line_rect.wid_ = vec_row_imgs[i].img->width;
            line_rect.hei_ = vec_row_imgs[i].img->height;
            line_rect.isBlackWdFlag_ = 1;
            SSeqLEngRect win_rect;
            win_rect.left_ = 0;
            win_rect.top_ = 0;
            win_rect.wid_ = vec_row_imgs[i].img->width;
            win_rect.hei_ = vec_row_imgs[i].img->height;
            line_rect.rectVec_.push_back(win_rect);
            line_rect_vec.push_back(line_rect);
            vec_short_row_imgs.push_back(vec_row_imgs[i]);
    
            int line_rect_vec_size =  line_rect_vec.size();
            for (int j = 0; j < line_rect_vec_size; j++) {
                merge_rect_vec.push_back(line_rect_vec[j]);
            }
        }
    }

    std::vector<SSeqLEngLineInfor> line_infor_vec;
    line_infor_vec.resize(merge_rect_vec.size());
    _m_recg_line_num = 0;
    for (int i = 0; i < merge_rect_vec.size(); i++) {
        _m_recg_line_num += merge_rect_vec[i].rectVec_.size();
    }

    // 2. recognize using batch
    ((CSeqLAsiaWordReg*)recog_processor)->set_candidate_num(_m_recg_cand_num);
    ((CSeqLAsiaWordReg*)recog_processor)->set_resnet_strategy_flag(_m_resnet_strategy_flag);
    ((CSeqLAsiaWordReg*)recog_processor)->recognize_line_batch(vec_short_row_imgs,\
        merge_rect_vec, line_infor_vec);
    
    // 3. decode row by row
    out_line_vec.clear();
    for (unsigned int i = 0; i < line_infor_vec.size(); i++) {
        if (_m_debug_line_index != -1 && _m_debug_line_index != i) {
            continue ;
        }

        CSeqLAsiaLineBMSSWordDecoder * line_decoder = new CSeqLAsiaLineBMSSWordDecoder; 
        //line_decoder->setWordLmModel(_m_eng_search_recog_chn, _m_inter_lm_chn);
        SSeqLEngLineInfor out_line; 
        line_decoder->set_save_low_prob_flag(_m_save_low_prob_flag);
        line_decoder->set_resnet_strategy_flag(_m_resnet_strategy_flag);
        out_line = line_decoder->decode_line(recog_processor, line_infor_vec[i]);
        delete line_decoder;
        if (long_line_id_vec.size() > 0) {
            short_line_vec.push_back(out_line);
        } else {
            out_line_vec.push_back(out_line);
        }
    }

    //merge the long line
    if (long_line_id_vec.size() > 0) {
        int long_line_vec_idx = 0;
        int short_line_vec_idx = 0;
        while (long_line_vec_idx < long_line_id_vec.size() ||
                short_line_vec_idx < short_line_vec.size()) {
            if (long_line_vec_idx < long_line_id_vec.size()) {
                int long_line_id = long_line_id_vec[long_line_vec_idx];
                while (out_line_vec.size() < long_line_id && \
                        short_line_vec_idx < short_line_vec.size()) {
                    out_line_vec.push_back(short_line_vec[short_line_vec_idx++]);
                }
                out_line_vec.push_back(long_line_res_vec[long_line_vec_idx++]);
            } else {
                while (short_line_vec_idx < short_line_vec.size()) {
                    out_line_vec.push_back(short_line_vec[short_line_vec_idx++]);
                }
            }
        }
    }

    // merge the result of the same row
    int out_line_vec_size = out_line_vec.size();
    for (unsigned int i = 0; i < out_line_vec_size; i++) {
        std::string str_line_rst = "";
        for (unsigned int j = 0; j < out_line_vec[i].wordVec_.size(); j++) {
            str_line_rst += out_line_vec[i].wordVec_[j].wordStr_;
        }
        out_line_vec[i].lineStr_ = str_line_rst;
        //std::cout << "final_res: " << out_line_vec[i].lineStr_ << std::endl;
    }

    return ;
}

void CSeqModelChBatch::recognize_line_col_chn(
    const std::vector<st_batch_img> &vec_col_imgs,\
    CSeqLAsiaWordReg *recog_processor, std::vector<SSeqLEngLineInfor> &out_line_vec) {

    if (vec_col_imgs.size() <= 0) {
        return ;
    }
    std::vector<st_batch_img> vec_rotate_imgs;
    std::vector<CvRect> vec_dect_rect;
    for (unsigned int i = 0; i < vec_col_imgs.size(); i++) {
        if (_m_debug_line_index != -1 && _m_debug_line_index !=i) {
            continue ;
        }

        // 2.rotate the cropped image patch
        IplImage* rot_col_img = rotate_image(vec_col_imgs[i].img, 90, false);
        int flag = g_ini_file->get_int_value(
                "DRAW_COL_ROTATE_FLAG", IniFile::_s_ini_section_global);
        if (flag == 1) {
            char save_path[256];
            snprintf(save_path, 256, "./temp/%s-%d.jpg", g_pimage_title, i);  
            cvSaveImage(save_path, rot_col_img);
        }

        st_batch_img batch_img;
        batch_img.img = rot_col_img;
        batch_img.bg_flag = vec_col_imgs[i].bg_flag;
        vec_rotate_imgs.push_back(batch_img);

        CvRect dect_rect;
        int dect_w = vec_col_imgs[i].img->width;
        int dect_h = vec_col_imgs[i].img->height;
        dect_rect = cvRect(0, 0, dect_w, dect_h);
        vec_dect_rect.push_back(dect_rect); 
    }
        
    std::vector<SSeqLEngLineInfor> out_col_line_vec;
    recognize_line_row_chn(vec_rotate_imgs, recog_processor, out_col_line_vec);
        
    // 5.convert the result to get the position in the orginal image
    std::vector<SSeqLEngLineInfor> rot_out_col_line_vec;
    rotate_col_recog_rst(out_col_line_vec, vec_dect_rect, rot_out_col_line_vec);
    for (int j = 0; j < rot_out_col_line_vec.size(); j++) {
        out_line_vec.push_back(rot_out_col_line_vec[j]);
    }

    // free the memory
    for (int i = 0; i < vec_rotate_imgs.size(); i++) {
        if (vec_rotate_imgs[i].img) {
            cvReleaseImage(&vec_rotate_imgs[i].img);
            vec_rotate_imgs[i].img = NULL;
        }
    }

    return ;
}

// process the recognition result of the column image
void CSeqModelChBatch::rotate_col_recog_rst(std::vector<SSeqLEngLineInfor> in_col_line_vec,
    std::vector<CvRect> dect_rect_vec,  std::vector<SSeqLEngLineInfor> &out_col_line_vec) {
    
    out_col_line_vec = in_col_line_vec;
    for (int line_idx = 0; line_idx < in_col_line_vec.size(); line_idx++) {

        out_col_line_vec[line_idx].left_ = dect_rect_vec[line_idx].x;
        out_col_line_vec[line_idx].top_ = dect_rect_vec[line_idx].y;
        out_col_line_vec[line_idx].wid_ = dect_rect_vec[line_idx].width;
        out_col_line_vec[line_idx].hei_ = dect_rect_vec[line_idx].height;

        for (int i = 0; i < in_col_line_vec[line_idx].wordVec_.size(); i++) {
            SSeqLEngWordInfor in_word = in_col_line_vec[line_idx].wordVec_[i];
            SSeqLEngWordInfor& out_word = out_col_line_vec[line_idx].wordVec_[i];
            out_word.wid_ = in_word.hei_;
            out_word.hei_ = in_word.wid_;
            out_word.left_ = in_word.top_ + dect_rect_vec[line_idx].x;
            int shift_y = (int)(out_word.hei_ * 0.25);
            out_word.top_ = std::max(in_word.left_ + dect_rect_vec[line_idx].y - shift_y, 0);
        }
    }

    return ;
}
};
/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
