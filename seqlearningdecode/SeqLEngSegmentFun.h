/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file SeqLSeqLESegSegmentFun.h
 * @author hanjunyu(com@baidu.com)
 * @date 2014/07/05 09:29:16
 * @brief 
 *  
 **/




#ifndef  __SEQLENGSEGMENTFUN_H_
#define  __SEQLENGSEGMENTFUN_H_

#include "cv.h"
#include "highgui.h"
#include <vector>
#include "SeqLDecodeDef.h"

#define MAX_SEQLESEG(a,b) ((a>b)?(a):(b))
#define MIN_SEQLESEG(a,b) ((a<b)?(a):(b))
#define SEQLESEG_FLOAT_INF 1E10

struct SSeqLESegRect{
	int left, right, top, bottom;
	bool flag;
    float gapConf;
	int  segFlag;
};

struct SeqLESegDecodeRect{
	int left, right, top, bottom;
	int isBlackWdFlag;
	std::vector<SSeqLESegDetectWin> winSet;

    // the range for the top and bottom side of the row
    int extendTop;
    int extendBot;
};

struct SSeqLEngSegmentInfor
{
    unsigned short lineWidth;
    unsigned short lineHeight;
    unsigned short coarseBlockNum;
    unsigned short coarseAvgBlockW;
    unsigned short coarseAvgBlockH;
    float coarseAvgBlockSpace;
    unsigned short fineBlockNum;
    unsigned short fineAvgBlockW;
    unsigned short fineAvgBlockH;
    float fineAvgBlockSpace;
    float gapThr1;
    float gapThr2;
};

inline int  SeqLESegFind(int *E, int i);
inline void SeqLESegSet(int *E, int i, int root);
inline int  SeqLESegUnion(int *E, int i, int j);


int  SeqLESegCachePixels(int w, int h, unsigned char *im, unsigned char b, int *que);
int  SeqLESegLabelObjects(int len, int *que, int w, unsigned char *im, int *lbl, int *sum);
void SeqLESegBoundObjects(int len, int *que, int w, int *lbl, int num, SSeqLESegRect *bnd);





void seqle_seg_char_segment(unsigned char *im, int wid, int hei, SeqLESegDecodeRect *line, 
    bool extend_flag, bool dirFlag, SSeqLESegRect *&rects, 
    int &rectNum, SSeqLEngSegmentInfor &infor);

void SeqLESegLocalBin( unsigned char *im, int wid, int hei, unsigned char *bim, int isBlackWdFlag );
void SeqLESegLocalBin2( unsigned char *im, int wid, int hei, unsigned char *bim, unsigned char *bim2, int isBlackWdFlag );

void SeqLESegGenIntMap( unsigned char *im, int wid, int hei, long *intMap, double *int2Map );
void SeqLESegNiblack( unsigned char *im, unsigned char *bim, int wid, int hei, long *intMap, double *int2Map, float rad );

SSeqLESegRect* SeqLESegRegionExtract( unsigned char *img, int wid, int hei, int *&lbl, int &rnum );
void SeqLESegNoisyRemoval( SSeqLESegRect *&rects, int &rectNum, int imgWidth, int imgHeight, std::vector<SSeqLESegDetectWin> &winSet, bool dirFlag );
void SeqLESegRectCount( SSeqLESegRect* rects, int& rectNum );
bool SeqLESegIsADotOfi( SSeqLESegRect *rect, int id, SSeqLESegRect *rects, int rectNum );
float SeqLESegHorOverlap( SSeqLESegRect *rect1, SSeqLESegRect *rect2 );
float SeqLESegHorOverlap( SSeqLESegRect *rect1, SSeqLESegDetectWin *rect2 );
float SeqLESegCompPosScore( SSeqLESegRect *rect, std::vector<SSeqLESegDetectWin> &winSet, int imgWidth, int imgHeight );

void SeqLESegMergeRect( SSeqLESegRect *&rects, int &rectNum, float over_th );
void SeqLESegSortRect( SSeqLESegRect *rects, int rectNum );

int SeqLESegMedianWidth( SSeqLESegRect *rects, int rectNum, int wid, int hei );
int SeqLESegAvgHeight( SSeqLESegRect *rects, int rectNum, int wid, int hei );
float SeqLESegStrokeWidth( unsigned char* image, int wid, int hei );
void SeqLESegSplitRect( SSeqLESegRect *&rects, int &rectNum, unsigned char *bim, unsigned char *gim, int wid, int hei, int isBlackWdFlag );
void SeqLESegSplitOneRect( SSeqLESegRect *&rects, int &rectNum, int k, unsigned char *bim, unsigned char *gim, int wid, int hei, int medWidth, int avgHeight, float strokeWidth, int isBlackWdFlag );
void SeqLESegAddRect( SSeqLESegRect *&rects, int &rectNum, int k, unsigned char *im, int wid, int hei, int *cuts, int cutNum );
SSeqLESegRect SeqLESegNewRect( unsigned char *im, int wid, int hei, int hs, int vs, int left, int right, int top, int bottom );

void SeqLESegFindCutPoint( unsigned char *bim, unsigned char *gim, int wid, int hei, int *cutPoints, int &cpNum, int medWidth, int avgHeight, float strokeWidth, int &segFlag, int isBlackWdFlag );
bool SeqLESegPPA( int *hist, int wid, int hei, float strokeWidth, int &cutPos, float &cutScore );

void SeqLESegCalcGaussCoef( float *&gaussCoefs, int &gaussLen, int wid, int hei, int avgHeight );
void SeqLESegSmoothHist( int *hist, int wid, int hei, float *gaussCoef, int gaussLen );
void SeqLESegCalcProjHist( unsigned char *bim, int wid, int hei, int *projHist, int type );

void SeqLESegRect2Word( SSeqLESegRect *&rects, int &rectNum , SSeqLEngSegmentInfor & infor);


void SeqLESegWordLineAdjust(std::vector<SSeqLESegDecodeSScanWindow> &lineSet, IplImage * pColorImg);
void SeqLESegWordLineMerge( std::vector<SSeqLESegDecodeSScanWindow> &lineSet );



// xie shufu add 20140415
// segment the row image and get the rectangles output
void seg_char_one_row(unsigned char *im, int wid, int hei, int minTop, int maxBot,\
        SeqLESegDecodeRect *line, bool dirFlag,\
        SSeqLESegRect *&rects, int &rectNum);

// merge the rectangle in one row
void merge_rect_one_row(SSeqLESegRect *&rects, int &rectNum);

// merge those CCs which are too small
void merge_small_rect(SSeqLESegRect *&rects, int &rectNum, int iLineH);

// remove those CCs which are noise CC
void noise_cc_removal(SSeqLESegRect *&rects, int &rectNum,\
         int imgHeight, bool dirFlag);
// get the CCs of one row image
void find_line_image_rect(unsigned char *lim, int lineWid, int lineHei, SeqLESegDecodeRect *line,\
        SSeqLESegRect *&rects, int &rectNum);

#endif  //__SEQLENGSEGMENTFUN_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
