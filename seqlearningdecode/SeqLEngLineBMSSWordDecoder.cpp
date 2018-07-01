/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file SeqLEngLineBMSSWordDecoder.cpp
 * @author hanjunyu(com@baidu.com)
 * @date 2014/06/19 16:20:24
 * @brief 
 *  
 **/
#include <algorithm>
#include <math.h>
#include <fstream>
#include "SeqLEngLineBMSSWordDecoder.h"
#include "esc_searchapi.h"
#include "SeqLEngWordRecog.h"
//#include <fstream>
using namespace esp;

CSeqLEngLineBMSSWordDecoder:: CSeqLEngLineBMSSWordDecoder(){
    //std::ifstream param_file("param.ini");
    //std::string line;

    //getline(param_file, line);
    //float lmw = atof(line.c_str());

    //getline(param_file, line);
    //float lmw0 = atof(line.c_str());

    //getline(param_file, line);
    //float regw = atof(line.c_str());

    //getline(param_file, line);
    //float cdw= atof(line.c_str());

    //getline(param_file, line);
    //float ediw = atof(line.c_str());
    float lmw = 0.02;
    float lmw0 = 0.03;
    float regw = 0.2;
    float cdw = 0.4;
    float ediw = 3.2;

            beamWidth_   = 20;
            //lmWeight_    = 1.0;//0.5;
            //lmWeight0_    = 0.05;//1.5;//0.5;
            lmWeight_    = lmw;//0.5;
            lmWeight0_    = lmw0;//1.5;//0.5;
            //spWeight_    = 0;//2;
            //spWeight2_   = 0.0;
            //regWeight_   = 0.05;
            regWeight_   = regw;//0.07;
            //mulRegWeight_ = 100;
            //sjWeight_    = 0;//2;
            cdWeight_    = cdw;
            edit_dis_weight_ = ediw;
            nodeExtStep_ = 1000;
            spellCheckNum_ = 5;
            shortLineMaxWordNum_ = 1;
            shortLmDiscount_ = 1.0;
            //lx added for subtitle to remove noise
            _remove_head_end_noise = false;
            _m_acc_eng_highlowtype_flag = false;
            _m_resnet_strategy_flag = false;
}

CSeqLEngLineBMSSWordDecoder :: ~CSeqLEngLineBMSSWordDecoder(){
/*    SSeqLEngLineInfor tempInputLine;
    std::swap(inputLine_, tempInputLine);*/
}

void CSeqLEngLineBMSSWordDecoder::set_attention_param() {

    float lmw = 0.03;
    float lmw0 = 0.02;
    float regw = 0.15;
    float cdw = 0.6;
    float ediw = 3.3;

    lmWeight_ = lmw;
    lmWeight0_ = lmw0;
    regWeight_ = regw;
    cdWeight_ = cdw;
    edit_dis_weight_ = ediw;

}

void CSeqLEngLineBMSSWordDecoder::set_resnet_strategy_flag(bool flag) {
    _m_resnet_strategy_flag = flag;
}

int CSeqLEngLineBMSSWordDecoder:: stringAlignment(std::string str1, std::string str2,
      bool backtrace , int sp , int dp , int ip ) {
   std::vector<int> gtStr;
   std::vector<int> recogStr;
   gtStr.resize(str1.size());
   recogStr.resize(str2.size());
   for(unsigned int i=0; i<str1.size(); i++){
      gtStr[i] = str1[i];
   }
   for(unsigned int i=0; i<str2.size(); i++){
      recogStr[i] = str2[i];
   }

/*
    std::cout << "gtStr ";
    for(unsigned int i=0; i < gtStr.size(); i++){
      std::cout << gtStr[i] <<" ";
    }
    std::cout << std::endl;
    std::cout << "recogStr ";
    for(unsigned int i=0; i<recogStr.size(); i++){
      std::cout << recogStr[i] <<" ";
    }
    std::cout << std::endl;
*/

    std::vector< std::vector<int> > matrix;
    int substitutions;
    int deletions;
    int insertions;
    int distance;
    int subPenalty = sp;
    int delPenalty = dp;
    int insPenalty = ip;
    int n = gtStr.size();
    int m = recogStr.size();

    if (n == 0) {
      substitutions = 0;
      deletions     = 0;
      insertions    = m;
      distance      = m;
    } else if (m == 0) {
      substitutions = 0;
      deletions     = n;
      insertions    = 0;
      distance      = n;
    } else {
      // initialize the matrix
      matrix.resize(n+1);
      for (int i = 0; i < n+1; ++i) {
        matrix[i].resize(m+1);
        for (int j = 0; j < m+1; ++j) {
          matrix[i][j] = 0;
        }
      }
      for (int i = 0; i < n+1; ++i) {
        matrix[i][0] = i;
      }
      for (int j = 0; j < m+1; ++j) {
        matrix[0][j] = j;
      }

      // calculate the insertions, substitutions and deletions
      for (int i = 1; i < n+1; ++i) {
        int s_i = gtStr[i-1];
        for (int j = 1; j < m+1; ++j) {
          int t_j = recogStr[j-1];
          int cost = (s_i == t_j) ? 0 : 1;
          const int above = matrix[i-1][j];
          const int left  = matrix[i][j-1];
          const int diag  = matrix[i-1][j-1];
          const int cell  = std::min(above + 1, 
              std::min (left + 1, diag + cost));
          matrix[i][j] = cell;
        }
      }
      
      if (backtrace) {
        size_t i = n;
        size_t j = m;
        substitutions = 0;
        deletions     = 0;
        insertions    = 0;

        // backtracking
        while (i != 0 && j != 0) {
          if (matrix[i][j] == matrix[i-1][j-1]) {
            --i;
            --j;
          }
          else if (matrix[i][j] == matrix[i-1][j-1] + 1) {
            ++substitutions;
            --i;
            --j;
          }
          else if (matrix[i][j] == matrix[i-1][j] + 1) {
            ++deletions;
            --i;
          }
          else {
            ++insertions;
            --j;
          }
        }
        while (i != 0) {
          ++deletions;
          --i;
        }
        while (j != 0) {
          ++insertions;
          --j;
        }
        int diff = substitutions + deletions + insertions;
        // sanity check;
        if (diff != matrix[n][m] ) {
            std::cout << "Found path with distance " << diff << " but Levenshtein distance is " << matrix[n][m];
        }

        distance = (subPenalty*substitutions) + (delPenalty*deletions) + (insPenalty*insertions);
      }
      else {
        distance = matrix[n][m];
      }
    }
    //LOG(INFO) << substitutions << " " << deletions << " " << insertions;
    return distance;
  }


SSeqLEngLineInfor CSeqLEngLineBMSSWordDecoder::decodelinefunction_att(
        SSeqLEngLineInfor& inputLine) {
    set_attention_param();
    inputLine_ = inputLine;
    expand_candidate_att();
    decode_att();
    enghighlowoutput_att(outputLine_);
    return outputLine_;
}


SSeqLEngLineInfor CSeqLEngLineBMSSWordDecoder::decodeLineFunction( SSeqLEngLineInfor & inputLine ){
    inputLine_ = inputLine;
    if (_m_resnet_strategy_flag) {
        regWeight_ = 0.25; //0.2;
        cdWeight_ = 0.6; //0.4;
        edit_dis_weight_ = 2.4; //3.2;
    }

    expand_candidate();
    //问题定位到decode函数会抹掉部分标点
    decode();
    //std::cout<<"outputLine_.wordVec_.size():"<<outputLine_.wordVec_.size()<<std::endl;
    //std::cout<<"outputLine:";
    for(unsigned int i =0; i<outputLine_.wordVec_.size();i++){
        std::cout<<outputLine_.wordVec_[i].wordStr_<<" ";
    }
    std::cout<<std::endl;

    //_m_acc_eng_highlowtype_flag=0
    if (_m_acc_eng_highlowtype_flag) {
    //会将原来的大小写恢复
        engacchighlowoutput(outputLine_);
    } else {
        SeqLEngHighLowOutput(outputLine_);
    }

    return outputLine_;
}

double CSeqLEngLineBMSSWordDecoder::calculate_cand_prob(std::string cand_str, 
        SSeqLEngWordInfor word) {
    std::vector<int> cand_path = ((CSeqLEngWordReg *)recognizer_)->stringutf8topath(cand_str);
    std::vector<int> path = ((CSeqLEngWordReg *)recognizer_)->stringutf8topath(word.wordStr_);

    /*for (int i = 0; i < path.size(); i++) {
        std::cout << path[i] << " ";
    }
    std::cout << std::endl;

    for (int i = 0; i < cand_path.size(); i++) {
        std::cout << cand_path[i] << " ";
    }
    std::cout << std::endl;*/

    if (cand_path.size() <= 0 || path.size() <= 0) {
        return -100.0;
    }

    int row = cand_path.size() + 1;
    int col = path.size() + 1;
    std::vector<std::vector<int> > map(row);

    double cand_prob_log = 0.0;
    std::vector<float> probmat = word.proMat_;

    for (int i = 0; i < row; i++) {
        map[i].resize(col);
    }

    for (int i = 0; i < row; i++) {
        map[i][0] = i;
    }

    for (int j = 0; j < col; j++) {
        map[0][j] = j;
    }

    for (int i = 0; i < row - 1; i++) {
        for (int j = 0; j < col - 1; j++) {
            if (cand_path[i] == path[j]) {
                map[i + 1][j + 1] = map[i][j];
            }
            else {
                map[i + 1][j + 1] = map[i][j] + 1;
            }
            map[i + 1][j + 1] = std::min(map[i + 1][j + 1], map[i][j + 1] + 1);
            map[i + 1][j + 1] = std::min(map[i + 1][j + 1], map[i + 1][j] + 1);
        }
    }

    int row_id = row - 1;
    int col_id = col - 1;
    int last_cand_max_id = -1; // max_id
    //int last_cand_pos = -1;  // the pos in the prob_mat
    int last_err_num = 0;
    bool low_prob_flag = true; // whether use the 0.1 to neg the prob

    while (row_id > 0 && col_id > 0) {
        if (cand_path[row_id - 1] == path[col_id - 1]) {
            if (last_cand_max_id >= 0) {

                if (last_cand_max_id != word.maxVec_[col_id] && \
                        last_cand_max_id - 32 != word.maxVec_[col_id]) {
                    low_prob_flag = false;
                }

                int index = col_id * _labelsdim + (last_cand_max_id + 2);
                cand_prob_log += safeLog(probmat[index] > probmat[index - 32] ? \
                        probmat[index] : probmat[index - 32]);
                /*if (probmat[index] > probmat[index - 32])
                    std::cout << "cand id: " << last_cand_max_id + 2 << " " << probmat[index] << " " << col_id  << std::endl;
                else
                    std::cout << "cand id: " << last_cand_max_id - 30 << " " << probmat[index-32] << " " << col_id  << std::endl;*/
                // other chars given a small prob 0.1
                if (last_err_num > 1) {
                    cand_prob_log += safeLog(pow(0.1, (last_err_num - 1)));
                    low_prob_flag = false;
                }
            }
            last_err_num = 0;
            last_cand_max_id = cand_path[row_id - 1];
            row_id--;
            col_id--;
        }
        else {
            // sub error
            if (map[row_id][col_id] == map[row_id - 1][col_id - 1] + 1) {
                if (last_err_num == 0 && last_cand_max_id >= 0) {
                    if (last_cand_max_id != word.maxVec_[col_id] && \
                            last_cand_max_id - 32 != word.maxVec_[col_id]) {
                        low_prob_flag = false;
                    }
                    int index = col_id * _labelsdim + (last_cand_max_id + 2);
                    cand_prob_log += safeLog(probmat[index] > probmat[index - 32] ? \
                            probmat[index] : probmat[index - 32]);
                    /*if (probmat[index] > probmat[index - 32])
                        std::cout << "cand id: " << last_cand_max_id + 2 << " " << probmat[index] << " " << col_id  << std::endl;
                    else
                        std::cout << "cand id: " << last_cand_max_id - 30 << " " << probmat[index-32] << " " << col_id  << std::endl;*/
                }
                last_err_num += 1;
                last_cand_max_id = cand_path[row_id - 1];
                row_id--;
                col_id--;
                //std::cout << "sub" << std::endl;
            }
            // insert error
            else if (map[row_id][col_id] == map[row_id - 1][col_id] + 1) {
                if (last_err_num == 0 && last_cand_max_id >= 0) {
                    if (last_cand_max_id != word.maxVec_[col_id] && \
                            last_cand_max_id - 32 != word.maxVec_[col_id]) {
                        low_prob_flag = false;
                    }
                    int index = col_id * _labelsdim + (last_cand_max_id + 2);
                    cand_prob_log += safeLog(probmat[index] > probmat[index - 32] ? \
                            probmat[index] : probmat[index - 32]);
                    /*if (probmat[index] > probmat[index - 32])
                        std::cout << "cand id: " << last_cand_max_id + 2 << " " << probmat[index] << " " << col_id  << std::endl;
                    else
                        std::cout << "cand id: " << last_cand_max_id - 30 << " " << probmat[index-32] << " " << col_id  << std::endl;*/
                }
                last_err_num += 1;
                last_cand_max_id = cand_path[row_id - 1];
                row_id--;
                //std::cout << "ins" << std::endl;
            }
            //del error
            else if (map[row_id][col_id] == map[row_id][col_id - 1] + 1) {
                last_err_num += 1;
                col_id--;
                //std::cout << "del" << std::endl;
            }
        }
    }
    if (col_id > 0) {
        last_err_num += col_id;
    }
    else if (row_id > 0) {
        last_err_num += row_id;
        last_cand_max_id = cand_path[0];
    }

    if (last_cand_max_id != word.maxVec_[0] && last_cand_max_id - 32 != word.maxVec_[0]) {
        low_prob_flag = false;
    }
    int index = (last_cand_max_id + 2);
    cand_prob_log += safeLog(probmat[index] > probmat[index-32] ? \
            probmat[index] : probmat[index-32]);
    /*if (probmat[index] > probmat[index - 32])
        std::cout << "cand id: " << last_cand_max_id + 2 << " " << probmat[index] << " " << col_id  << std::endl;
    else
        std::cout << "cand id: " << last_cand_max_id - 30 << " " << probmat[index-32] << " " << col_id  << std::endl;*/
    if (last_err_num > 1) {
        cand_prob_log += safeLog(pow(0.1, (last_err_num - 1)));
        low_prob_flag = false;
    }
    if (low_prob_flag) {
        cand_prob_log += safeLog(0.0001);
    }

    return cand_prob_log;
}

void CSeqLEngLineBMSSWordDecoder::expand_candidate_att() {
    candisVec_.clear();
    candisVec_.resize(inputLine_.wordVec_.size());
    int word_size = inputLine_.wordVec_.size();

    for (int i = 0; i < inputLine_.wordVec_.size(); ++i) {
        vector<pair<string, double> > output;
        output.clear();
        pthread_t thread_id = pthread_self();
        SSeqLEngWordInfor word = inputLine_.wordVec_[i];
        SSeqLEngWordCandis & candidate = candisVec_[i];

        if (word.wordStr_.length() >= 3 && word.toBeAdjustCandi_) {
            espModel_->esp_get((threadType)thread_id, word.wordStr_.c_str(), \
                    output, 2, spellCheckNum_);
        }

        candidate.strVec_.push_back(word.wordStr_);
        candidate.regLogProbVec_.push_back(word.regLogProb_);
        if (word.main_line_) {
            word.acc_score_ += 0.1;
        }

        //candidate.checkScoreVec_.push_back(0.1 * word.wordStr_.size());
        //candidate.edit_dis_vec_.push_back(0.0);
        
        if (output.size() != 0) {
            candidate.checkScoreVec_.push_back(-0.1 + word.acc_score_);
        } else {
            candidate.checkScoreVec_.push_back(0.1 * word.wordStr_.size());
        }
        candidate.edit_dis_vec_.push_back(0.0);

        for (int j = 0; j < output.size(); ++j) {
            int edit_dis = stringAlignment(candidate.strVec_[0], output[j].first);
            if (edit_dis == 0) {
                candidate.checkScoreVec_[0] = -0.1 * j + 0.5 + word.acc_score_; 
                continue;
            }
            //if (word.regLogProb_ < -1.8) { // too small cand prob to calucate the word
            //    break;
            //}
            candidate.strVec_.push_back(output[j].first);

            // define the candidate word prob is the times of the last possible recognition
            //double cand_prob = safeLog(1.0 - safeExp(word.regLogProb_)) - safeLog(69.0); // 95 char and remove big small
            double cand_prob = calculate_cand_prob(output[j].first, word);
            //std::cout << "Cand: " << output[j].first << " --- " << word.wordStr_ << " " << cand_prob << std::endl;

            candidate.regLogProbVec_.push_back(cand_prob); 

            candidate.checkScoreVec_.push_back(-0.1 * j);
            //std::cout << edit_dis << " " << candidate.checkScoreVec_[candidate.checkScoreVec_.size()-1] << std::endl;
            candidate.edit_dis_vec_.push_back(edit_dis * -1.0 / (candidate.strVec_[0].size()));
        }
        candidate.candiNum_ = candidate.strVec_.size();
        /*std::cout << "candisVec_ id: " << i << std::endl;
        for (int k = 0; k < candidate.candiNum_; ++k) {
            std::cout << "cand str: " << candidate.strVec_[k];
            std::cout << " reg prob: " << candidate.regLogProbVec_[k];
            std::cout << " check score: " << candidate.checkScoreVec_[k];
            std::cout << " edit dis: " << candidate.edit_dis_vec_[k] << std::endl;
        }*/
    }
}

void CSeqLEngLineBMSSWordDecoder::expand_candidate() {
    candisVec_.clear();
//std::cout<<"inputLine_.wordVec_.size():"<<inputLine_.wordVec_.size()<<std::endl;
    candisVec_.resize(inputLine_.wordVec_.size());

    for (int i = 0; i < inputLine_.wordVec_.size(); ++i) {
        vector<pair<string, double> > output;
        output.clear();
        pthread_t threadID = pthread_self();
        SSeqLEngWordInfor word = inputLine_.wordVec_[i];
        SSeqLEngWordCandis & candidate = candisVec_[i];

        //if (inputLine_.wordVec_.size() > 1 && word.toBeAdjustCandi_) {
        //    espModel_->esp_get((threadType)threadID, word.wordStr_.c_str(), output, 2, spellCheckNum_);
        //}
        //else
        //spellCheckNum_=5,单词长度超过3，output的size就会是5
        if (word.wordStr_.length() >= 3 && word.toBeAdjustCandi_) {
            espModel_->esp_get((threadType)threadID, word.wordStr_.c_str(), output, 2, spellCheckNum_);
        }

        candidate.strVec_.push_back(word.wordStr_);

        //std::cout<<"word regLogProb:"<<word.regLogProb_<<std::endl;
        candidate.regLogProbVec_.push_back(word.regLogProb_);
        //candidate.checkScoreVec_.push_back(1.0 / (output.size() + 1));
        //std::cout<<"output.size():"<<output.size()<<std::endl;
        if (output.size() != 0) {
            candidate.checkScoreVec_.push_back(-0.1);
        } else {
        //std::cout<<"word.wordStr_.size():"<<word.wordStr_.size()<<std::endl;
            candidate.checkScoreVec_.push_back(0.1 * word.wordStr_.size());
        }
        //candidate.edit_dis_vec_.push_back(1.0);
        candidate.edit_dis_vec_.push_back(0.0);

        for (int j = 0; j < output.size(); ++j) {
        //std::cout<<"candidate.strVec_[0]:"<<candidate.strVec_[0]<<"  output[j].first:"<<output[j].first<<std::endl;
            int edit_dis = stringAlignment(candidate.strVec_[0], output[j].first);
        //std::cout<<"edit_dis:"<<edit_dis<<std::endl;
            if (edit_dis == 0) {
                candidate.checkScoreVec_[0] = -0.1 * j + 0.5; 
                //candidate.checkScoreVec_[0] = 1.0 - 0.1 * j; 
                continue;
            }
            candidate.strVec_.push_back(output[j].first);
        //std::cout<<"word.proMat_[0]:"<<word.proMat_[0]<<"  word.proMat_.size():"<<word.proMat_.size()<<std::endl;
            candidate.regLogProbVec_.push_back(((CSeqLEngWordReg *)recognizer_)->onePrefixPathLogProb(output[j].first,
                    &(word.proMat_[0]), word.proMat_.size())); 
            //candidate.checkScoreVec_.push_back(1.0 - 0.1 * j);
            candidate.checkScoreVec_.push_back(-0.1 * j);
            //candidate.edit_dis_vec_.push_back(1.0 - edit_dis * 1.0 / (candidate.strVec_[0].size()));
            candidate.edit_dis_vec_.push_back(edit_dis * -1.0 / (candidate.strVec_[0].size()));
        }
        candidate.candiNum_ = candidate.strVec_.size();
       // for (int k = 0; k < candidate.candiNum_; ++k) {
         //   std::cout << "cand str: " << candidate.strVec_[k];
           // std::cout << " reg prob: " << candidate.regLogProbVec_[k];
           // std::cout << " check score: " << candidate.checkScoreVec_[k];
            //std::cout << " edit dis: " << candidate.edit_dis_vec_[k] << std::endl;
       // }
    }
}

void CSeqLEngLineBMSSWordDecoder::extractCandisVector( ){
   SSeqLEngLineInfor & inputLine = inputLine_;
   candisVec_.clear();
   candisVec_.resize( inputLine.wordVec_.size());
  
   vector<pair<string, double> > output;
   pthread_t threadID = pthread_self();

   for(unsigned int i=0; i<inputLine.wordVec_.size(); i++){
     //std::cout << " extractCandisVector i " << i << "size " << inputLine.wordVec_.size() <<  std::endl;
     
     SSeqLEngWordInfor tempWord = inputLine.wordVec_[i];

     loc_rm_quote(tempWord);

     char endChar = tempWord.wordStr_[tempWord.wordStr_.length()-1];
     bool endPunctFlag = SeqLLineDecodeIsPunct(endChar) && tempWord.wordStr_.length()>1;

     SSeqLEngWordCandis & tempCandi = candisVec_[i] ;
     if(!endPunctFlag){
       if(inputLine_.wordVec_.size()>1 ){
        transform(tempWord.wordStr_.begin(), tempWord.wordStr_.end(), tempWord.wordStr_.begin(), ::tolower);
        espModel_->esp_get( (threadType)threadID, tempWord.wordStr_.c_str(), output, 2, spellCheckNum_ );
        tempCandi.candiNum_ = std::min( (unsigned int)(output.size()), spellCheckNum_) + 1;
       }
       else if(inputLine.wordVec_[0].wordStr_.length()>5 ){
        transform(tempWord.wordStr_.begin(), tempWord.wordStr_.end(), tempWord.wordStr_.begin(), ::tolower);
        espModel_->esp_get( (threadType)threadID, tempWord.wordStr_.c_str(), output, 2, 1 );
        tempCandi.candiNum_ = std::min( (unsigned int)(output.size()), spellCheckNum_) + 1;
        output.clear();
       }
       else{
        transform(tempWord.wordStr_.begin(), tempWord.wordStr_.end(), tempWord.wordStr_.begin(), ::tolower);
        tempCandi.candiNum_ =  1;
        output.clear();
       }
     }
     else
     {
       tempWord.wordStr_.erase(tempWord.wordStr_.length()-1,1);
       if(inputLine_.wordVec_.size()>1 ){
        transform(tempWord.wordStr_.begin(), tempWord.wordStr_.end()-1, tempWord.wordStr_.begin(), ::tolower);
        espModel_->esp_get( (threadType)threadID, tempWord.wordStr_.c_str(), output, 2, spellCheckNum_ );
        tempCandi.candiNum_ = std::min( (unsigned int)(output.size()), spellCheckNum_) + 1;
       }
       else if(inputLine.wordVec_[0].wordStr_.length()>5 ){
        transform(tempWord.wordStr_.begin(), tempWord.wordStr_.end()-1, tempWord.wordStr_.begin(), ::tolower);
        espModel_->esp_get( (threadType)threadID, tempWord.wordStr_.c_str(), output, 2, 1 );
        tempCandi.candiNum_ = std::min( (unsigned int)(output.size()), spellCheckNum_) + 1;
        output.clear();
       }
       else{
        transform(tempWord.wordStr_.begin(), tempWord.wordStr_.end(), tempWord.wordStr_.begin(), ::tolower);
        tempCandi.candiNum_ =  1;
        output.clear();
       }
     }


     tempCandi.strVec_.resize( tempCandi.candiNum_);
     tempCandi.rmPunctStrVec_.resize( tempCandi.candiNum_);
     tempCandi.regLogProbVec_.resize( tempCandi.candiNum_);
     tempCandi.checkScoreVec_.resize( tempCandi.candiNum_);
     tempCandi.editDistVec_.resize( tempCandi.candiNum_);
     tempCandi.highLowTypeVec_.resize( tempCandi.candiNum_);
     tempCandi.quote_pos_vec_.resize(tempCandi.candiNum_);
    
    for (int j = 0; j < tempCandi.candiNum_; j++) {
        tempCandi.quote_pos_vec_[j] = tempWord.quote_pos_;
    }

     if(endPunctFlag){
        tempCandi.rmPunctStrVec_[0] = tempWord.wordStr_;
        tempCandi.strVec_[0] = tempWord.wordStr_;
        tempCandi.strVec_[0] += endChar;
     }
     else
     {
        tempCandi.rmPunctStrVec_[0] = tempWord.wordStr_;
        tempCandi.strVec_[0] = tempWord.wordStr_;
     }

     tempCandi.editDistVec_[0] = 0;
     tempCandi.regLogProbVec_[0] = tempWord.regLogProb_;
     tempCandi.checkScoreVec_[0] = 0.001;
     tempCandi.checkScore_ = 0.001;
     if(output.size() > 0){
       if(tempCandi.rmPunctStrVec_[0] == output[0].first ){
         tempCandi.checkScoreVec_[0] = output[0].second + 0.01;
       }
       else
       {
         tempCandi.checkScoreVec_[0] = output[0].second;
       }

       if(tempCandi.checkScoreVec_[0] <= output[0].second ){
         tempCandi.checkScoreVec_[0] = output[0].second + 0.00001;
       }
     }
     /*if(output.size() > 0){
       //tempCandi.checkScoreVec_[0] += output[0].second + 0.7;//0.00001;
       tempCandi.checkScoreVec_[0] += output[0].second + 0.1;//0.00001;
       tempCandi.checkScore_ = tempCandi.checkScoreVec_[0];
     }
     else{
       tempCandi.checkScoreVec_[0] = 0.1;//0.00001;
     }
*/
     double saveScore0 = tempCandi.checkScoreVec_[0];
     tempCandi.checkScoreVec_[0] =1;
     if(output.size() > 0){
       if(tempCandi.rmPunctStrVec_[0] == output[0].first ){
         tempCandi.checkScoreVec_[0] += 0.05;
       }
     }

      
     tempCandi.regLogProbVec_[0] =((CSeqLEngWordReg *) recognizer_)->onePrefixPathUpLowerLogProb(tempCandi.strVec_[0], \
               &(tempWord.proMat_[0]), tempWord.proMat_.size(), tempCandi.highLowTypeVec_[0] ); 
     //std::cout << tempCandi.strVec_[0] <<" " << tempCandi.regLogProbVec_[0] << std::endl;
       
 /*    if(inputLine.wordVec_.size()==1 && tempCandi.regLogProbVec_[0]>-0.3){
         tempCandi.candiNum_ = 1;
    }*/

     //std::cout << "============" << tempCandi.strVec_[0] << " == " << tempCandi.checkScoreVec_[0] <<std::endl;
     for( unsigned int j=1; j<tempCandi.candiNum_; j++){
        if(endPunctFlag){
            tempCandi.rmPunctStrVec_[j] = output[j-1].first;
            tempCandi.strVec_[j] = output[j-1].first;
            tempCandi.strVec_[j] += endChar;
        }
        else
        {
            tempCandi.rmPunctStrVec_[j] = output[j-1].first;
            tempCandi.strVec_[j] = output[j-1].first;
        }
       tempCandi.regLogProbVec_[j] =((CSeqLEngWordReg *) recognizer_)->onePrefixPathUpLowerLogProb(tempCandi.strVec_[j], \
               &(tempWord.proMat_[0]), tempWord.proMat_.size(), tempCandi.highLowTypeVec_[j] ); 

       //tempCandi.checkScoreVec_[j] = output[j-1].second/saveScore0;
       tempCandi.checkScoreVec_[j] = 1.0 - stringAlignment( tempCandi.strVec_[0], tempCandi.strVec_[j]) * 1.0 / \
                                     (tempCandi.strVec_[0].size()+5);
       tempCandi.editDistVec_[j] = stringAlignment( tempCandi.strVec_[0], tempCandi.strVec_[j]);

       //std::cout << j << " " << tempCandi.strVec_[j] << " " << tempCandi.regLogProbVec_[j] << std::endl;
     }

     //tempCandi.candiNum_ = 1;

   }
   //std::cout << "==== end extractCandisVector " << std::endl;
}

// For debug
void CSeqLEngLineBMSSWordDecoder::catCandisVectorToLine(SSeqLEngLineInfor & outputLine){
    outputLine = inputLine_;
    outputLine.wordVec_.clear();
    for(unsigned int i=0; i<inputLine_.wordVec_.size(); i++){
        //std::cout << "--- i ---" << i << std::endl;
        SSeqLEngWordCandis & tempCandis = candisVec_[i];
        for(unsigned int j=0; j<tempCandis.candiNum_; j++){
          SSeqLEngWordInfor tempWord = inputLine_.wordVec_[i];
          tempWord.wordStr_ = tempCandis.strVec_[j];
          tempWord.regLogProb_ = tempCandis.regLogProbVec_[j];
          outputLine.wordVec_.push_back(tempWord);
        }
    }
}

void CSeqLEngLineBMSSWordDecoder :: initDecodeNodes( ){
    nodeVec_.clear();
    // step 1: init the nodeVec_
    int initNodeNum = calInitNodeNum();
    nodeVec_.resize(initNodeNum);
    assert(nodeVec_.size() == initNodeNum );
    for(unsigned int i=0; i< nodeVec_.size(); i++){
        nodeVec_[i].lmHist_ = NULL;
        nodeVec_[i].lmHist_ = new LangModel::CalIterWrapper(LangModel::CalType_CompactTrie);
    }
    std::cout<< " nodeVec_.size() " << nodeVec_.size() << std::endl;
}

void CSeqLEngLineBMSSWordDecoder :: extendDecodeNodes( ){
    int orgNum  =  nodeVec_.size();
    int newNum = orgNum + nodeExtStep_;
    nodeVec_.resize( newNum );
    assert(newNum == nodeVec_.size());
    for(unsigned int i=orgNum; i<newNum; i++){
      nodeVec_[i].lmHist_ = NULL;
      nodeVec_[i].lmHist_ = new LangModel::CalIterWrapper( LangModel::CalType_CompactTrie);
    }

}

void CSeqLEngLineBMSSWordDecoder :: releaseDecodeNodes( ){
    for(unsigned int i=0; i<nodeVec_.size(); i++){
        delete nodeVec_[i].lmHist_;
        nodeVec_[i].lmHist_=NULL;
    }
    nodeVec_.clear();
    std::vector<SSeqLEngDecodeNode> tempVec = nodeVec_;
    nodeVec_.swap(tempVec); 
}

void CSeqLEngLineBMSSWordDecoder :: calLeftRightTopBottom( ){
    minLeft_ = inputLine_.wordVec_[0].left_;
    maxRight_ = inputLine_.wordVec_[0].left_ + inputLine_.wordVec_[0].wid_;
    minTop_ = inputLine_.wordVec_[0].top_;
    maxBottom_ = inputLine_.wordVec_[0].top_ + inputLine_.wordVec_[0].hei_;
    
    for(unsigned int i=1; i<inputLine_.wordVec_.size(); i++){
      if(inputLine_.wordVec_[i].wordStr_=="") continue;
      minLeft_ = std::min(minLeft_, inputLine_.wordVec_[i].left_);
      maxRight_ = std::max(maxRight_, inputLine_.wordVec_[i].left_ + inputLine_.wordVec_[i].wid_);
      minTop_  = std::min(minTop_, inputLine_.wordVec_[i].top_);
      maxBottom_ = std::max(maxBottom_, inputLine_.wordVec_[i].top_ + inputLine_.wordVec_[i].hei_);
    }
    lineHei_ = maxBottom_ - minTop_;
    lineWid_ = maxRight_ - minLeft_;
}
bool CSeqLEngLineBMSSWordDecoder :: havePathRelations( SSeqLEngDecodeNode & node,\
        SSeqLEngWordInfor & word ){
 // if( fabs(node.endPos_ - word.left_)< ( 1.5*lineHei_) ){
   if( (node.endPos_<= (word.left_-0.0*lineHei_)  && node.endPos_+ 1.5*lineHei_> word.left_) /*  || \
      node.endPos_>= word.left_  && node.endPos_-0.2*lineHei_<word.left_*/ ){
    return true;
  }
  else
  {
    return false;
  }
}

bool is_punc(std::string test_str) {
    if (test_str.size() == 1 && !(
            (test_str[0] >= '0' && test_str[0] <= '9') ||
            (test_str[0] >= 'a' && test_str[0] <= 'z') ||
            (test_str[0] >= 'A' && test_str[0] <= 'Z'))) {
        return true;
    }
    return false;
}

bool CSeqLEngLineBMSSWordDecoder :: havePathRelations( int wordIndex1, int wordIndex2){
    int wordRight1 = inputLine_.wordVec_[wordIndex1].left_ + inputLine_.wordVec_[wordIndex1].wid_;
    int wordLeft2 = inputLine_.wordVec_[wordIndex2].left_;
    //if(wordLeft2 < wordRight1 /*-0.2*spaceConf_.meanBoxSpace_*/){
     if(wordLeft2 < wordRight1 - 0.2*spaceConf_.avgCharWidth_){
      return false;
    }
    vector<int> insertLeftVec;
    for(unsigned int i=0; i<inputLine_.wordVec_.size(); i++){
      if( (inputLine_.wordVec_[i].left_ > wordRight1) && \
              inputLine_.wordVec_[i].left_ < wordLeft2 ){
          bool haveInsertd = false;
          for(unsigned int j=0; j<insertLeftVec.size(); j++){
            if(insertLeftVec[j] == inputLine_.wordVec_[i].left_){
              haveInsertd = true;
              break;
            }
          }
          if(!haveInsertd){
            insertLeftVec.push_back(inputLine_.wordVec_[i].left_);
          }
      }
    }

    //std::cout << " spaceConf_.meanBoxSpace_ " << 5.5*spaceConf_.meanBoxSpace_ << " lineHei_ " << 0.8*lineHei_ << std::endl;
    //if(insertLeftVec.size()<1 || (wordLeft2 -wordRight1 )< 3.5*spaceConf_.meanBoxSpace_ ){
    //if( /*insertLeftVec.size()<1 ||*/ (wordLeft2 -wordRight1 )< 5.5*spaceConf_.meanBoxSpace_ ){
     if( insertLeftVec.size()<1  || (wordLeft2 -wordRight1 )< 1.5*spaceConf_.avgCharWidth_ ){
    //if( insertLeftVec.size()<1 || (wordLeft2 -wordRight1 )< 0*spaceConf_.avgCharWidth_ ){
//    if( insertLeftVec.size()<1 || (wordLeft2 -wordRight1 )< 0.8*lineHei_ ){
      return true;
    }
    else
    {
      return false;
    }
}

bool CSeqLEngLineBMSSWordDecoder::havepathrelations_att(int wordIndex1, int wordIndex2) {
    int wordright1 = inputLine_.wordVec_[wordIndex1].left_ + inputLine_.wordVec_[wordIndex1].wid_;
    int wordleft2 = inputLine_.wordVec_[wordIndex2].left_;
    //if(wordLeft2 < wordRight1 /*-0.2*spaceConf_.meanBoxSpace_*/){
    if (wordleft2 < wordright1 - 0.2 * spaceConf_.avgCharWidth_) {
        return false;
    }
    vector<int> insertleftvec;
    bool absloute_insert = false;
    for (unsigned int i = 0; i < inputLine_.wordVec_.size(); i++) {
        if ((inputLine_.wordVec_[i].left_ > wordright1 - 0.2 * spaceConf_.avgCharWidth_) && \
              inputLine_.wordVec_[i].left_ < wordleft2 && (
              inputLine_.wordVec_[i].left_ + inputLine_.wordVec_[i].wid_ > wordright1 && \
              inputLine_.wordVec_[i].left_ + inputLine_.wordVec_[i].wid_ < wordleft2 + \
              0.2*spaceConf_.avgCharWidth_)){

            if (inputLine_.wordVec_[i].left_ >= wordright1 && 
                  inputLine_.wordVec_[i].left_ + inputLine_.wordVec_[i].wid_ <= wordleft2) {
                absloute_insert = true;
            }

            bool haveinsertd = false;
            for (unsigned int j = 0; j < insertleftvec.size(); j++) {
                if (insertleftvec[j] == inputLine_.wordVec_[i].left_) {
                    haveinsertd = true;
                    break;
                }
            }
            if (!haveinsertd) {
                insertleftvec.push_back(inputLine_.wordVec_[i].left_);
            }
        }
    }
    /*std::cout << wordIndex1 << " " << inputLine_.wordVec_[wordIndex1].wordStr_  << " "
        << wordIndex2 << " " << inputLine_.wordVec_[wordIndex2].wordStr_  << " "
        << wordRight1 << " " << wordLeft2 << " " << insertLeftVec.size() << std::endl;*/

    if (insertleftvec.size() < 1) {
        return true;
    }
    else if (absloute_insert) {
        return false;
    }
    else if ((wordleft2 -wordright1) < 1.5*spaceConf_.avgCharWidth_) {
        return true;
    }
    else
    {
        return false;
    }
}

bool CSeqLEngLineBMSSWordDecoder::havepathrelations(int wordIndex1, int wordIndex2) {
    int wordright1 = inputLine_.wordVec_[wordIndex1].left_ + inputLine_.wordVec_[wordIndex1].wid_;
    int wordleft2 = inputLine_.wordVec_[wordIndex2].left_;
    //if(wordLeft2 < wordRight1 /*-0.2*spaceConf_.meanBoxSpace_*/){
    //std::cout << "compare str: " << inputLine_.wordVec_[wordIndex1].wordStr_ << " " << inputLine_.wordVec_[wordIndex2].wordStr_ << std::endl;
    //std::cout << "left-right: " << wordleft2 << " " <<  wordright1 << " " << spaceConf_.avgCharWidth_ << std::endl;
    if (wordleft2 < wordright1 - 0.2 * spaceConf_.avgCharWidth_) {
        return false;
    }
    if (wordleft2 - wordright1 > std::max(inputLine_.wordVec_[wordIndex1].wid_ + 
                inputLine_.wordVec_[wordIndex2].wid_, (int)(10 * spaceConf_.avgCharWidth_))) {
        return false;
    }
    vector<int> insertleftvec;
    bool absloute_insert = false;
    for (unsigned int i = 0; i < inputLine_.wordVec_.size(); i++) {
        if ((inputLine_.wordVec_[i].left_ > wordright1 - 0.2 * spaceConf_.avgCharWidth_) && \
              inputLine_.wordVec_[i].left_ < wordleft2 && (
              inputLine_.wordVec_[i].left_ + inputLine_.wordVec_[i].wid_ > wordright1 && \
              inputLine_.wordVec_[i].left_ + inputLine_.wordVec_[i].wid_ < wordleft2 + \
              0.2*spaceConf_.avgCharWidth_)){

            if (inputLine_.wordVec_[i].left_ >= wordright1 && 
                  inputLine_.wordVec_[i].left_ + inputLine_.wordVec_[i].wid_ <= wordleft2 &&
                  inputLine_.wordVec_[i].wordStr_ != ".") {
                absloute_insert = true;
            }

            bool haveinsertd = false;
            for (unsigned int j = 0; j < insertleftvec.size(); j++) {
                if (insertleftvec[j] == inputLine_.wordVec_[i].left_) {
                    haveinsertd = true;
                    break;
                }
            }
            if (!haveinsertd) {
                insertleftvec.push_back(inputLine_.wordVec_[i].left_);
            }
        }
    }
    /*std::cout << wordIndex1 << " " << inputLine_.wordVec_[wordIndex1].wordStr_  << " "
        << wordIndex2 << " " << inputLine_.wordVec_[wordIndex2].wordStr_  << " "
        << wordRight1 << " " << wordLeft2 << " " << insertLeftVec.size() << std::endl;*/

    if (insertleftvec.size() < 1) {
        return true;
    }
    else if (absloute_insert) {
        return false;
    }
    else if ((wordleft2 -wordright1) < 1.5*spaceConf_.avgCharWidth_) {
        return true;
    }
    else
    {
        return false;
    }
}

bool CSeqLEngLineBMSSWordDecoder :: isStartWord(SSeqLEngWordInfor & word, int leftSpace){
  if( (word.left_ - minLeft_)< leftSpace ){
  //if( (word.left_ - minLeft_)< 3){
     return true;
  }
  else{
     return false;
  }
}

bool CSeqLEngLineBMSSWordDecoder :: isEndWord(SSeqLEngWordInfor & word, int rightSpace){
  if( (maxRight_ - (word.left_+word.wid_) )< rightSpace){
  //if( (maxRight_ - (word.left_+word.wid_) )< 3){
     return true;
  }
  else{
     return false;
  }
}

void CSeqLEngLineBMSSWordDecoder :: printPath( int thisNode){
    //std::cout << "Path i " << thisNode << " score: " \
        <<nodeVec_[thisNode].score_ << " acLmScore " << nodeVec_[thisNode].acLmScore_ \
        << " acLmScore0 " << nodeVec_[thisNode].acLmScore0_ \
        << " acMulRegScore_  " << nodeVec_[thisNode].acMulRegScore_ \
        <<" acCdScore " << nodeVec_[thisNode].acCdScore_ << " acSpScore " << nodeVec_[thisNode].acSpScore_ \
        <<" acSpScore2 " << nodeVec_[thisNode].acSpScore2_ << " acRegScore " << nodeVec_[thisNode].acRegScore_ \
        <<" parent node " << nodeVec_[thisNode].parent_  \
        << " STR: ";
    std::cout << "path " << thisNode << " score: " << nodeVec_[thisNode].score_;
    std::cout << " acLmScore: " << nodeVec_[thisNode].acLmScore_;
    std::cout << " acLmScore0: " << nodeVec_[thisNode].acLmScore0_;
    std::cout << " acCdScore: " << nodeVec_[thisNode].acCdScore_;
    std::cout << " ac_edit_dis_score: " << nodeVec_[thisNode].ac_edit_dis_score_;
    std::cout << " acRegScore: " << nodeVec_[thisNode].acRegScore_;
    std::cout << " parent node: " << nodeVec_[thisNode].parent_;
    std::cout << " STR: ";
    SSeqLEngLineInfor tempLine;
    tempLine.wordVec_.resize( nodeVec_[thisNode].depth_);

    int index = nodeVec_[thisNode].depth_ - 1;
    while(true){
      if(thisNode < 0){
          break;
      }    
      int wordIndex = nodeVec_[thisNode].comp_;
      int candiIndex = nodeVec_[thisNode].candi_;
      tempLine.wordVec_[index] = inputLine_.wordVec_[wordIndex];
      tempLine.wordVec_[index].wordStr_ = candisVec_[wordIndex].strVec_[candiIndex];
      thisNode = nodeVec_[thisNode].parent_;
      index--;
    }

    for(unsigned int i=0; i<tempLine.wordVec_.size(); i++){
      std::cout << tempLine.wordVec_[i].wordStr_ << " ";
    }
    std::cout << std::endl;

}


void CSeqLEngLineBMSSWordDecoder :: trackBestPathToOutput( int nodeNum ){
 /*   
    int endNodeRightSpace = 1.0*spaceConf_.meanBoxWidth_;
    if( (inputLine_.left_+inputLine_.wid_)> (inputLine_.imgWidth_-spaceConf_.meanBoxWidth_)){
        endNodeRightSpace = 1.0*spaceConf_.meanBoxWidth_;
    }
   */ 
    int endNodeRightSpace = 0.5*spaceConf_.avgCharWidth_;

    // find the max score path
    outputLine_.left_ = inputLine_.left_;
    outputLine_.top_ = inputLine_.top_;
    outputLine_.wid_ = inputLine_.wid_;
    outputLine_.hei_ = inputLine_.hei_;
    outputLine_.imgWidth_ = inputLine_.imgWidth_;
    outputLine_.imgHeight_ = inputLine_.imgHeight_;
    //std::cout << "track input " << inputLine_.imgWidth_ << " " << inputLine_.imgHeight_ << std::endl;
    //std::cout << "track input " << outputLine_.imgWidth_ << " " << outputLine_.imgHeight_ << std::endl;

    int maxNode = -1;
    double maxScore = 0;
    for(int i=nodeNum-1; i>=0; i--){
        //std::cout << "trackBestPathToOutput i" << i << std::endl;
        //printPath(i);

       int wordIndex = nodeVec_[i].comp_;
       if( isEndWord( inputLine_.wordVec_[wordIndex], endNodeRightSpace) ){
         
         if(maxNode<0){
           maxNode = i;
           maxScore = nodeVec_[i].score_;
         }
         else if( maxScore < nodeVec_[i].score_ ){
           maxNode = i;
           maxScore = nodeVec_[i].score_;
         }
       }      
    }
    if(nodeNum>0 && maxNode==-1){
        //std::cerr << "nodeNum>0 && maxNode==-1" <<std::endl;
        for(int i=nodeNum-1; i>=0; i--){
            if(maxNode<0){
                maxNode = i;
                maxScore = nodeVec_[i].score_;
            }
            else if( maxScore < nodeVec_[i].score_ ){
                maxNode = i;
                maxScore = nodeVec_[i].score_;
    
            }
        }
    }

    std::cout << "trackBestPathToOutput maxNode " << maxNode << " maxScore " << maxScore << std::endl; 
    printPath(maxNode);
    // cat the word result to output line
    if(maxNode >=0 )
    {
        outputLine_.wordVec_.resize(nodeVec_[maxNode].depth_);
        int this_node = maxNode;
        int index = nodeVec_[maxNode].depth_ - 1;
        while (true) {
            if (this_node < 0) {
                break;
            }    
            int word_index = nodeVec_[this_node].comp_;
            int cand_index = nodeVec_[this_node].candi_;
            outputLine_.wordVec_[index] = inputLine_.wordVec_[word_index];
            outputLine_.wordVec_[index].wordStr_ = candisVec_[word_index].strVec_[cand_index];
            /*
            std::cout << "index: " << index << std::endl;
            std::cout << "string: " << outputLine_.wordVec_[index].wordStr_ << std::endl;
            std::cout << "word index: " << wordIndex << std::endl;
            std::cout << "word vec size: " << outputLine_.wordVec_.size() << std::endl;
            std::cout << "candis vec size: " << candisVec_.size() << std::endl;
            std::cout << "candi index: " << candiIndex << std::endl;
            std::cout << "high low size: " << candisVec_[wordIndex].highLowTypeVec_.size() << std::endl;*/
            //outputLine_.wordVec_[index].highLowType_ = candisVec_[wordIndex].highLowTypeVec_[candiIndex];
            //outputLine_.wordVec_[index].quote_pos_ = candisVec_[wordIndex].quote_pos_vec_[candiIndex];
            this_node = nodeVec_[this_node].parent_;
            index--;
        }
    }
    else
    {
      outputLine_.wordVec_.clear();
    }
}

bool bigger_score_first(const SSeqLEngDecodeNode & n1, const SSeqLEngDecodeNode & n2 ){
  return n1.score_ > n2.score_;
}

double calCdScoreFun(double score){
  return score - 1 ;
  //return log(score);
}

void CSeqLEngLineBMSSWordDecoder::decode() {
    outputLine_.wordVec_.clear();
    if(candisVec_.size() == 0) {
        return;
    }
    //init
    calLeftRightTopBottom();
    initDecodeNodes();

    int start_node_left_space = 0.5 * spaceConf_.avgCharWidth_;

    int node_num = 0;
    int start_node = -1;
    //std::vector<SSeqLEngWordInfor> tmp_word_vec;
    
    //decode
    //std::cout << "candisVec_.size():" << candisVec_.size() << std::endl; 
    for (int i = 0; i < candisVec_.size(); ++i) {  // wordVec_
        start_node = node_num;
        SSeqLEngWordCandis & this_candidate = candisVec_[i];
        SSeqLEngWordInfor  & this_word = inputLine_.wordVec_[i];
    
    /*std::cout<<"this_candidate:"<<std::endl;
    for (unsigned int i = 0; i < this_candidate.strVec_.size(); i++)
    {
        std::cout << this_candidate.strVec_[i] << " ";
    }
        std::cout << std::endl;

    std::cout<<"this_word:"<<std::endl;
    std::cout<<this_word.wordStr_<<std::endl;*/
    
        int this_end_pos = this_word.left_ + this_word.wid_;
        for (int j = start_node - 1; j >= 0; --j) {  // history path
            //std::cout << "i and j: " << i << " " << j << std::endl;
            //std::cout << "node num: " << node_num << " start node: " << start_node << std::endl;
            if (!havepathrelations(nodeVec_[j].comp_, i)) {
                //std::cout << "no relation, comp and i: " << nodeVec_[j].comp_ << std::endl;
                continue;
            }
            //if (this_candidate.candiNum_ == 1) {
            //    tmp_word_vec.push_back(this_word);
            //    continue;
            //}
            for (int k = 0; k < this_candidate.candiNum_; ++k) {  // candidate
                SSeqLEngDecodeNode & par_node = nodeVec_[j];
                SSeqLEngDecodeNode & this_node = nodeVec_[node_num];
                this_node.comp_ = i;
                this_node.candi_ = k;
                this_node.endPos_ = this_end_pos;
                this_node.depth_ = par_node.depth_ + 1;
                this_node.parent_ = j;
                         
                // regScore 
                this_node.regScore_ = this_candidate.regLogProbVec_[k];

                const char * hist_char =  this_candidate.strVec_[k].c_str();
                double lm_score = par_node.lmScore_;
                LangModel::CalIterWrapper & r = *(this_node.lmHist_);
                r = (*interWordLm_).NextCal(*par_node.lmHist_, &(hist_char), size_t(1), lm_score);
                this_node.lmScore_ = lm_score -  par_node.lmScore_;
                if (lm_score < SEQLENGINTRAGRAMOOPRO - 5) {
                    double new_lm = 0;
                    r = (*interWordLm_).BeginCal(new_lm);
                }
                if (lmWeight0_ > 0) {
                    double lm_score_n1 = 0;
                    LangModel::CalIterWrapper r_n1(LangModel::CalType_CompactTrie) ;
                    r_n1 =  (*interWordLm_).BeginCal(lm_score_n1);
                    r_n1 = (*interWordLm_).NextCal(r_n1, &(hist_char), size_t(1), lm_score_n1);
                    this_node.lmScore0_ = lm_score_n1;
                } else {
                    this_node.lmScore0_ = 0;
                }

                this_node.cdScore_ = this_candidate.checkScoreVec_[k];
                this_node.edit_dis_score_ = this_candidate.edit_dis_vec_[k];

                this_node.acLmScore_ = this_node.lmScore_ + par_node.acLmScore_;
                this_node.acLmScore0_ = this_node.lmScore0_ * fabs(this_node.endPos_ - par_node.endPos_)
                        / lineWid_ + par_node.acLmScore0_;
                this_node.acCdScore_ = this_node.cdScore_ + par_node.acCdScore_;
                this_node.acRegScore_ = this_node.regScore_ + par_node.acRegScore_;
                this_node.ac_edit_dis_score_ = this_node.edit_dis_score_ + par_node.ac_edit_dis_score_;

                //std::cout<<"this_node.depth_:"<<this_node.depth_<<std::endl;
                if(this_node.depth_ <= shortLineMaxWordNum_ ){
                    this_node.score_ = lmWeight_ * this_node.acLmScore_ * shortLmDiscount_
                            + lmWeight0_ * this_node.acLmScore0_*shortLmDiscount_
                            + cdWeight_ * this_node.acCdScore_*
                            + regWeight_ * this_node.acRegScore_
                            + edit_dis_weight_ * this_node.ac_edit_dis_score_;
                    //std::cout<<"this_node.score_:"<<this_node.score_<<std::endl;
                } else {
                    //std::cout<<"print score:"<<lmWeight_<<" "<<this_node.acLmScore_<<" "<<lmWeight0_<<" "<<this_node.acLmScore0_<<" "<<this_node.acCdScore_<<" "<<this_node.acRegScore_<<" "<<this_node.ac_edit_dis_score_<<std::endl;
                    this_node.score_ = lmWeight_ * this_node.acLmScore_
                            + lmWeight0_ * this_node.acLmScore0_
                            + cdWeight_ * this_node.acCdScore_
                            + regWeight_ * this_node.acRegScore_
                            + edit_dis_weight_ * this_node.ac_edit_dis_score_;
                    //std::cout<<"his_this_node.score_:"<<this_node.score_<<std::endl;
                }
                node_num++;
                
                if(node_num >= nodeVec_.size()){
                    extendDecodeNodes();
                }
                //std::cout << "this node string: " << this_candidate.strVec_[k] << " score: " << this_node.score_;
                //std::cout << " ac score: " << this_node.acCdScore_ << " reg score: " << this_node.acRegScore_ << std::endl;
            }
        }

        // start, no history
        if (isStartWord(this_word, start_node_left_space)) {
            //std::cout<<"this_candidate.candiNum_:"<<this_candidate.candiNum_<<std::endl;
            for(int k = 0; k < this_candidate.candiNum_; ++k) {  // candi
                SSeqLEngDecodeNode & this_node = nodeVec_[node_num];
                this_node.comp_ = i;
                this_node.candi_ = k;
                this_node.endPos_ = this_end_pos;
                this_node.depth_ = 1;
                this_node.parent_ = -1;
                double lm_score = 0;
                LangModel::CalIterWrapper & r = *(this_node.lmHist_);
                r =  (*interWordLm_).BeginCal(lm_score);
                const char * hist_char =  this_candidate.strVec_[k].c_str();
                r = (*interWordLm_).NextCal( r, &( hist_char ), size_t(1), lm_score );
                this_node.lmScore_ = lm_score;
                this_node.lmScore0_ = lm_score;
            
                this_node.cdScore_ = this_candidate.checkScoreVec_[k];
                this_node.edit_dis_score_ = this_candidate.edit_dis_vec_[k];

                // regScore 
                this_node.regScore_ = this_candidate.regLogProbVec_[k];
                this_node.acLmScore_ = this_node.lmScore_;
                this_node.acLmScore0_ = this_node.lmScore0_ * fabs(this_node.endPos_ - minLeft_) / lineWid_;
                this_node.acCdScore_ = this_node.cdScore_;
                this_node.acRegScore_ = this_node.regScore_;
                this_node.ac_edit_dis_score_ = this_node.edit_dis_score_;
                
                //std::cout<<"this_node.depth_:"<<this_node.depth_<<std::endl;        
                
                if(this_node.depth_ <= shortLineMaxWordNum_ ){
                    this_node.score_ = lmWeight_ * this_node.acLmScore_*shortLmDiscount_
                            + lmWeight0_ * this_node.acLmScore0_*shortLmDiscount_
                            + cdWeight_ * this_node.acCdScore_
                            + regWeight_ * this_node.acRegScore_
                            + edit_dis_weight_ * this_node.ac_edit_dis_score_;
                    //std::cout<<"this_node.score_:"<<this_node.score_<<std::endl;
                    if(this_candidate.strVec_[k] == "●")
                         //this_node.score_ += 2.5;
                         this_node.acRegScore_ += 10.5;
                      //std::cout<<"checkthis_node.score_:"<<this_node.score_<<std::endl;
                    
                } else {
                    this_node.score_ = lmWeight_ * this_node.acLmScore_
                            + lmWeight0_ * this_node.acLmScore0_
                            + cdWeight_ * this_node.acCdScore_
                            + regWeight_ * this_node.acRegScore_
                            + edit_dis_weight_ * this_node.ac_edit_dis_score_;
                    //std::cout<<"this_node.score_:"<<this_node.score_<<std::endl;
                    if(this_candidate.strVec_[k] == "●")
                        //this_node.score_ += 2.5;
                        this_node.acRegScore_ += 10.5;
                     //std::cout<<"checkthis_node.score_:"<<this_node.score_<<std::endl; 
                }
                node_num++;
                //std::cout<<"node_num:"<<node_num<<" nodeVec_.size()"<<nodeVec_.size()<<std::endl;
                if(node_num >= nodeVec_.size()){
                    extendDecodeNodes();
                }
                //std::cout << "start -- this node string: " << this_candidate.strVec_[k] << " score: " << this_node.score_;
                //std::cout << " ac score: " << this_node.acCdScore_ << " reg score: " << this_node.acRegScore_ << std::endl;
            }
        }
        //sort
        std::cout << "begin sort node:" << std::endl;
        std::cout << "start node:" << start_node << " node_num:" << node_num << std::endl;
        for (int index = 0; index < nodeVec_.size(); index++)
        {
            std::cout<<nodeVec_[index].score_<<" ";
        }
        std::cout<<std::endl;
        sort(nodeVec_.begin()+start_node, nodeVec_.begin()+node_num, bigger_score_first );
        std::cout<<"after sort:"<<std::endl;
        for(int index=0; index< nodeVec_.size(); index++)
        {
            std::cout<< nodeVec_[index].score_<<" ";
        }
        std::cout<<std::endl;
        if( node_num > start_node + beamWidth_ ){
            node_num = start_node + beamWidth_;
        }
    }
    
    // track the output path
    trackBestPathToOutput(node_num);
    releaseDecodeNodes();
    return;
}

void CSeqLEngLineBMSSWordDecoder::decode_att() {
    outputLine_.wordVec_.clear();
    if (candisVec_.size() == 0) {
        return;
    }
    //init
    calLeftRightTopBottom();
    initDecodeNodes();

    int start_node_left_space = 0.5 * spaceConf_.avgCharWidth_;

    int node_num = 0;
    int start_node = -1;

    /*regWeight_ = 0.3; // minimize the influence of the reg
    lmWeight_ = 0.01; 
    lmWeight0_ = 0.015;*/

    //decode 
    if (inputLine_.wordVec_.size() > 1) {
        for (int i = inputLine_.wordVec_.size() - 1; i >= 1; i--) {
        
            if (inputLine_.wordVec_[i].wordStr_ == inputLine_.wordVec_[i - 1].wordStr_ &&
                    inputLine_.wordVec_[i].left_ == inputLine_.wordVec_[i - 1].left_ &&
                    inputLine_.wordVec_[i].top_ == inputLine_.wordVec_[i - 1].top_ &&
                    inputLine_.wordVec_[i].wid_ == inputLine_.wordVec_[i - 1].wid_ &&
                    inputLine_.wordVec_[i].hei_ == inputLine_.wordVec_[i - 1].hei_) {
                inputLine_.wordVec_.erase(inputLine_.wordVec_.begin() + i);
                candisVec_.erase(candisVec_.begin() + i);
            }
        }
    }
    for (int i = 0; i < candisVec_.size(); ++i) {  // wordVec_
        start_node = node_num;
        SSeqLEngWordCandis & this_candidate = candisVec_[i];
        SSeqLEngWordInfor  & this_word = inputLine_.wordVec_[i];
        int this_end_pos = this_word.left_ + this_word.wid_;
        //std::cout << "i " << i << " start_node " << start_node << " node_num " << node_num << std::endl;
        for (int j = start_node - 1; j >= 0; --j) {  // history path
            //std::cout << "i and j: " << i << " " << j << std::endl;
            //std::cout << "node num: " << node_num << " start node: " << start_node << std::endl;
            if (!havepathrelations_att(nodeVec_[j].comp_, i)) {
                //std::cout << "no relation, comp and i: " << nodeVec_[j].comp_ << std::endl;
                continue;
            }
            for (int k = 0; k < this_candidate.candiNum_; ++k) {  // candidate
                SSeqLEngDecodeNode & par_node = nodeVec_[j];
                SSeqLEngDecodeNode & this_node = nodeVec_[node_num];
                this_node.comp_ = i;
                this_node.candi_ = k;
                this_node.endPos_ = this_end_pos;
                this_node.depth_ = par_node.depth_ + 1;
                this_node.parent_ = j;
                         
                // regScore 
                this_node.regScore_ = this_candidate.regLogProbVec_[k];

                const char * hist_char =  this_candidate.strVec_[k].c_str();
                double lm_score = par_node.lmScore_;
                LangModel::CalIterWrapper & r = *(this_node.lmHist_);
                r = (*interWordLm_).NextCal(*par_node.lmHist_, &(hist_char), size_t(1), lm_score);
                if (this_word.main_line_ && this_candidate.checkScoreVec_[k] > 0) {
                    this_node.lmScore_ = std::max(-40.0, lm_score -  par_node.lmScore_);
                }
                else{
                    this_node.lmScore_ = std::max(-75.0, lm_score -  par_node.lmScore_);
                }
                if (lm_score < SEQLENGINTRAGRAMOOPRO - 5) {
                    double new_lm = 0;
                    r = (*interWordLm_).BeginCal(new_lm);
                }
                if (lmWeight0_ > 0) {
                    double lm_score_n1 = 0;
                    LangModel::CalIterWrapper r_n1(LangModel::CalType_CompactTrie);
                    r_n1 =  (*interWordLm_).BeginCal(lm_score_n1);
                    r_n1 = (*interWordLm_).NextCal(r_n1, &(hist_char), size_t(1), lm_score_n1);
                    if (this_word.main_line_ && this_candidate.checkScoreVec_[k] > 0) {
                        this_node.lmScore0_ = std::max(-40.0, lm_score_n1);
                    } else {
                        this_node.lmScore0_ = std::max(-75.0, lm_score_n1);
                    }
                } else {
                    this_node.lmScore0_ = 0;
                }

                this_node.cdScore_ = this_candidate.checkScoreVec_[k];
                this_node.edit_dis_score_ = this_candidate.edit_dis_vec_[k];

                this_node.acLmScore_ = this_node.lmScore_ + par_node.acLmScore_;
                this_node.acLmScore0_ = this_node.lmScore0_ * fabs(this_node.endPos_ \
                        - par_node.endPos_) / lineWid_ + par_node.acLmScore0_;
                this_node.acCdScore_ = this_node.cdScore_ + par_node.acCdScore_;
                this_node.acRegScore_ = this_node.regScore_ + par_node.acRegScore_;
                this_node.ac_edit_dis_score_ = this_node.edit_dis_score_ + \
                                               par_node.ac_edit_dis_score_;

                if (this_node.depth_ <= shortLineMaxWordNum_) {
                    this_node.score_ = lmWeight_ * this_node.acLmScore_ * shortLmDiscount_
                            + lmWeight0_ * this_node.acLmScore0_*shortLmDiscount_
                            + cdWeight_ * this_node.acCdScore_*
                            + regWeight_ * this_node.acRegScore_
                            + edit_dis_weight_ * this_node.ac_edit_dis_score_;
                } else {
                    this_node.score_ = lmWeight_ * this_node.acLmScore_
                            + lmWeight0_ * this_node.acLmScore0_
                            + cdWeight_ * this_node.acCdScore_
                            + regWeight_ * this_node.acRegScore_
                            + edit_dis_weight_ * this_node.ac_edit_dis_score_;
                }
                node_num++;
                if (node_num >= nodeVec_.size()) {
                    extendDecodeNodes();
                }
                /*
                std::cout << "Node id: " << node_num - 1 << std::endl;
                std::cout << "this node string: " << this_candidate.strVec_[k] << " score: " << this_node.score_;
                std::cout << " ac score: " << this_node.acCdScore_ << " reg score: " << this_node.acRegScore_;
                std::cout << " ac lm score: " << this_node.acLmScore_ << " ac lm score0: " << this_node.acLmScore0_;
                std::cout << " " << this_node.cdScore_ << " " << this_node.regScore_ << " " << this_node.acLmScore_ << " " << this_node.acLmScore0_ << std::endl;*/
            }
        }

        // start, no history
        if (isStartWord(this_word, start_node_left_space)) {
            for (int k = 0; k < this_candidate.candiNum_; ++k) {  // candi
                SSeqLEngDecodeNode & this_node = nodeVec_[node_num];
                this_node.comp_ = i;
                this_node.candi_ = k;
                this_node.endPos_ = this_end_pos;
                this_node.depth_ = 1;
                this_node.parent_ = -1;
                double lm_score = 0;
                LangModel::CalIterWrapper & r = *(this_node.lmHist_);
                r =  (*interWordLm_).BeginCal(lm_score);
                const char * hist_char = this_candidate.strVec_[k].c_str();
                r = (*interWordLm_).NextCal(r, &(hist_char), size_t(1), lm_score);
                if (this_word.main_line_ && this_candidate.checkScoreVec_[k] > 0) {
                    this_node.lmScore_ = std::max(-40.0, lm_score);
                    this_node.lmScore0_ = std::max(-40.0, lm_score);
                } else {
                    this_node.lmScore_ = std::max(-75.0, lm_score);
                    this_node.lmScore0_ = std::max(-75.0, lm_score);
                }
            
                this_node.cdScore_ = this_candidate.checkScoreVec_[k];
                this_node.edit_dis_score_ = this_candidate.edit_dis_vec_[k];

                // regScore 
                this_node.regScore_ = this_candidate.regLogProbVec_[k];
                this_node.acLmScore_ = this_node.lmScore_;
                this_node.acLmScore0_ = this_node.lmScore0_ * \
                                        fabs(this_node.endPos_ - minLeft_) / lineWid_;
                this_node.acCdScore_ = this_node.cdScore_;
                this_node.acRegScore_ = this_node.regScore_;
                this_node.ac_edit_dis_score_ = this_node.edit_dis_score_;

                if (this_node.depth_ <= shortLineMaxWordNum_) {
                    this_node.score_ = lmWeight_ * this_node.acLmScore_*shortLmDiscount_
                            + lmWeight0_ * this_node.acLmScore0_*shortLmDiscount_
                            + cdWeight_ * this_node.acCdScore_
                            + regWeight_ * this_node.acRegScore_
                            + edit_dis_weight_ * this_node.ac_edit_dis_score_;
                } else {
                    this_node.score_ = lmWeight_ * this_node.acLmScore_
                            + lmWeight0_ * this_node.acLmScore0_
                            + cdWeight_ * this_node.acCdScore_
                            + regWeight_ * this_node.acRegScore_
                            + edit_dis_weight_ * this_node.ac_edit_dis_score_;
                }
                node_num++;
                if (node_num >= nodeVec_.size()) {
                    extendDecodeNodes();
                }
                /*
                std::cout << "Node id: " << node_num - 1 << std::endl;
                std::cout << "start -- this node string: " << this_candidate.strVec_[k] << " score: " << this_node.score_;
                std::cout << " ac score: " << this_node.acCdScore_ << " reg score: " << this_node.acRegScore_;
                std::cout << " ac lm score: " << this_node.acLmScore_ << " ac lm score0: " << this_node.acLmScore0_;
                std::cout << " " << this_node.cdScore_ << " " << this_node.regScore_ << " " << this_node.acLmScore_ << " " << this_node.acLmScore0_ << std::endl;
                */
            }
        }
        //sort
        sort(nodeVec_.begin()+start_node, nodeVec_.begin() + node_num, bigger_score_first);
        if (node_num > start_node + beamWidth_) {
            node_num = start_node + beamWidth_;
        }
    }
    
    // track the output path
    trackBestPathToOutput(node_num);
    releaseDecodeNodes();
    return;
}

void CSeqLEngLineBMSSWordDecoder :: beamSearchDecode( ){
    outputLine_.wordVec_.clear();
    if(candisVec_.size()==0){
      return;
    }
    assert( candisVec_.size() == inputLine_.wordVec_.size());
    //init
    calLeftRightTopBottom();
    initDecodeNodes();

    int startNodeLeftSpace = 0.5*spaceConf_.avgCharWidth_;

    int nodeNum = 0;
    int startNode = -1;
    
    //decode 
    
    for(unsigned int i=0; i<candisVec_.size(); i++ ){  // wordVec_
       startNode = nodeNum;
       SSeqLEngWordCandis & thisWordCandi = candisVec_[i];
       SSeqLEngWordInfor  & thisWord = inputLine_.wordVec_[i];
       //std::cout << "word vec: " << inputLine_.wordVec_[i].wordStr_.c_str() << std::endl;
       //std::cout << "rm punct word vec: " << thisWordCandi.rmPunctStrVec_[0].c_str() << std::endl;
       int thisEndPos = thisWord.left_ + thisWord.wid_;


       for( int j=startNode-1; j>=0; j--){  // history patch
       //   std::cout << "beamSearchDecode i "<< i << "j:..." << j << std::endl;
          // relation check
          //if( !havePathRelations(nodeVec_[j], thisWord )){
          //if( false || ( !havePathRelations(nodeVec_[j].comp_, i ) ) ){
          if( false || ( !havePathRelations(nodeVec_[j].comp_, i ) ) ){
             continue;
          }
          for(unsigned int k=0; k<thisWordCandi.candiNum_; k++){  // candi
            
            if(thisWordCandi.rmPunctStrVec_[k].length()==0){
                continue;
            }

            SSeqLEngDecodeNode & parNode = nodeVec_[j];
            SSeqLEngDecodeNode & thisNode = nodeVec_[nodeNum];
            thisNode.comp_ = i;
            thisNode.candi_ = k;
            thisNode.endPos_ = thisEndPos;
            thisNode.depth_ = parNode.depth_+1;
            thisNode.parent_ = j;
            thisNode.avSpace_ = ((thisWord.left_ - parNode.endPos_) + parNode.avSpace_*(parNode.depth_-1))/parNode.depth_;
            thisNode.spScore_ = -1*fabs((thisWord.left_ - parNode.endPos_)-thisNode.avSpace_)/(thisNode.avSpace_+0.1);
            thisNode.sjScore_ = ( (thisWord.left_ - parNode.endPos_) -0.3*lineHei_ )/lineHei_;
            thisNode.sjScore_ = thisNode.sjScore_ < 0 ? thisNode.sjScore_ : 0;
            if( thisWordCandi.editDistVec_[k]==0 ){
                thisNode.mulRegScore_ = thisWord.mulRegScore_;
            }else{
                thisNode.mulRegScore_ = 0;
            }
         
            // regScore 
            //thisNode.regScore_ = thisWord.regLogProb_;
            thisNode.regScore_ = thisWordCandi.regLogProbVec_[k];

            if (spaceConf_.meanBoxSpace_ < 0.01) {
                spaceConf_.meanBoxSpace_ += 0.01;
            }

            thisNode.spScore2_ = thisWord.dectScore_ + ((thisWord.left_ - parNode.endPos_) - \
                    spaceConf_.meanBoxSpace_)/spaceConf_.meanBoxSpace_;
            

            if(1){
              const char * histChar =  thisWordCandi.rmPunctStrVec_[k].c_str();
              double lmScore = parNode.lmScore_;
              LangModel::CalIterWrapper & r = *(thisNode.lmHist_);
              r = (*interWordLm_).NextCal( *parNode.lmHist_, &( histChar ), size_t(1), lmScore );
              thisNode.lmScore_ = lmScore -  parNode.lmScore_;
              if(lmScore<SEQLENGINTRAGRAMOOPRO-5){
                  double newLm = 0;
                  r = (*interWordLm_).BeginCal(newLm);
              }

              if(lmWeight0_>0){
                double lmScoreN1 = 0;
                LangModel::CalIterWrapper r_n1(LangModel::CalType_CompactTrie) ;
                r_n1 =  (*interWordLm_).BeginCal( lmScoreN1 );
                r_n1 = (*interWordLm_).NextCal( r_n1, &( histChar ), size_t(1), lmScoreN1 );
                thisNode.lmScore0_ = lmScoreN1;
              }
              else{
                thisNode.lmScore0_ = 0;
              }


            }else if (0){
              const char * histChar =  thisWordCandi.rmPunctStrVec_[k].c_str();
              double lmScoreN1 = 0;
              LangModel::CalIterWrapper r_n1(LangModel::CalType_CompactTrie) ;
              r_n1 =  (*interWordLm_).BeginCal( lmScoreN1 );
              r_n1 = (*interWordLm_).NextCal( r_n1, &( histChar ), size_t(1), lmScoreN1 );
              *(thisNode.lmHist_) = r_n1;
              thisNode.lmScore0_ = lmScoreN1;
              
              double lmScoreN2 = parNode.lmScore0_;
              LangModel::CalIterWrapper r_n2(LangModel::CalType_CompactTrie) ;
              r_n2 = (*interWordLm_).NextCal( *parNode.lmHist_, &( histChar ), size_t(1), lmScoreN2 );
              thisNode.lmScore_ = lmScoreN2 - parNode.lmScore0_; 
            }
            else
            {
              double lmScore = 0;
              LangModel::CalIterWrapper & r = *(thisNode.lmHist_);
              r =  (*interWordLm_).BeginCal( lmScore );
              const char * histChar =  thisWordCandi.rmPunctStrVec_[k].c_str();
              r = (*interWordLm_).NextCal( r, &( histChar ), size_t(1), lmScore );
              thisNode.lmScore_ = lmScore;
              thisNode.lmScore0_ = lmScore;
            }


            //thisNode.cdScore_ = log(thisWordCandi.checkScoreVec_[k]);
            thisNode.cdScore_ = calCdScoreFun(thisWordCandi.checkScoreVec_[k]);
            thisNode.cdScore_ = thisNode.cdScore_ > -100 ? thisNode.cdScore_ : -100;
            //thisNode.cdScore_ = thisNode.cdScore_*(thisNode.endPos_-parNode.endPos_)/lineHei_;
            
            //thisNode.acLmScore_ = thisNode.lmScore_*fabs(thisNode.endPos_ - parNode.endPos_) / (lineWid_)  \
                                  + parNode.acLmScore_;
            thisNode.acLmScore_ = thisNode.lmScore_  \
                                  + parNode.acLmScore_;
            thisNode.acLmScore0_ = thisNode.lmScore0_*fabs(thisNode.endPos_ - parNode.endPos_) / (lineWid_) \
                                   + parNode.acLmScore0_;
            thisNode.acCdScore_ = thisNode.cdScore_ + parNode.acCdScore_;
            thisNode.acSpScore_ = thisNode.spScore_ + parNode.acSpScore_;
            thisNode.acSpScore2_ = thisNode.spScore2_ + parNode.acSpScore2_;
            thisNode.acSjScore_ = thisNode.sjScore_ + parNode.acSjScore_;
            thisNode.acRegScore_ = thisNode.regScore_ + parNode.acRegScore_;
            thisNode.acMulRegScore_ = thisNode.mulRegScore_ +  parNode.acMulRegScore_;

            
            //thisNode.score_ = (lmWeight_ * (thisNode.lmScore_ + thisNode.lmScore0_) + cdWeight_ * thisNode.cdScore_ \
                     + spWeight_ * thisNode.spScore_ )* \
                 fabs(thisNode.endPos_ - parNode.endPos_) / (lineWid_) + \
                parNode.score_; 
            //thisNode.score_ = lmWeight_ * thisNode.acLmScore_ / thisNode.depth_   + \
                              lmWeight0_ * thisNode.acLmScore0_ / thisNode.depth_ +\
                              cdWeight_ * thisNode.acCdScore_  + \
                              spWeight_ * thisNode.acSpScore_ /thisNode.depth_;
            if(thisNode.depth_ <= shortLineMaxWordNum_ ){
                thisNode.score_ = lmWeight_ * thisNode.acLmScore_*shortLmDiscount_  + \
                              lmWeight0_ * thisNode.acLmScore0_*shortLmDiscount_ +\
                              cdWeight_ * thisNode.acCdScore_*  + \
                              sjWeight_ * thisNode.acSjScore_  + \
                              spWeight2_ * thisNode.acSpScore2_ + \
                              regWeight_ * thisNode.acRegScore_ + \
                              mulRegWeight_ * thisNode.acMulRegScore_ + \
                              spWeight_ * thisNode.acSpScore_ /thisNode.depth_;
            }
            else
            {
                thisNode.score_ = lmWeight_ * thisNode.acLmScore_  + \
                              lmWeight0_ * thisNode.acLmScore0_ +\
                              cdWeight_ * thisNode.acCdScore_  + \
                              sjWeight_ * thisNode.acSjScore_  + \
                              spWeight2_ * thisNode.acSpScore2_ + \
                              regWeight_ * thisNode.acRegScore_ + \
                              mulRegWeight_ * thisNode.acMulRegScore_ + \
                              spWeight_ * thisNode.acSpScore_ /thisNode.depth_;
            }
            
            nodeNum++;
            if(nodeNum >= nodeVec_.size()){
              extendDecodeNodes();
            }
          }
          
          //for( ; ; ) no need to group word-pair
        }
       // start , no history
       if( isStartWord( thisWord,startNodeLeftSpace ) ){
         for(unsigned int k=0; k<thisWordCandi.candiNum_; k++){  // candi
            SSeqLEngDecodeNode & thisNode = nodeVec_[nodeNum];
            thisNode.comp_ = i;
            thisNode.candi_ = k;
            thisNode.endPos_ = thisEndPos;
            thisNode.depth_ = 1;
            thisNode.parent_ = -1;
            double lmScore = 0;
            LangModel::CalIterWrapper & r = *(thisNode.lmHist_);
            r =  (*interWordLm_).BeginCal( lmScore );
            const char * histChar =  thisWordCandi.rmPunctStrVec_[k].c_str();
            r = (*interWordLm_).NextCal( r, &( histChar ), size_t(1), lmScore );
            thisNode.lmScore_ = lmScore;
            thisNode.lmScore0_ = lmScore;
            
            //thisNode.cdScore_ = log(thisWordCandi.checkScoreVec_[k]);
            thisNode.cdScore_ = calCdScoreFun(thisWordCandi.checkScoreVec_[k]);
            thisNode.cdScore_ = thisNode.cdScore_ > -100 ? thisNode.cdScore_ : -100;
            //thisNode.cdScore_ = thisNode.cdScore_*(thisNode.endPos_-minLeft_ )/lineHei_;
      
            thisNode.spScore_ = 0;
            thisNode.sjScore_ = 0;
            thisNode.avSpace_ = 0;
            thisNode.spScore2_ = thisWord.dectScore_;

            // regScore 
          //  thisNode.regScore_ = thisWord.regLogProb_;
            thisNode.regScore_ = thisWordCandi.regLogProbVec_[k];
            if( thisWordCandi.editDistVec_[k]==0 ){
                thisNode.mulRegScore_ = thisWord.mulRegScore_;
            }else{
                thisNode.mulRegScore_ = 0;
            }


            //thisNode.acLmScore_ = thisNode.lmScore_*fabs(thisNode.endPos_ - minLeft_) / (lineWid_);
            thisNode.acLmScore_ = thisNode.lmScore_;
            thisNode.acLmScore0_ = thisNode.lmScore0_*fabs(thisNode.endPos_ - minLeft_) / (lineWid_);
            thisNode.acCdScore_ = thisNode.cdScore_;
            thisNode.acSpScore_ = thisNode.spScore_;
            thisNode.acSpScore2_ = thisNode.spScore2_;
            thisNode.acSjScore_ = thisNode.sjScore_;
            thisNode.acRegScore_ = thisNode.regScore_;
            thisNode.acMulRegScore_ = thisNode.mulRegScore_;

            //thisNode.score_ = lmWeight_ * thisNode.acLmScore_ / thisNode.depth_  + \
                              lmWeight0_ * thisNode.acLmScore0_ / thisNode.depth_ +\
                              cdWeight_ * thisNode.acCdScore_  + \
                              spWeight_ * thisNode.acSpScore_ /thisNode.depth_;
            if(thisNode.depth_ <= shortLineMaxWordNum_ ){
                thisNode.score_ = lmWeight_ * thisNode.acLmScore_*shortLmDiscount_  + \
                              lmWeight0_ * thisNode.acLmScore0_*shortLmDiscount_ +\
                              cdWeight_ * thisNode.acCdScore_  + \
                              sjWeight_ * thisNode.acSjScore_  + \
                              spWeight2_ * thisNode.acSpScore2_ + \
                              regWeight_ * thisNode.acRegScore_ + \
                              mulRegWeight_ * thisNode.acMulRegScore_ + \
                              spWeight_ * thisNode.acSpScore_ /thisNode.depth_;
            }
            else
            {
                thisNode.score_ = lmWeight_ * thisNode.acLmScore_  + \
                              lmWeight0_ * thisNode.acLmScore0_ +\
                              cdWeight_ * thisNode.acCdScore_  + \
                              sjWeight_ * thisNode.acSjScore_  + \
                              spWeight2_ * thisNode.acSpScore2_ + \
                              regWeight_ * thisNode.acRegScore_ + \
                              mulRegWeight_ * thisNode.acMulRegScore_ + \
                              spWeight_ * thisNode.acSpScore_ /thisNode.depth_;
            }
            
            
            nodeNum++;
            if(nodeNum >= nodeVec_.size()){
               extendDecodeNodes();
            }
         }

       }
       //sort
       sort(nodeVec_.begin()+startNode, nodeVec_.begin()+nodeNum, bigger_score_first );
       if( nodeNum > startNode + beamWidth_ ){
          nodeNum = startNode + beamWidth_;
       }
    }
    
    // track the output path
    trackBestPathToOutput( nodeNum);
    
    releaseDecodeNodes();
    return;
}

void SeqLEngRemoveBoundaryNoisyWord( SSeqLEngLineInfor & line, int imgWid, int imgHei, bool filterShort)
{
    std::vector<SSeqLEngWordInfor>::iterator ita = line.wordVec_.begin();
    while(ita != line.wordVec_.end())
    {
        if(ita->top_<=1 || (ita->top_+ita->hei_)>=imgHei-2 || (filterShort && strlen(ita->wordStr_.c_str())<3) )
            ita=line.wordVec_.erase(ita);
        else
            ita++;
    }    
}


// remove noisy text around image boundary
void CSeqLEngLineBMSSWordDecoder :: SeqLEngRemoveBoundaryNoisyText( SSeqLEngLineInfor & line, int imgWid, int imgHei, esp_engsearch *g_pEngSearchRecog )
{
	int validNum;
    int validCharNum;
	int noisyNum;
    int noisyCharNum;
	int longExistNum;
	int longExistCharNum;
    int shortWordNum;
	int punCharNum;
	int singleCharNum;
	int longestWordCharNum;
    
	validNum = 0;
    validCharNum =0;
	noisyNum = 0;
    noisyCharNum = 0;
	longExistNum = 0;
    longExistCharNum = 0;
	shortWordNum = 0;
	punCharNum   = 0;
	singleCharNum = 0;
	longestWordCharNum = 0;

	bool onlyOneLine = false;
	if( line.hei_ > 0.6 * imgHei )
		onlyOneLine = true;

	pthread_t threadID = pthread_self();
    
    int estWordNum = (line.wid_)/(line.hei_);

	if( line.top_ <= 1 || (line.top_+line.hei_) >= imgHei-2 )
	{
        if(((line.hei_)<imgHei*0.2 && (line.wid_)<4*(line.hei_))
                || (line.hei_)<imgHei*0.1)
        {
            line.wordVec_.clear();                                                          
        }
        if(1)
        {
		    for( int j=0; j< line.wordVec_.size(); j++ )
		    {
			    vector<pair<string, double> > output;
			    string inputWord;

			    inputWord  = line.wordVec_[j].wordStr_;
			    transform( inputWord.begin(), inputWord.end(), inputWord.begin(), ::tolower );
			    //printf( "orig word: %s\n", line.wordVec_[j].wordStr_.c_str());

			    int check_rst = g_pEngSearchRecog->esp_get( (threadType)threadID, inputWord.c_str(), output, 0, 1 );
			    //printf( "orig word: %s %d %d\n", inputWord.c_str(), check_rst, strlen(inputWord.c_str()) );

               if( strlen( line.wordVec_[j].wordStr_.c_str()) <= 2 )
			   { 
			    	if( check_rst == 0 )
				    {
					   validNum += 1;
				    }
				    else
				    {
					   noisyNum += 1;
                       noisyCharNum += strlen( line.wordVec_[j].wordStr_.c_str());
				    }
				    shortWordNum ++;
				    if( strlen( line.wordVec_[j].wordStr_.c_str()) < 2 )
					   singleCharNum ++;
			    }
			    else if( check_rst >= 0 )
			    {	
				    validNum +=1;
                    validCharNum += strlen( line.wordVec_[j].wordStr_.c_str());
				    if( check_rst == 0 )
				    {
					   if( strlen( line.wordVec_[j].wordStr_.c_str()) > longestWordCharNum )
                       {
					    	longestWordCharNum = strlen( line.wordVec_[j].wordStr_.c_str());
                       }
					   if( strlen( line.wordVec_[j].wordStr_.c_str()) >= 4 )
                       {
                            longExistNum ++;
                            longExistCharNum+= strlen( line.wordVec_[j].wordStr_.c_str());
                       }
				    }
			    }
			    else
			    {
				 //   printf( "there is no valid dic word!\n" );
				    noisyNum +=1;
                    noisyCharNum += strlen( line.wordVec_[j].wordStr_.c_str());
			    }
		     }
		     //printf( "noisyNum: %d, validNum: %d, totNum: %d\n", noisyNum, validNum, line.wordVec_.size() );
		     if( !onlyOneLine )
		     {
                 if( ((estWordNum<=8&&longExistNum<=noisyCharNum)|| (estWordNum>8&&longExistNum<noisyCharNum)) || shortWordNum >= 0.8 * line.wordVec_.size() )
                 {
                     //    EngRemoveBoundaryNoisyWord(sent, imgWid, imgHei,true);
				     line.wordVec_.clear();
                 }
                 else if( ((estWordNum<=15&&longExistNum<=3.0*noisyNum)|| (estWordNum>15&&longExistNum <=2.0*noisyNum)) || shortWordNum >= 0.5 * line.wordVec_.size() )
			     {
               // printf("11,Delete by longExistNum=%d, validNum=%d,validCharNum=%d,noisyNum=%d,noisyCharNum=%d,shortWordNum=%d,sent->wordNum=%d,estWordNum=%d\n", \
                    longExistNum,validNum,validCharNum, noisyNum,noisyCharNum,shortWordNum,line.wordVec_.size(),estWordNum);
                    SeqLEngRemoveBoundaryNoisyWord( line, imgWid, imgHei,false);
			     }
		     }
        }
	}

}


void CSeqLEngLineBMSSWordDecoder :: SeqLEngRemoveNoisyWords( SSeqLEngLineInfor & line, int imgWid, int imgHei, esp_engsearch *g_pEngSearchRecog )
{
	int validNum;
    int validCharNum;
	int noisyNum;
    int noisyCharNum;
	int longExistNum;
	int longExistCharNum;
    int shortWordNum;
	int punCharNum;
	int singleCharNum;
	int longestWordCharNum;
    
	validNum = 0;
    validCharNum =0;
	noisyNum = 0;
    noisyCharNum = 0;
	longExistNum = 0;
    longExistCharNum = 0;
	shortWordNum = 0;
	punCharNum   = 0;
	singleCharNum = 0;
	longestWordCharNum = 0;

	bool onlyOneLine = false;
	if( line.hei_ > 0.6 * imgHei )
		onlyOneLine = true;

	pthread_t threadID = pthread_self();
    
    int estWordNum = (line.wid_)/(line.hei_);
    std::vector<bool> toDeleteVec;
    toDeleteVec.resize(line.wordVec_.size());

	for( int j=0; j< line.wordVec_.size(); j++ )
	{
		vector<pair<string, double> > output;
		string inputWord;

		inputWord  = line.wordVec_[j].wordStr_;
		transform( inputWord.begin(), inputWord.end(), inputWord.begin(), ::tolower );
		//printf( "orig word: %s\n", line.wordVec_[j].wordStr_.c_str());

		int check_rst = g_pEngSearchRecog->esp_get( (threadType)threadID, inputWord.c_str(), output, 0, 1 );
		//printf( "orig word: %s %d %d\n", inputWord.c_str(), check_rst, strlen(inputWord.c_str()) );
        
        toDeleteVec[j] = false;
        if( strlen( line.wordVec_[j].wordStr_.c_str()) <= 2 )
		{ 
            if( check_rst == 0 )
            {
                validNum += 1;
            }
            else
            {
                noisyNum += 1;
                noisyCharNum += strlen( line.wordVec_[j].wordStr_.c_str());
            }
            shortWordNum ++;
            if( strlen( line.wordVec_[j].wordStr_.c_str()) < 2 )
                singleCharNum ++;
          }
          else if( check_rst >= 0 )
          {	
                validNum +=1;
                validCharNum += strlen( line.wordVec_[j].wordStr_.c_str());
                if( check_rst == 0 )
                {
                    if( strlen( line.wordVec_[j].wordStr_.c_str()) > longestWordCharNum )
                    {
                        longestWordCharNum = strlen( line.wordVec_[j].wordStr_.c_str());
                    }
                    if( strlen( line.wordVec_[j].wordStr_.c_str()) >= 4 )
                    {
                        longExistNum ++;
                        longExistCharNum+= strlen( line.wordVec_[j].wordStr_.c_str());
                    }
				}
			}
			else
			{
//				    printf( "there is no valid dic word!\n" );
				    noisyNum +=1;
                    noisyCharNum += strlen( line.wordVec_[j].wordStr_.c_str());
                    toDeleteVec[j] = true;

			}
    }
    //printf( "noisyNum: %d, validNum: %d, totNum: %d\n", noisyNum, validNum, line.wordVec_.size() );
    if( !onlyOneLine )
	{
        int deleteIndex = 0;
        if(longExistNum<=noisyCharNum || noisyCharNum>2){
            std::vector<SSeqLEngWordInfor>::iterator ita = line.wordVec_.begin();
            while(ita != line.wordVec_.end())
           {
               if(toDeleteVec[deleteIndex])
                 ita=line.wordVec_.erase(ita);
               else
                 ita++;

               deleteIndex ++ ;
           }    
        }
    }

}

bool SeqLEngDecodeIsEnglishChar(char c){
    if( c >='a'&& c <='z'){
        return true;
    } 
    if( c >='A'&& c <='Z'){
        return true;
    }
    return false;
}

bool SeqLEngDecodeIsDigitChar(char c){
    if( c >='0' && c<='9'){
        return true;
    }
    return false;
}

void CSeqLEngLineBMSSWordDecoder :: SeqLEngPostPuntDigWordText(SSeqLEngLineInfor & line ){
    std::vector<SSeqLEngWordInfor> wordVec;
    for (unsigned int w=0; w<line.wordVec_.size(); w++) {
        SSeqLEngWordInfor & inputWord = line.wordVec_[w];
        std::vector<std::string> punctStrVec;
        int startIndex = 0;
        int perFlag = -1;
        std::string tempStr;
        tempStr.reserve(inputWord.wordStr_.length()+1);
        //std::cout << "inside pos punct: " << inputWord.wordStr_.c_str() << std::endl;
        for(unsigned int i=0; i<inputWord.wordStr_.length(); i++){
            int thisFlag = -1;
            if(SeqLEngDecodeIsEnglishChar(inputWord.wordStr_[i])){
                thisFlag = 0;
            }
            if(SeqLEngDecodeIsDigitChar(inputWord.wordStr_[i])){
                thisFlag = 1;
            }
            if(perFlag!=-1 && thisFlag==perFlag){
                tempStr.push_back(inputWord.wordStr_[i]);
                continue;
            }

            if(perFlag==1 && thisFlag==0){   // 16mb.
                int afterEngNum = 0;
                for(unsigned j=i+1;j<inputWord.wordStr_.length();j++){
                  if(SeqLEngDecodeIsEnglishChar(inputWord.wordStr_[j])){
                    afterEngNum++;
                  }
                  else{
                    break;
                  }
                }
                if(afterEngNum<=2){
                   perFlag = 0;
                   tempStr.push_back(inputWord.wordStr_[i]);
                   continue;
                
                }
                /*
              if(i==(inputWord.wordStr_.length()-1)){
                   perFlag = 0;
                   tempStr.push_back(inputWord.wordStr_[i]);
                   continue;
              }
              else if (i==(inputWord.wordStr_.length()-2)){
                 if(SeqLEngDecodeIsEnglishChar(inputWord.wordStr_[i+1])){
                   perFlag = 0;
                   tempStr.push_back(inputWord.wordStr_[i]);
                   continue;
                 }
              }
              else if( SeqLEngDecodeIsEnglishChar(inputWord.wordStr_[i+1]) && \
                      (!SeqLEngDecodeIsEnglishChar(inputWord.wordStr_[i+2]))){
                   perFlag = 0;
                   tempStr.push_back(inputWord.wordStr_[i]);
                   continue;
              }*/
            }

            if(tempStr.length()==0){
                perFlag = thisFlag;
                tempStr.push_back(inputWord.wordStr_[i]);
                continue;
            }
           
            if(i!=0 && i!=(inputWord.wordStr_.length()-1)){  // a-b
                if(inputWord.wordStr_[i] == '-'  \ 
                    && SeqLEngDecodeIsEnglishChar( inputWord.wordStr_[i-1] ) \
                    && SeqLEngDecodeIsEnglishChar( inputWord.wordStr_[i+1] ) ){
                tempStr.push_back(inputWord.wordStr_[i]);
                continue;
                }
            }
            
             if (i!=0 && i!=(inputWord.wordStr_.length()-1)){  // 201*-0-**
                if (inputWord.wordStr_[i] == '-'  \ 
                    && SeqLEngDecodeIsDigitChar( inputWord.wordStr_[i-1] ) \
                    && SeqLEngDecodeIsDigitChar( inputWord.wordStr_[i+1] ) ){
                tempStr.push_back(inputWord.wordStr_[i]);
                continue;
                }
            }

            if (i!=0 && i != (inputWord.wordStr_.length()-1)) {  // it's
                if (inputWord.wordStr_[i] == '\''  \ 
                    && SeqLEngDecodeIsEnglishChar(inputWord.wordStr_[i-1]) \
                    && SeqLEngDecodeIsEnglishChar(inputWord.wordStr_[i+1])){
                tempStr.push_back(inputWord.wordStr_[i]);
                continue;
                }
            }
            
            if(i!=0 && i!=(inputWord.wordStr_.length()-1)){  // 1.1
                if(inputWord.wordStr_[i] == '.'  \ 
                    && SeqLEngDecodeIsDigitChar( inputWord.wordStr_[i-1] ) \
                    && SeqLEngDecodeIsDigitChar( inputWord.wordStr_[i+1] ) ){
                tempStr.push_back(inputWord.wordStr_[i]);
                continue;
                }
            }

            if(i!=0 ){ // -1
                if(inputWord.wordStr_[i-1] == '-'  \  
                    && SeqLEngDecodeIsDigitChar( inputWord.wordStr_[i] ) ){
                perFlag = thisFlag;
                tempStr.push_back(inputWord.wordStr_[i]);
                continue;
                }
            }

            perFlag = thisFlag;
            if(tempStr.length()>0){
                punctStrVec.push_back(tempStr);
                tempStr.clear();
            }
            tempStr.push_back(inputWord.wordStr_[i]);
        }
        if(tempStr.length()>0){
            punctStrVec.push_back(tempStr);
            tempStr.clear();
        }
        int startCharIndex = 0; 
        for(unsigned int i=0; i<punctStrVec.size(); i++){
            SSeqLEngWordInfor newWord = inputWord;
            newWord.wordStr_ = punctStrVec[i];
            newWord.left_ = inputWord.left_ + startCharIndex*inputWord.wid_/(inputWord.wordStr_.length()+punctStrVec.size()-1);
            newWord.wid_ = newWord.wordStr_.length()*inputWord.wid_/(inputWord.wordStr_.length()+punctStrVec.size()-1);

            wordVec.push_back(newWord);
            startCharIndex += (newWord.wordStr_.length()+1);
        }
    }
    //previous
    line.wordVec_ = wordVec;
    //revision
    //line.wordVec_.clear();
    //for (int i = 0; i < wordVec.size(); i++) {
    //    line.wordVec_.push_back(wordVec[i]);
    //}
    ////
}


void CSeqLEngLineBMSSWordDecoder :: SeqLEngMergeCloseWordText( SSeqLEngLineInfor & line, esp_engsearch *g_pEngSearchRecog ){
    SeqLEngMergeHyphenCloseWordText(line );
    seqleng_merge_special_line(line);
    //SeqLEngMergeAValidCloseWord( line, g_pEngSearchRecog );
}


void CSeqLEngLineBMSSWordDecoder :: SeqLEngMergeHyphenCloseWordText(SSeqLEngLineInfor & line ){
    std::vector<SSeqLEngWordInfor> wordVec;
    int orgWordSize = line.wordVec_.size();
    //std::cout << "mean box space: " << spaceConf_.meanBoxSpace_ << std::endl;
    for(int i=0; i<orgWordSize; ){
        //std::cout << line.wordVec_[i].wordStr_ << std::endl; 
        if(i<orgWordSize-2){
            if( line.wordVec_[i+1].wordStr_.length()>0)
            {
                //merge "a - b" to "a-b" and "it ' s" to "it's"
                if (line.wordVec_[i+1].wordStr_=="-" || (line.wordVec_[i+1].wordStr_=="'" && (line.wordVec_[i+2].wordStr_=="s"\
                                || line.wordVec_[i+2].wordStr_=="S"))) {
                bool largeSpaceFlag = false;
                SSeqLEngWordInfor newWord = line.wordVec_[i];
                int newRight = line.wordVec_[i].left_ + line.wordVec_[i].wid_;
                int newBottom = line.wordVec_[i].top_ + line.wordVec_[i].hei_;
                newWord.left_ = std::min( line.wordVec_[i+1].left_, newWord.left_);
                newWord.top_  = std::min( line.wordVec_[i+1].top_, newWord.top_);
                //std::cout << "first space: " << line.wordVec_[i+1].left_ - newRight << std::endl;
                if (newRight + (line.wordVec_[i+1].wordStr_ == "'" ? 3 : 1) * spaceConf_.meanBoxSpace_ < line.wordVec_[i+1].left_) {
                    largeSpaceFlag = true;
                }
                newRight = std::max( line.wordVec_[i+1].left_+line.wordVec_[i+1].wid_, newRight);
                newBottom = std::max( line.wordVec_[i+1].top_+line.wordVec_[i+1].hei_, newBottom);
                
                //std::cout << "second space: " << line.wordVec_[i+2].left_ - newRight << std::endl;
                if (newRight+(line.wordVec_[i+1].wordStr_ == "'" ? 3 : 1) * spaceConf_.meanBoxSpace_ < line.wordVec_[i+2].left_) {
                    largeSpaceFlag = true;
                }
                newWord.left_ = std::min( line.wordVec_[i+2].left_, newWord.left_);
                newWord.top_  = std::min( line.wordVec_[i+2].top_, newWord.top_);
                newRight = std::max( line.wordVec_[i+2].left_+line.wordVec_[i+2].wid_, newRight);
                newBottom = std::max( line.wordVec_[i+2].top_+line.wordVec_[i+2].hei_, newBottom);
                
                newWord.wid_ = newRight - newWord.left_;
                newWord.hei_ = newBottom - newWord.top_;

                newWord.wordStr_ += line.wordVec_[i+1].wordStr_;
                newWord.wordStr_ += line.wordVec_[i+2].wordStr_;
               
                if(largeSpaceFlag==false){
                    wordVec.push_back(newWord);
                    i=i+3;
                    continue;
                }
               }
          }
        }
        SSeqLEngWordInfor newWord2 = line.wordVec_[i];
        wordVec.push_back(newWord2);
        i=i+1;
    }
    line.wordVec_ = wordVec;
    //revision
    //line.wordVec_.clear();
    //for (int i = 0; i < wordVec.size(); i++) {
    //    line.wordVec_.push_back(wordVec[i]);
    //}
    ////
}

void CSeqLEngLineBMSSWordDecoder :: seqleng_merge_special_line(SSeqLEngLineInfor & line){

    std::vector<SSeqLEngWordInfor> word_vec;
    int org_word_size = line.wordVec_.size();
    if (org_word_size <= 1) {
        return;
    }

    word_vec.push_back(line.wordVec_[0]);
    bool is_web = false; //this flag is for website: www.abc.com

    int k = 0;
    for (int i = 1; i < org_word_size; ++i) {
        if (line.wordVec_[i].wordStr_.length() > 0) {
            if ((i < org_word_size - 1)){ 
                if (((!strcmp(line.wordVec_[i - 1].wordStr_.c_str(), "w")) || \
                (!strcmp(line.wordVec_[i - 1].wordStr_.c_str(), "W"))) && \
                ((!strcmp(line.wordVec_[i].wordStr_.c_str(), "w")) || \
                (!strcmp(line.wordVec_[i].wordStr_.c_str(), "W"))) && \
                ((!strcmp(line.wordVec_[i + 1].wordStr_.c_str(), "w")) || \
                (!strcmp(line.wordVec_[i + 1].wordStr_.c_str(), "W")))
                ){
                is_web = true; //this line is a website
                }
            }

            //Since language model throws away "'", makes "it's" to be "it s"
            //If a single "s" appear after an english word, there should be a missing "'" 
            bool is_eng_word = false;  
            if (line.wordVec_[i - 1].wordStr_.length() > 1) {
                is_eng_word = true;
                for (int n = 0; n < line.wordVec_[i - 1].wordStr_.length(); ++n) {
                    is_eng_word = is_eng_word && (SeqLEngDecodeIsEnglishChar(line.wordVec_[i - 1].wordStr_[n]));
                }
            }

            if ((((!strcmp(line.wordVec_[i].wordStr_.c_str(), "s")) \
                || (!strcmp(line.wordVec_[i].wordStr_.c_str(), "S")) \
                ) && is_eng_word) || is_web
              ) {
                bool large_space_flag = false;

                SSeqLEngWordInfor new_word = word_vec[k];
                int new_right = word_vec[k].left_ + word_vec[k].wid_;
                int new_bottom = word_vec[k].top_ + word_vec[k].hei_;
                new_word.left_ = std::min(line.wordVec_[i].left_, new_word.left_);
                new_word.top_  = std::min(line.wordVec_[i].top_, new_word.top_);
                if (new_right + 2 * spaceConf_.meanBoxSpace_ < line.wordVec_[i].left_) {
                    large_space_flag = true;
                }

                new_right = std::max(line.wordVec_[i].left_+line.wordVec_[i].wid_, new_right);
                new_bottom = std::max(line.wordVec_[i].top_+line.wordVec_[i].hei_, new_bottom);
                
                new_word.wid_ = new_right - new_word.left_;
                new_word.hei_ = new_bottom - new_word.top_;

                if (is_web == false) {
                    new_word.wordStr_ += "'";
                }

                new_word.wordStr_ += line.wordVec_[i].wordStr_;
               
                if (large_space_flag == false) {
                    word_vec[k] = new_word;
                    continue;
                }
            }
          }
        SSeqLEngWordInfor new_word2 = line.wordVec_[i];
        word_vec.push_back(new_word2);
        ++k;
    }
    line.wordVec_ = word_vec;
    //revision
    //line.wordVec_.clear();
    //for (int i = 0; i < word_vec.size(); i++) {
    //    line.wordVec_.push_back(word_vec[i]);
    //}
    ////
}


bool SeqLEngIsAValidWord( string word, esp_engsearch *g_pEngSearchRecog )
{
	string tempWord = word;
    if( SeqLLineDecodeIsPunct(tempWord[tempWord.length()-1] )){
       tempWord.erase(tempWord.begin() + tempWord.length()-1); 
    }
    if(tempWord.length()==0){
      return false;
    }

	pthread_t threadID = pthread_self();
	vector<pair<string, double> > output;

	transform( tempWord.begin(), tempWord.end(), tempWord.begin(), ::tolower );

	g_pEngSearchRecog->esp_get( (threadType)threadID, tempWord.c_str(), output, 0, 1 );

	if( output.size() > 0 )
	{
		if( strcmp( tempWord.c_str(), output[0].first.c_str() ) == 0 )
			return true;
	}

	return false;
}

void RngEngMergeTwoWord( SSeqLEngWordInfor & word1, SSeqLEngWordInfor & word2 )
{
    int maxRight = std::max(word1.left_+word1.wid_, word2.left_+word2.wid_);
    int maxBottom = std::max(word1.top_+word1.hei_, word2.top_+word2.hei_);
	word1.left_  = std::min( word1.left_, word2.left_);
	word1.top_   = std::min( word1.top_, word2.top_ );
	word1.wordStr_ += word2.wordStr_;
}


void CSeqLEngLineBMSSWordDecoder :: SeqLEngMergeAValidCloseWord( SSeqLEngLineInfor & line, esp_engsearch *g_pEngSearchRecog )
{
	if( line.wordVec_.size() <= 1  ){
		return;
    }
  
   float MaxWordDist = 0;
   for( int i=0; i<line.wordVec_.size()-1; i++ )
   {
            MaxWordDist +=  line.wordVec_[i+1].left_ - (line.wordVec_[i].left_+line.wordVec_[i].wid_-1) + 1;
   }
   MaxWordDist = MaxWordDist/(line.wordVec_.size()-1);

    for( int i=0; i<line.wordVec_.size()-1; i++ )
	{
        if(i>=( line.wordVec_.size()-1) ) break;

		float betWordDist = line.wordVec_[i+1].left_ - (line.wordVec_[i].left_+line.wordVec_[i].wid_-1) + 1;

		if( betWordDist > 2*spaceConf_.meanBoxSpace_ ){
			continue;
        }
        
        if(line.wordVec_[i].wordStr_.length()==0){
            line.wordVec_.erase( line.wordVec_.begin()+i);
            i--;
            continue;
        }
        
        if(line.wordVec_[i+1].wordStr_.length()==0){
            continue;
        }

        if( SeqLLineDecodeIsPunct(line.wordVec_[i].wordStr_[ line.wordVec_[i].wordStr_.length() -1]) ){
            continue;
        }
        
        if( (!SeqLEngDecodeIsEnglishChar( line.wordVec_[i].wordStr_[ line.wordVec_[i].wordStr_.length() -1])) \
                || (!SeqLEngDecodeIsEnglishChar(line.wordVec_[i+1].wordStr_[0] ))){
            continue;
        }

		string mergedWord = line.wordVec_[i].wordStr_ + line.wordVec_[i+1].wordStr_;

		bool IsValidWord1 = SeqLEngIsAValidWord( line.wordVec_[i].wordStr_, g_pEngSearchRecog );
		bool IsValidWord2 = SeqLEngIsAValidWord( line.wordVec_[i+1].wordStr_, g_pEngSearchRecog );
		bool IsValidWord  = SeqLEngIsAValidWord( mergedWord, g_pEngSearchRecog );

		if(((!IsValidWord1 && !IsValidWord2) ) && IsValidWord )
		{
			RngEngMergeTwoWord( line.wordVec_[i], line.wordVec_[i+1] );
			line.wordVec_.erase( line.wordVec_.begin()+(i+1) );
            //std::cout << " SeqLEngMergeAValidCloseWord ..1. " << std::endl;
			i --;
		}
        else if( ((betWordDist < 0.8*spaceConf_.meanBoxSpace_ ) && IsValidWord ) )
		{
			RngEngMergeTwoWord( line.wordVec_[i], line.wordVec_[i+1] );
			line.wordVec_.erase( line.wordVec_.begin()+(i+1) );
            //std::cout << " SeqLEngMergeAValidCloseWord ..2. " << std::endl;
			i --;
		}/*
        else if((betWordDist <=  (1.0*MAX( charDist1, charDist2 )+1) ) && ( (IsValidWord) || (IsValidWord1 && IsValidWord2)) && (sent->words.size()==2) && (strlen(mergedWord.c_str())<12))
		{
			MergeTwoWord( &sent->words[i], &sent->words[i+1] );
			sent->words.erase( sent->words.begin()+(i+1) );
			i --;
		}
        else if( ((betWordDist <= 0.75*MaxWordDist) && betWordDist <= 1.5*MAX(charDist1, charDist2)) && (sent->words.size()>2) && (highFlagStr1 && highFlagStr2))
		{
			MergeTwoWord( &sent->words[i], &sent->words[i+1] );
			sent->words.erase( sent->words.begin()+(i+1) );
			i --;
		}*/

	}

}

void CSeqLEngLineBMSSWordDecoder :: SeqLEngHighLowOutput( SSeqLEngLineInfor & line){
    int upWordNum = 0;
    for(unsigned int i=0; i<line.wordVec_.size(); i++){
       SSeqLEngWordInfor & tempWord = line.wordVec_[i];
	   transform( tempWord.wordStr_.begin(), tempWord.wordStr_.end(), tempWord.wordStr_.begin(), ::tolower );
       /*
       if(tempWord.highLowType_==2){
         upWordNum++ ;
       }*/
        if (tempWord.highLowType_==2) {
            transform(tempWord.wordStr_.begin(), tempWord.wordStr_.end(), tempWord.wordStr_.begin(), ::toupper);
        } 
        else if (tempWord.wordStr_.length() > 0 && tempWord.highLowType_ == 1) {
            transform(tempWord.wordStr_.begin(), tempWord.wordStr_.begin() + 1, \
                    tempWord.wordStr_.begin(), ::toupper);
        }
    }
    /*
    if (line.wordVec_.size()>0) {
        if (line.wordVec_[0].wordStr_.length()>0 && line.wordVec_[0].highLowType_==1) {
	        transform( line.wordVec_[0].wordStr_.begin(), line.wordVec_[0].wordStr_.begin()+1,\
                line.wordVec_[0].wordStr_.begin(), ::toupper );
        }
    }
    if (upWordNum > 0.7 * line.wordVec_.size()) {
        for (unsigned int i = 0; i < line.wordVec_.size(); i++) {
            SSeqLEngWordInfor & tempWord = line.wordVec_[i];
	        transform( tempWord.wordStr_.begin(), tempWord.wordStr_.end(), tempWord.wordStr_.begin(), ::toupper );
        }
       
    }*/
}

void CSeqLEngLineBMSSWordDecoder :: enghighlowoutput_att( SSeqLEngLineInfor & line){
    int upWordNum = 0;
    for (unsigned int i = 0; i < line.wordVec_.size(); i++) {
       SSeqLEngWordInfor & tempWord = line.wordVec_[i];
	   transform( tempWord.wordStr_.begin(), tempWord.wordStr_.end(), tempWord.wordStr_.begin(), ::tolower );
       if (tempWord.highLowType_ == 2) {
         upWordNum++ ;
       }
        if (line.wordVec_[i].wordStr_.length() > 0 && line.wordVec_[i].highLowType_ == 1) {
	        transform( line.wordVec_[i].wordStr_.begin(), line.wordVec_[i].wordStr_.begin() + 1,\
                line.wordVec_[i].wordStr_.begin(), ::toupper );
        }
        if (line.wordVec_[i].wordStr_.length() > 0 && line.wordVec_[i].highLowType_ == 2) {
	        transform( line.wordVec_[i].wordStr_.begin(), line.wordVec_[i].wordStr_.end(),\
                line.wordVec_[i].wordStr_.begin(), ::toupper );
        }
    }
    if (upWordNum > 0.7 * line.wordVec_.size()) {
        for (unsigned int i = 0; i < line.wordVec_.size(); i++) {
            SSeqLEngWordInfor & tempWord = line.wordVec_[i];
	        transform( tempWord.wordStr_.begin(), tempWord.wordStr_.end(), tempWord.wordStr_.begin(), ::toupper );
        }
       
    }
}
void CSeqLEngLineBMSSWordDecoder::engacchighlowoutput(SSeqLEngLineInfor& line){
    int upWordNum = 0;
    for (unsigned int i = 0; i < line.wordVec_.size(); i++) {
       SSeqLEngWordInfor & tempWord = line.wordVec_[i];
	   transform( tempWord.wordStr_.begin(), tempWord.wordStr_.end(), tempWord.wordStr_.begin(), ::tolower );
       if (tempWord.maxVec_.size() > 0) {
        for (int j = 0; j < std::min(tempWord.maxVec_.size(), tempWord.wordStr_.size()); j++) {
            if (tempWord.maxVec_[j] >= 32 && tempWord.maxVec_[j] <= 57) {
                transform( tempWord.wordStr_.begin() + j, tempWord.wordStr_.begin() + j + 1, tempWord.wordStr_.begin() + j, ::toupper);
            }
        }
       }
    }
}

int CSeqLEngLineBMSSWordDecoder::loc_rm_quote(SSeqLEngWordInfor & word) {
    int num_quote = 0; 
    int quote_pos = -1;
    for (int i = 0; i < word.wordStr_.size(); i++) {
        if (word.wordStr_[i] == '\'') {
            num_quote ++;    
            quote_pos = i;
        }
    }
    if (num_quote == 1) {
        word.quote_pos_ = quote_pos;
        //std::cout << "before erase: " << word.wordStr_ << std::endl;
        word.wordStr_.erase(quote_pos, 1);
        //std::cout << "after erass: " << word.wordStr_ << std::endl;
        
    } else {
        word.quote_pos_ = -1;
    }
    return 0;
}

int CSeqLEngLineBMSSWordDecoder::recover_quote(SSeqLEngLineInfor & line) {
    const char * tmp = "\'";
    for (int i = 0; i < line.wordVec_.size(); i++) {
        if (line.wordVec_[i].quote_pos_ >= 1) {
            //std::cout << "quote pos: " << line.wordVec_[i].quote_pos_ << std::endl;
            //std::cout << "word string: " << line.wordVec_[i].wordStr_ << std::endl;
            if (line.wordVec_[i].quote_pos_ < line.wordVec_[i].wordStr_.size()) {
                line.wordVec_[i].wordStr_.insert(line.wordVec_[i].quote_pos_, tmp);
            } else if (line.wordVec_[i].quote_pos_ == line.wordVec_[i].wordStr_.size()) {
                line.wordVec_[i].wordStr_.append(tmp);
            }
        }
    } 
    return 0;
}
/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
