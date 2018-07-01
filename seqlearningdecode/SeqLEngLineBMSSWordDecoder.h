/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file SeqLEngLineBMSSWordDecoder.h
 * @author hanjunyu(com@baidu.com)
 * @date 2014/06/19 15:18:24
 * @brief 
 *  
 **/

#ifndef  __SEQLENGLINEBMSSWORDDECODER_H_
#define  __SEQLENGLINEBMSSWORDDECODER_H_

// English line decoder based on beam search algorithom (BMS)
// The search space is malloc when initialztion, so we call a static decoder,(S)

#include "SeqLEngLineDecodeHead.h"
#include <algorithm>
#include <math.h>
#include "SeqLEngLineBMSSWordDecoder.h"
#include "esc_searchapi.h"
#include "SeqLEngWordRecog.h"

struct SSeqLEngWordCandis{
  std::vector<string> strVec_;
  std::vector<string> rmPunctStrVec_;
  std::vector<int> editDistVec_;
  std::vector<float> checkScoreVec_;
  std::vector<float> regLogProbVec_; 
  std::vector<int> highLowTypeVec_;
  std::vector<float> edit_dis_vec_;
  int candiNum_;
  float checkScore_;
    std::vector<int> quote_pos_vec_;
};

// structure of node
struct SSeqLEngDecodeNode {
    int comp_;    // start component
    int candi_; 
    int endPos_;  // save the end postion for estimate the path extend
    int depth_;
    double avSpace_;
    double lmScore_;
    double lmScore0_;
    double cdScore_;
    double spScore_;
    double spScore2_;
    double sjScore_;
    double edit_dis_score_;

    int startPos_; // the initial position xie shufu add 
    double lcScore_; // xie shufu add, score of language category
    double avCHWidth_; // the average width of Chinese characters
    int iCHDepth_; // the depth of Chinese characters
    double CHWidthScore_; // the score of char width
    double occupyScore_; // xieshufu add the score about one row including how many characters
    double acOccupyScore_; // the accumulate score 
    int iLcType; // the language category of this node
    int iWordDepth;
    double acLcScore_; // xie shufu add the accumulative score of language category
    double acCHWidthScore_; // the score of character width
    //
    double sumScore_;  // xie shufu add the sum of all the score
    double avgScore_;  // xie shufu add the average of all the score
    int lm_parent_;
    int lm_word_len_; // max = 6
    std::string lm_word_str_;
    std::string lm_leaf_str_; // xie shufu add to save the char at the leaf node
    
    double regScore_;
    double mulRegScore_;
    double acLmScore_;
    double acLmScore0_;
    double acCdScore_;
    double acSpScore_;
    double acSpScore2_;
    double acSjScore_;
    double acRegScore_;
    double acMulRegScore_;
    double ac_edit_dis_score_;
    double score_;    // accumulated cost
    int parent_;
    LangModel::CalIterWrapper *lmHist_;
};

bool bigger_score_first(const SSeqLEngDecodeNode & n1, const SSeqLEngDecodeNode &n2);

class CSeqLEngLineBMSSWordDecoder : public CSeqLEngLineDecoder{
    public:
        CSeqLEngLineBMSSWordDecoder();
        virtual ~CSeqLEngLineBMSSWordDecoder();
        virtual SSeqLEngLineInfor decodeLineFunction(SSeqLEngLineInfor & inputLine);
        virtual SSeqLEngLineInfor decodelinefunction_att(SSeqLEngLineInfor & inputLine);
        void set_resnet_strategy_flag(bool flag);
    
    protected:
       SSeqLEngLineInfor inputLine_;
       SSeqLEngLineInfor outputLine_;
       std::vector<SSeqLEngWordCandis> candisVec_;
       std::vector<SSeqLEngDecodeNode> nodeVec_;
       
       float lmWeight_;
       float lmWeight0_;
       float cdWeight_;
       float spWeight_;
       float sjWeight_;
       float spWeight2_;
       float regWeight_;
       float mulRegWeight_;
       float edit_dis_weight_;
       
       int shortLineMaxWordNum_;
       float shortLmDiscount_;

       int minLeft_;
       int maxRight_;
       int minTop_;
       int maxBottom_;
       int lineHei_;
       int lineWid_;
       
       unsigned int beamWidth_;
       unsigned int nodeExtStep_;
       unsigned int spellCheckNum_;

       unsigned int calInitNodeNum( ){
          return inputLine_.wordVec_.size()*beamWidth_ + \
              4*beamWidth_*(spellCheckNum_+1) + 1;
       }
        
       void printPath(int nodeNum);
       void catCandisVectorToLine(SSeqLEngLineInfor & outputLine);
       void initDecodeNodes( );
       void extendDecodeNodes( );
       void releaseDecodeNodes( );
       void calLeftRightTopBottom( );
       bool havePathRelations( SSeqLEngDecodeNode & node, SSeqLEngWordInfor & word );
       bool havePathRelations( int wordIndex1, int wordIndex2);
        bool havepathrelations_att(int wordIndex1, int wordIndex2);
       bool isStartWord(SSeqLEngWordInfor & word, int leftSpace);
       bool isEndWord(SSeqLEngWordInfor & word, int rightSpace);
       void trackBestPathToOutput( int nodeNum );
        double calculate_cand_prob(std::string cand_str, SSeqLEngWordInfor word);
        bool havepathrelations(int wordIndex1, int wordIndex2);

       int stringAlignment(std::string str1, std::string str2, \
      bool backtrace = true, int sp = 1, int dp = 1, int ip = 1);

       void beamSearchDecode( );


      void SeqLEngRemoveBoundaryNoisyText( SSeqLEngLineInfor & line, int imgWid, int imgHei,\
              esp::esp_engsearch *g_pEngSearchRecog );
      void SeqLEngRemoveNoisyWords( SSeqLEngLineInfor & line, int imgWid, int imgHei, esp::esp_engsearch *g_pEngSearchRecog );
      void SeqLEngPostPuntDigWordText(SSeqLEngLineInfor & line );
      void SeqLEngMergeCloseWordText( SSeqLEngLineInfor & line, esp::esp_engsearch *g_pEngSearchRecog );
      void SeqLEngMergeHyphenCloseWordText(SSeqLEngLineInfor & line );
        void seqleng_merge_special_line(SSeqLEngLineInfor & line);
      void SeqLEngMergeAValidCloseWord( SSeqLEngLineInfor & line, esp::esp_engsearch *g_pEngSearchRecog );
      void extractCandisVector();
      void SeqLEngHighLowOutput( SSeqLEngLineInfor & line);
        void enghighlowoutput_att(SSeqLEngLineInfor& line);
        void engacchighlowoutput(SSeqLEngLineInfor& line);
      int loc_rm_quote(SSeqLEngWordInfor & word);
      int recover_quote(SSeqLEngLineInfor & word);
 
      bool _remove_head_end_noise;
      void expand_candidate();
      void decode();
        void expand_candidate_att();
        void decode_att();
        void set_attention_param();
        bool _m_resnet_strategy_flag;
};  














#endif  //__SEQLENGLINEBMSSWORDDECODER_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
