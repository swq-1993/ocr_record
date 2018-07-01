/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file reform_en_rst.cpp
 * @author xieshufu(com@baidu.com)
 * @date 2015/09/10 10:03:57
 * @brief 
 *  
 **/
#include "reform_en_rst.h"
#include <iostream>
#include <stdio.h>
#include "common_func.h"
#include "SeqLEngWordRecog.h"

namespace nsseqocrchn
{
int combine_chars(const SSeqLEngWordInfor &word_info, int *pchar_flag, 
    std::vector<SSeqLEngWordInfor> &word_vec, bool resnet_strategy_flag);
int classify_char_type(std::string str, int &char_type);

void shrink_max_str(std::string & maxStr, std::string & shrinkStr,\
        std::vector<int> & shrinkFrom, std::vector<int> & shrinkTo)
{ 
    //std::vector<int> shrinkFrom, shrinkTo;
    shrinkStr.clear();
    shrinkFrom.clear();
    shrinkTo.clear();
    //char cur_char = 0;
    //int start_char = -1;
    for (unsigned int k = 0; k < maxStr.size(); k++) {
        if (maxStr[k] != '_' && (k == 0 || maxStr[k-1] != maxStr[k])) {
            shrinkStr.push_back(maxStr[k]);
            shrinkFrom.push_back(k);
        }
    }
    shrinkTo.resize(shrinkFrom.size());
    for (unsigned int i = 0; i < shrinkFrom.size(); i++) {
        if (i == shrinkFrom.size() - 1) {
            shrinkTo[i] = shrinkFrom[i]+1 <= (int)(maxStr.size() - 1) ?\
                          shrinkFrom[i]+1 : shrinkFrom[i];
            //shrinkTo[i] = shrinkFrom[i]+2 <= maxStr.size() - 1 ? shrinkFrom[i]+2 : maxStr.size() - 1;
            //shrinkTo[i] = maxStr.size() - 1;
        }
        else {
            shrinkTo[i] = shrinkFrom[i+1] - 1;
        }
    }
}

float global_string_mapping(std::string & from_str,\
        std::string & to_str, std::vector<int> & from_map) { 
    int len1 = from_str.size() + 1; // cols
    int len2 = to_str.size() + 1; // rows
    float * score_table = new float[len1 * len2];
    int * score_from = new int[len1 * len2];
    float space_match_panelty = -1;
    float mismatch_panelty = -1.5;
    float match_panelty = 1;
    score_table[0] = 0;
    score_from[0] = -1;
    for (int i = 1; i < len1; i++) {
        score_table[i] = score_table[i-1] + space_match_panelty;
        score_from[i] = i-1;
    }
    for (int j = 1; j < len2; j++) {
        score_table[j*len1] = score_table[(j-1)*len1] + space_match_panelty;
        score_from[j*len1] = (j-1)*len1;
    }
    for (int j = 1; j < len2; j++) {
        for (int i = 1; i < len1; i++) {
            float val1 = score_table[(j-1)*len1+i]+space_match_panelty; // score from top, left -> -
            float val2 = score_table[j*len1+i-1]+space_match_panelty;   // score from left, top -> -
            float val3 = score_table[(j-1)*len1+i-1];                   
            if (from_str[i-1] == to_str[j-1]) {
                val3 += match_panelty;     // score from topleft, match
            }
            else {
                // score from topleft, mismatch
                val3 += mismatch_panelty; 
            }
            float maxval = val1;
            int maxval_from = (j - 1) * len1 + i;
            if (val2 > maxval) {
                maxval = val2;
                maxval_from = j * len1 + i - 1;
            }
            if (val3 > maxval) {
                maxval = val3;
                maxval_from = (j - 1) * len1 + (i - 1);
            }
            score_table[j * len1 + i] = maxval;
            score_from[j * len1 + i] = maxval_from;
        }
    }
    from_map.clear();
    from_map.resize(len1 - 1);
    for (int i = 0; i < len1 - 1; i++) {
        from_map[i] = -1;
    }
    int ind = len1 * len2 - 1;
    while (score_from[ind] != -1) {
        int i = ind % len1;
        int j = ind / len1;
        int from1 = (j - 1) * len1 + i;
        int from2 = j * len1 + i - 1;
        int from3 = (j - 1) * len1 + i - 1;
        if (score_from[ind] == from1) {
            ind = from1;
        }
        else if (score_from[ind] == from2) {
            from_map[i - 1] = -1;
            ind = from2;
        }
        else if (score_from[ind] == from3) {
            from_map[i - 1] = j - 1;
            //assert(from_str[i-1] == to_str[j-1]);
            ind = from3;
        }
    }
    float max_score = score_table[len1 * len2 - 1];
    if (score_table) {
        delete [] score_table; 
        score_table = NULL;
    }
    if (score_from) {
        delete [] score_from;
        score_from = NULL;
    }
    return max_score;
}

// find the position of each char in maxStr
void wordstr_to_maxstr(std::string &wordStr, std::string &maxStr, \
        std::vector<float> & charFrom, std::vector<float> & charTo, float wordLen) {
    std::vector<int> shrink_from;
    std::vector<int> shrink_to;
    std::string shrink_str;
    shrink_max_str(maxStr, shrink_str, shrink_from, shrink_to);

    std::vector<int> char2shrink;
    float score = global_string_mapping(wordStr, shrink_str, char2shrink);
    int neg_start = -1;
    int  neg_stop = -1;
    float time_len = wordLen / maxStr.size();
    //std::cout << "time_len = " << time_len << std::endl;

    charFrom.clear(); 
    charTo.clear();
    charFrom.resize(wordStr.size());
    charTo.resize(wordStr.size());
    for (unsigned int i = 0; i < wordStr.size(); i++) {
        if (char2shrink[i] != -1) {
            int j = char2shrink[i];
            // old stargety
            if (0) {
                charFrom[i] = shrink_from[j] * time_len;
                charTo[i] = (shrink_to[j]+1) * time_len;
            }
            else {
                // new stragety
                if (j == 0) {
                    charFrom[i] = shrink_from[j] > 2 ? (shrink_from[j] - 2) * time_len : 0;
                }
                else {
                    charFrom[i] = (shrink_from[j] + shrink_from[j - 1] + 1) / 2.0 * time_len;
                }
                //charTo[i] = (j == shrink_str.size() - 1) ? wordLen : (shrink_from[j]+ shrink_from[j+1] + 1)/2.0 * time_len;
                charTo[i] = (j == (int)(shrink_str.size() - 1)) ?\
                            (shrink_from[j] + 2) * time_len :\
                            (shrink_from[j] + shrink_from[j + 1] + 1) / 2.0 * time_len;
            }
        }
    }
    for (unsigned int i = 0; i < wordStr.size(); i++) {
        if (i == 0) {
            if (char2shrink[i] == -1) {
                neg_start = 0;
                neg_stop = 0;
            }
        }
        else {
            if (char2shrink[i] == -1) {
                if (char2shrink[i-1] == -1) {
                    neg_stop++; // continue extend mismatch
                }
                else {// if(char2shrink[i-1] != -1      // start extend mismatch
                    neg_start = i;
                    neg_stop = i;
                }
            }
            else // if(char2shrink[i] != -1)
            {
                if (char2shrink[i - 1] == -1)  // mismatch stop
                {
                    float left_pos = neg_start == 0 ? 0 : charTo[neg_start - 1];
                    float right_pos = neg_stop == (int)wordStr.size() - 1 ? \
                                      wordLen : charFrom[neg_stop + 1];
                    float avg_len = (right_pos - left_pos) / (neg_stop - neg_start + 1);
                    for (int j = neg_start; j <= neg_stop; j++) {
                        charFrom[j] = left_pos + (j - neg_start) * avg_len;
                        charTo[j] = charFrom[j] + avg_len;
                    }
                }
            }
        }
    }
}
void wordstr_to_maxstr2(std::string &wordStr, std::string &maxStr,\
        std::vector<float> & charFrom, std::vector<float> & charTo, float wordLen) {
    std::string wordstr_small = wordStr;
    std::string maxstr_small = maxStr;
    for (int i = 0; i < wordstr_small.size(); i++) {
        if (wordstr_small[i] >= 'A' && wordstr_small[i] <= 'Z') {
            wordstr_small[i] = wordstr_small[i] - 'A' + 'a';
        }
    }
    for (int i = 0; i < maxstr_small.size(); i++) {
        if (maxstr_small[i] >= 'A' && maxstr_small[i] <= 'Z') {
            maxstr_small[i] = maxstr_small[i] - 'A' + 'a';
        }
    }
    return wordstr_to_maxstr(wordstr_small, maxstr_small, charFrom, charTo, wordLen);
}

void reform_line_infor_vec(std::vector<SSeqLEngLineInfor> &inLineInforVec,\
        std::vector<SSeqLEngLineInfor> & outLineInforVec) {
    outLineInforVec.clear();
    outLineInforVec.resize(inLineInforVec.size());
    for (int i = 0; i < inLineInforVec.size(); i++) {
        SSeqLEngLineInfor & in_line = inLineInforVec[i];
        std::vector<SSeqLEngWordInfor> in_word_vec = in_line.wordVec_;
        SSeqLEngLineInfor &out_line = outLineInforVec[i];
        std::vector<SSeqLEngWordInfor> & out_word_vec = out_line.wordVec_;
        out_line.left_ = in_line.left_;
        out_line.top_ = in_line.top_;
        out_line.wid_ = in_line.wid_;
        out_line.hei_ = in_line.hei_;
        for (int j = 0; j < in_word_vec.size(); j++) {
            SSeqLEngWordInfor & temp_word = in_word_vec[j];
            float left = temp_word.left_;
            float top = temp_word.top_;
            float wid = temp_word.wid_;
            float hei = temp_word.hei_;
            float right = left + wid - 1;
            std::string word_str = temp_word.wordStr_;
            std::string max_str = temp_word.maxStr_;
            if (j != in_word_vec.size() - 1) // merge word_str_ which has the same maxStr_
            {
                while (j + 1 <= in_word_vec.size() - 1 && in_word_vec[j + 1].maxStr_ == max_str) {
                    word_str += in_word_vec[j + 1].wordStr_;
                    if (in_word_vec[j + 1].left_ + in_word_vec[j + 1].wid_ - 1 > right) {
                        right = in_word_vec[j + 1].left_ + in_word_vec[j + 1].wid_ - 1;
                    }
                    j++;
                }
            }
            std::vector<float> char_from;
            std::vector<float> char_to;
            wordstr_to_maxstr2(word_str, max_str, char_from, char_to, right - left + 1.0);
            for (int k = 0; k < word_str.size(); k++) {
                SSeqLEngWordInfor new_word;
                new_word.left_ = (int)(char_from[k] + left + 0.5)-3;
                new_word.top_ = (int)top;
                new_word.wid_ = (int)(char_to[k] - char_from[k] + 0.5)+6;
                new_word.hei_ = (int)hei;
                new_word.wordStr_ = "";
                if (new_word.wid_ > 0) {
                    new_word.wordStr_.push_back(word_str[k]);
                    out_word_vec.push_back(new_word);
                }
            }
        }
    }
}

// this function will save both word and char information
void reform_line_infor_vec_split_char(std::vector<SSeqLEngLineInfor> &inLineInforVec,\
        int image_width, std::vector<SSeqLEngLineInfor> & outLineInforVec) {
    outLineInforVec.clear();
    outLineInforVec.resize(inLineInforVec.size());
    for (unsigned int i = 0; i < inLineInforVec.size(); i++) {
        SSeqLEngLineInfor & in_line = inLineInforVec[i];
        std::vector<SSeqLEngWordInfor> in_word_vec = in_line.wordVec_;
        SSeqLEngLineInfor &out_line = outLineInforVec[i];
        std::vector<SSeqLEngWordInfor> & out_word_vec = out_line.wordVec_;
        out_line.left_ = in_line.left_;
        out_line.top_ = in_line.top_;
        out_line.wid_ = in_line.wid_;
        out_line.hei_ = in_line.hei_;
        for (unsigned int j = 0; j < in_word_vec.size(); j++) {
            SSeqLEngWordInfor & temp_word = in_word_vec[j];
            SSeqLEngWordInfor save_word = in_word_vec[j];

            float left = temp_word.left_;
            float top = temp_word.top_;
            float wid = temp_word.wid_;
            float hei = temp_word.hei_;
            float right = left + wid - 1;
            std::string word_str = temp_word.wordStr_;
            std::string max_str = temp_word.maxStr_;
            if (j != in_word_vec.size() - 1) // merge word_str_ which has the same maxStr_
            {
                while (j + 1 <= (int)in_word_vec.size() - 1 \
                        && in_word_vec[j + 1].maxStr_ == max_str) {
                    word_str += in_word_vec[j + 1].wordStr_;
                    if (in_word_vec[j + 1].left_ + in_word_vec[j + 1].wid_ - 1 > right) {
                        right = in_word_vec[j + 1].left_ + in_word_vec[j + 1].wid_ - 1;
                    }
                    j++;
                }
            }

            save_word.wordStr_ = word_str;
            std::vector<float> char_from;
            std::vector<float> char_to;
            wordstr_to_maxstr2(word_str, max_str, char_from, char_to, right - left + 1.0);

            for (unsigned int k = 0; k < word_str.size(); k++) {
                SSeqLEngCharInfor new_char;
                
                new_char.left_ = std::max((int)(char_from[k] + left + 0.5) - 3, 0);
                new_char.top_ = (int)top;
                new_char.wid_ = (int)(char_to[k] - char_from[k] + 0.5) + 6;
                new_char.hei_ = (int)hei;
                if (new_char.left_ + new_char.wid_ >= image_width) {
                    new_char.wid_ = image_width - new_char.left_;
                }
                snprintf(new_char.charStr_, 4, "%s", "");
                if (new_char.wid_ > 0) {
                    char char_str = word_str[k];
                    snprintf(new_char.charStr_, 4, "%c", char_str);
                    save_word.charVec_.push_back(new_char);
                }
            }

            //save_word.left_add_space_ = true;
            out_word_vec.push_back(save_word);
        }
    }
}

// compute the position of each character
void est_line_split_char(std::vector<SSeqLEngLineInfor> &inLineInforVec,\
     int image_width, std::vector<SSeqLEngLineInfor> &outLineInforVec) {

    outLineInforVec.clear();
    outLineInforVec.resize(inLineInforVec.size());
    for (unsigned int i = 0; i < inLineInforVec.size(); i++) {
        SSeqLEngLineInfor & in_line = inLineInforVec[i];
        std::vector<SSeqLEngWordInfor> in_word_vec = in_line.wordVec_;
        SSeqLEngLineInfor &out_line = outLineInforVec[i];
        std::vector<SSeqLEngWordInfor> & out_word_vec = out_line.wordVec_;
        out_line.left_ = in_line.left_;
        out_line.top_ = in_line.top_;
        out_line.wid_ = in_line.wid_;
        out_line.hei_ = in_line.hei_;
        for (unsigned int j = 0; j < in_word_vec.size(); j++) {
            SSeqLEngWordInfor & temp_word = in_word_vec[j];
            SSeqLEngWordInfor save_word = in_word_vec[j];

            float left = temp_word.left_;
            float top = temp_word.top_;
            float wid = temp_word.wid_;
            float hei = temp_word.hei_;
            float right = left + wid - 1;
            std::string word_str = temp_word.wordStr_;
            std::string max_str = temp_word.maxStr_;
            
            save_word.wordStr_ = word_str;
            int char_num = word_str.size();
            float char_width = (float)wid / char_num;

            for (unsigned int k = 0; k < char_num; k++) {
                SSeqLEngCharInfor new_char;
                
                new_char.left_ = left + char_width * k;
                new_char.top_ = (int)top;
                new_char.wid_ = (int)(char_width + 0.5);
                new_char.hei_ = (int)hei;
                if (new_char.left_ + new_char.wid_ >= image_width) {
                    new_char.wid_ = image_width - new_char.left_;
                }
                snprintf(new_char.charStr_, 4, "%s", "");
                if (new_char.wid_ > 0) {
                    char char_str = word_str[k];
                    snprintf(new_char.charStr_, 4, "%c", char_str);
                    save_word.charVec_.push_back(new_char);
                }
            }

            save_word.left_add_space_ = true;
            out_word_vec.push_back(save_word);
        }
    }
}

//
void split_eng_word(const SSeqLEngWordInfor &word_in,
    std::vector<SSeqLEngWordInfor> &word_out_vec, bool resnet_strategy_flag) {
    
    bool eng_model_flag = word_in.left_add_space_?true:false;
    if (eng_model_flag) {
        // output by english module
        std::vector<SSeqLEngCharInfor> char_vec = word_in.charVec_;
        int char_num = char_vec.size();
        int *flag_vec = new int[char_num];
        for (int char_idx = 0; char_idx < char_num; char_idx++) {
            int char_type = 0;
            std::string char_label = std::string(char_vec[char_idx].charStr_);
            classify_char_type(char_label, char_type);
            if (char_type == 2) {
                // English
                flag_vec[char_idx] = 1;
            } else {
                flag_vec[char_idx] = 0;
            }
        }
        
        combine_chars(word_in, flag_vec, word_out_vec, resnet_strategy_flag);
        delete []flag_vec;
        flag_vec = NULL;
    } else {
        // output by chinese module
        word_out_vec.push_back(word_in);
    }

    return ;
}

int get_sub_word(const SSeqLEngWordInfor &word_info, int start_idx, int end_idx, 
        SSeqLEngWordInfor &sub_word, bool resnet_strategy_flag) {
    if (start_idx > end_idx) {
        return -1;
    }
    std::vector<SSeqLEngCharInfor> char_vec = word_info.charVec_;
    sub_word.charVec_.clear();
    sub_word.left_add_space_ = 1;
    sub_word.charVec_.push_back(char_vec[start_idx]);
    int x0 = char_vec[start_idx].left_;
    int y0 = char_vec[start_idx].top_;
    int x1 = x0 + char_vec[start_idx].wid_ - 1;
    int y1 = y0 + char_vec[start_idx].hei_ - 1;

    std::string word_str = std::string(char_vec[start_idx].charStr_); 
    bool eng_flag = false;
    bool high_flag = false;
    if (resnet_strategy_flag) {
        check_eng_str(word_str, eng_flag, high_flag);
    }

    for (int i = start_idx + 1; i <= end_idx; i++) {
        x0 = std::min(x0, char_vec[i].left_);
        y0 = std::min(y0, char_vec[i].top_);
        x1 = std::max(x1, char_vec[i].left_ + char_vec[i].wid_ - 1);
        y1 = std::max(y1, char_vec[i].top_ + char_vec[i].hei_ - 1); 
        sub_word.charVec_.push_back(char_vec[i]);
        word_str += std::string(char_vec[i].charStr_); 
    }
    sub_word.left_ = x0;
    sub_word.top_ = y0;
    sub_word.wid_ = x1 - x0 + 1;
    sub_word.hei_ = y1 - y0 + 1;
    sub_word.wordStr_ = word_str;
    
    if (resnet_strategy_flag) {
        if (eng_flag) {
            int input_word_len = word_info.charVec_.size();
            int output_word_len = end_idx - start_idx + 1;
            sub_word.regLogProb_ = word_info.regLogProb_ / input_word_len * output_word_len;
            //std::cout << "sub eng word: " << word_str << std::endl;
        } else {
            sub_word.regLogProb_ = 1.0;
            //std::cout << "no sub eng word: " << word_str << std::endl;
        }
    }

    return 0;
}

//
int get_combine_word(const std::vector<SSeqLEngWordInfor> &word_vec_in, int start_idx, int end_idx, 
        SSeqLEngWordInfor &combine_word) {
    if (start_idx > end_idx) {
        return -1;
    }
    int x0 = word_vec_in[start_idx].left_;
    int y0 = word_vec_in[start_idx].top_;
    int x1 = x0 + word_vec_in[start_idx].wid_ - 1;
    int y1 = y0 + word_vec_in[start_idx].hei_ - 1;
    std::string word_str = word_vec_in[start_idx].wordStr_; 
    for (int i = start_idx + 1; i <= end_idx; i++) {
        x0 = std::min(x0, word_vec_in[i].left_);
        y0 = std::min(y0, word_vec_in[i].top_);
        x1 = std::max(x1, word_vec_in[i].left_ + word_vec_in[i].wid_ - 1);
        y1 = std::max(y1, word_vec_in[i].top_ + word_vec_in[i].hei_ - 1); 
        word_str += word_vec_in[i].wordStr_; 
    }
    combine_word.left_ = x0;
    combine_word.top_ = y0;
    combine_word.wid_ = x1 - x0 + 1;
    combine_word.hei_ = y1 - y0 + 1;
    combine_word.wordStr_ = word_str;
    
    return 0;
}

int combine_chars(const SSeqLEngWordInfor &word_info, int *pchar_flag, 
    std::vector<SSeqLEngWordInfor> &word_vec, bool resnet_strategy_flag) {
    
    int ret_val = 0;
    int char_num = word_info.charVec_.size();
    int end_idx = -1;
    int start_idx = 0;
    int non_eng_num = 0;
    int *pnon_eng_idx = new int[char_num];

    for (int i = 0; i < char_num; i++) {
        if (pchar_flag[i] == 0) {
            pnon_eng_idx[non_eng_num] = i;
            non_eng_num++;
        }
    }

    if (non_eng_num == 0) {
        word_vec.push_back(word_info);
    } else {
        for (int i = 0; i < non_eng_num; i++) {
            int start_idx = 0;
            int end_idx = 0;
            if (i == 0) {
                start_idx = 0;
                end_idx = pnon_eng_idx[i] - 1;
            }
            else {
                start_idx = pnon_eng_idx[i - 1] + 1;
                end_idx = pnon_eng_idx[i] - 1;
            }

            int ret_sub = 0;
            SSeqLEngWordInfor sub_word;
            ret_sub = get_sub_word(word_info, start_idx, end_idx, sub_word, resnet_strategy_flag);
            if (ret_sub == 0) {
                word_vec.push_back(sub_word);
            }
            ret_sub = get_sub_word(word_info, pnon_eng_idx[i], 
                    pnon_eng_idx[i], sub_word, resnet_strategy_flag);
            if (ret_sub == 0) {
                word_vec.push_back(sub_word);
            }
        }
        // the last words
        int start_idx = pnon_eng_idx[non_eng_num - 1] + 1;
        int end_idx = char_num - 1;            
        SSeqLEngWordInfor sub_word;
        int ret_sub = get_sub_word(word_info, start_idx, end_idx, sub_word, resnet_strategy_flag);
        if (ret_sub == 0) {
            word_vec.push_back(sub_word);
        }
        // sort the word vector
        sort(word_vec.begin(), word_vec.end(), leftSeqLWordRectFirst);
    }

    delete []pnon_eng_idx;
    pnon_eng_idx = NULL;

    return ret_val;
}

//
int classify_char_type(std::string str, int &char_type) {
    int ret_val = 0;
    bool num_flag = false;
    bool eng_flag = false;
    bool high_flag = false;    

    // language category type 0: Chinese; 1: 0-9; 2: a-z A-Z; 3:punctuation
    char_type = 0;
    check_num_str(str, num_flag);
    check_eng_str(str, eng_flag, high_flag);
    if (num_flag) {
        char_type = 1;
    } else if (eng_flag) {
        char_type = 2;
    } else {
        char_type = 3;
    }

    return ret_val;
}

// 
void reform_eng_line(const std::vector<SSeqLEngWordInfor> &word_vec_in,
    std::vector<SSeqLEngWordInfor> &word_vec_out, bool resnet_strategy_flag) {
    word_vec_out.clear();
    int word_num = word_vec_in.size();

    for (int j = 0; j < word_num; j++) {
        SSeqLEngWordInfor word = word_vec_in[j];
        std::vector<SSeqLEngWordInfor> word_split_vec;
        split_eng_word(word, word_split_vec, resnet_strategy_flag);

        for (int k = 0; k < word_split_vec.size(); k++) {
            word_vec_out.push_back(word_split_vec[k]);
        }
    }

    return ;
}


//
int formulate_output(const std::vector<SSeqLEngLineInfor> &in_line_vec,
    std::vector<SSeqLEngLineInfor> &out_line_vec) {
    int ret_val = 0;
    int line_num = in_line_vec.size();

    out_line_vec = in_line_vec;
    bool spotflag;
    for (int i = 0; i < line_num; i++) {
        spotflag = false;
        SSeqLEngLineInfor line_info = in_line_vec[i];
        std::vector<SSeqLEngWordInfor> &word_vec = line_info.wordVec_;
        int word_num = word_vec.size();
        if (word_num <= 0) {
            continue;
        }
        //sunwanqi add 
        for (int index = 0; index < word_vec.size(); index++)
        {
            std::string::size_type idx;
            idx = word_vec[index].wordStr_.find("●");
            if (idx != std::string::npos)
            {
                spotflag = true;
            }
        }
        if (spotflag)
        {
            continue;
        }
            
        bool resnet_strategy_flag = false;
        reform_eng_line(word_vec, out_line_vec[i].wordVec_, resnet_strategy_flag);
    }

    return ret_val;
}

int formulate_output_resnet(const std::vector<SSeqLEngLineInfor> &in_line_vec,
    std::vector<SSeqLEngLineInfor> &out_line_vec) {
    int ret_val = 0;
    int line_num = in_line_vec.size();

    out_line_vec = in_line_vec;
    bool spotflag;
    for (int i = 0; i < line_num; i++) {
        spotflag = false;
        SSeqLEngLineInfor line_info = in_line_vec[i];
        std::vector<SSeqLEngWordInfor> &word_vec = line_info.wordVec_;
        int word_num = word_vec.size();
        if (word_num <= 0) {
            continue;
        }
        //sunwanqi add
        std::vector<int> unicode_vec;
        for (int index = 0; index < word_vec.size(); index++)
        {
            std::string::size_type idx;
            idx=word_vec[index].wordStr_.find("●");
            if (idx != std::string::npos )
                spotflag = true;
        }
        if (spotflag)
        {
            continue;
        }
        bool resnet_strategy_flag = true;
        reform_eng_line(word_vec, out_line_vec[i].wordVec_, resnet_strategy_flag);
    }

    return ret_val;
}
};
/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
