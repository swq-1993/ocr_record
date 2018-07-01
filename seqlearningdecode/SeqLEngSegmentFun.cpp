/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file SeqLSeqLESegSegmentFun.cpp
 * @author panyifeng(com@baidu.com)
 * @date 2013/05/01 21:08:29
 * @brief 
 *  
 **/


#include "SeqLEngSegmentFun.h"

#include <memory.h>
#include <stdio.h>
#include "common_func.h"
#include "dll_main.h"

extern char g_pimage_title[256];
extern char g_psave_dir[256];
extern int g_iline_index;

char G_thisImageName[256];
//-------------------------------------------------------------------------------------------------
inline int SeqLESegFind(int *E, int i)
{
	int root = i;
	while(E[root]<root) root = E[root];
	return root;
}


//-------------------------------------------------------------------------------------------------
inline void SeqLESegSet(int *E, int i, int root)
{
	while(E[i]<i)
	{
		int j = E[i];
		E[i] = root;
		i = j;
	}
	E[i] = root;
}


//-------------------------------------------------------------------------------------------------
inline int SeqLESegUnion(int *E, int i, int j)
{
	int root = SeqLESegFind(E, i);
	if(i!=j)
	{
		int rootj = SeqLESegFind(E, j);
		if(root>rootj) root = rootj;
		SeqLESegSet(E, i, root);
		SeqLESegSet(E, j, root);
	}
	return root;
}


//-------------------------------------------------------------------------------------------------
int SeqLESegCachePixels(int w, int h, unsigned char *im, unsigned char b, int *que)
{
	// remove pixels at four sides
	int i, i1 = w*(h-1), wl = w-1, len = 0;	
	memset(im,    255-b, w);
	memset(im+i1, 255-b, w);
	for(i=w; i<i1; i+=w) im[i] = im[i+wl] = 255-b;

	// cache pixels with level b
	for(i=w; i<i1; i++) if(im[i]==b) que[len++] = i;	
	
	return len;
}


//-------------------------------------------------------------------------------------------------
int SeqLESegLabelObjects(int len, int *que, int w, unsigned char *im, int *lbl, int *sum)
{
	if(len==0) return 0;
	unsigned char b = im[que[0]];
	
	// an array to store the equivalence information among the labels
	int *E = new int[len+1];
//	printf( "len: %d\n", len );
	
	int i, j, wl = w-1, wr = w+1, m = 0;
	for(j=0; j<len; j++)
	{
		i = que[j];

		// decision tree
		if(im[i-w]==b) 
			lbl[i] = E[lbl[i-w]]; // copy(b)
		else
		{
			if(im[i-wl]==b)
			{
				if(im[i-wr]==b) 
					lbl[i] = SeqLESegUnion(E, lbl[i-wl], lbl[i-wr]); // copy(c,a)
				else
				{
					if(im[i-1]==b)
						lbl[i] = SeqLESegUnion(E, lbl[i-wl], lbl[i-1]); // copy(c,d)
					else
						lbl[i] = E[lbl[i-wl]]; // copy(c)
				}
			}
			else
			{
				if(im[i-wr]==b)
					lbl[i] = E[lbl[i-wr]]; // copy(a)
				else
				{
					if(im[i-1]==b)
						lbl[i] = E[lbl[i-1]]; // copy(d)
					else
						m = m+1, lbl[i] = m, E[m] = m; // new label
				}
			}
		}	
	}

	// relabel connect components
	for(i=1; i<=m; i++) E[i] = SeqLESegFind(E, i);
	int n = 0;
	for(i=1; i<=m; i++)
	{
		if(E[i]==i) n++, E[i] = n;
		else E[i] = E[E[i]];
	}
	for(i=1; i<=n; i++) sum[i] = 0;
	for(j=0; j<len; j++)
	{
		i = que[j];
		int lbli = E[lbl[i]];
		lbl[i] = lbli;
		sum[lbli]++;	
	}	
	
	if( E != NULL )
	{
		delete []E;
		E = NULL;
	}
	return n;
}


//-------------------------------------------------------------------------------------------------
void SeqLESegBoundObjects( int len, int *que, int w, int *lbl, int num, SSeqLESegRect *bnd ) 
{
	int i, j;
	for(i=0; i<num; i++)
	{
		bnd[i].left  = bnd[i].top = 10000;
		bnd[i].right = bnd[i].bottom = 0;
		bnd[i].flag  = true;
	}
	for(j=0; j<len; j++)
	{
		i = que[j];
		int x = i%w, y = i/w;
		SSeqLESegRect &r = bnd[lbl[i]-1];
		if(r.left>x) r.left = x;
		if(r.right<x) r.right = x;
		if(r.top>y) r.top = y;
		r.bottom = y;
	}
}




void SeqLESegCollectCoarseSegmentInfor(SSeqLESegRect *& rects, int &rectNum,int lineWid,int lineHei,SSeqLEngSegmentInfor &infor)
{
    // Get coarse infor
    infor.coarseBlockNum=rectNum;
    infor.coarseAvgBlockH=0;
    infor.coarseAvgBlockW=0;
    infor.coarseAvgBlockSpace=0.0;
    infor.lineWidth = lineWid;
    infor.lineHeight = lineHei;
    if(infor.coarseBlockNum>0)
    {
        int tempLeft = rects[0].left;
        int tempRight = rects[0].right;
        int tempTop = rects[0].top;
        int tempBottom = rects[0].bottom;
        int sumWidth=0,sumHeight=0;
        float sumSpace=0;
        for(int i=0; i<infor.coarseBlockNum; i++)
        {
            tempLeft=MIN_SEQLESEG(tempLeft, rects[i].left);
            tempTop=MIN_SEQLESEG(tempTop, rects[i].top);
            tempRight=MIN_SEQLESEG(tempRight, rects[i].right);
            tempBottom=MIN_SEQLESEG(tempBottom, rects[i].bottom);
            sumWidth += (rects[i].right-rects[i].left+1);
            sumHeight += (rects[i].bottom-rects[i].top+1);
            if(i>0 )
            {
                sumSpace += MAX_SEQLESEG(rects[i].left - rects[i-1].right,1);
                //printf("sumSpace=%f, rects[i].left=%d,rects[i].right=%d, rects[i-1].left=%d,rects[i-1].right=%d\n",sumSpace,rects[i].left,rects[i].right, rects[i-1].left,rects[i-1].right);
            }
        
        }
        infor.coarseAvgBlockW=sumWidth/infor.coarseBlockNum;
        infor.coarseAvgBlockH=sumHeight/infor.coarseBlockNum;
        if(infor.coarseBlockNum>1)
        {
            infor.coarseAvgBlockSpace = sumSpace/(infor.coarseBlockNum-1);
        }
        infor.lineWidth = tempRight - tempLeft +1;
        infor.lineHeight = tempBottom - tempTop +1;
    }
}

void SeqLESegCollectFineSegmentInfor(SSeqLESegRect *& rects, int &rectNum,int lineWid,int lineHei,SSeqLEngSegmentInfor &infor)
{
    // Get coarse infor
    infor.fineBlockNum=rectNum;
    infor.fineAvgBlockH=0;
    infor.fineAvgBlockW=0;
    if(infor.fineBlockNum>0)
    {
        int sumWidth=0,sumHeight=0;
        float sumSpace=0;
        for(int i=0; i<infor.fineBlockNum; i++)
        {
            sumWidth += (rects[i].right-rects[i].left+1);
            sumHeight += (rects[i].bottom-rects[i].top+1);
            if(i>0 )
                sumSpace += MAX_SEQLESEG(rects[i].left - rects[i-1].right,1);
        }
        infor.fineAvgBlockW=sumWidth/infor.fineBlockNum;
        infor.fineAvgBlockH=sumHeight/infor.fineBlockNum;
        if(infor.fineBlockNum>1)
        {
            infor.fineAvgBlockSpace = sumSpace/(infor.fineBlockNum-1);
        }
    }
}

// character over-segmentation
void seqle_seg_char_segment(unsigned char *im, int wid, int hei, SeqLESegDecodeRect *line, 
    bool extend_flag, bool dirFlag, 
    SSeqLESegRect *&rects, int &rectNum, SSeqLEngSegmentInfor &infor)
{
	unsigned char *lim, *bim;
	int *label = NULL;
	int lineWid, lineHei, ext;

	ext = 0;
    if (extend_flag)
    {
        ext = (line->bottom - line->top + 1) / 8;
        ext = MAX_SEQLESEG(2, ext);
    } 

	line->left   -= 2*ext;
	line->left    = MAX_SEQLESEG( 0, line->left );
	line->right  += 2*ext;
	line->right   = MIN_SEQLESEG( wid-1, line->right );
	line->top    -= ext;
	line->top     = MAX_SEQLESEG( 0, line->top );
	line->bottom += ext;
	line->bottom  = MIN_SEQLESEG( hei-1, line->bottom );

	lineWid = line->right - line->left + 1;
	lineHei = line->bottom - line->top + 1;
    /*std::cout<< "Init LineWid.." << lineWid << " lineHei.." << lineHei << std::endl; */ // for debug 
	rectNum = 0;
	if( lineWid <= 0 || lineHei <= 0 )
	{
		return;
	}

	lim     = new unsigned char[lineWid*lineHei]; 
	bim     = new unsigned char[lineWid*lineHei]; 

	for( int i=0; i<line->winSet.size(); i++ )
	{
		line->winSet[i].left   -= line->left;
		line->winSet[i].right  -= line->left;
		line->winSet[i].top    -= line->top;
		line->winSet[i].bottom -= line->top;
	}

	for( int i=0; i<lineHei; i++ )
		for( int j=0; j<lineWid; j++ )
			lim[i*lineWid+j] = im[(i+line->top)*wid+j+line->left];
    

	if(0)
    {
        SeqLESegLocalBin( lim, lineWid, lineHei, bim, line->isBlackWdFlag );

	    rects = SeqLESegRegionExtract( bim, lineWid, lineHei, label, rectNum );
    }
    else
    {
        unsigned char * bim2    = new unsigned char[lineWid*lineHei];
	    int *label2 = NULL;
	    SeqLESegLocalBin2( lim, lineWid, lineHei, bim, bim2, line->isBlackWdFlag );

	    rects = SeqLESegRegionExtract( bim, lineWid, lineHei, label, rectNum );

        int rectNum2     = 0;
        SSeqLESegRect *rects2 = NULL; 
        rects2 = SeqLESegRegionExtract( bim2, lineWid, lineHei, label2, rectNum2 );
    
        int valid1 = 0;
        for( int i=0; i<rectNum; i++ )
        {
            if( rects[i].bottom-rects[i].top < lineHei/3 )
                continue;
            int j;
            for( j=0; j<rectNum2; j++ )
            {
                if( rects2[j].top <= 2 || rects2[j].bottom >= lineHei-3 )
                continue;
                if( rects[i].top >= rects2[j].top && rects[i].bottom <= rects2[j].bottom && 
                    rects[i].left >= rects2[j].left && rects[i].right <= rects2[j].right )
                     break;
            }
            if( j == rectNum2 &&
                rects[i].top >= 2 && rects[i].bottom <= lineHei-3 )
            {
                //printf( "1: %d_%d_%d_%d(%d)\n", rects[i].left, rects[i].right, rects[i].top, rects[i].bottom, lineHei );
                valid1 ++;
            }
        }

        int valid2 = 0;
        for( int i=0; i<rectNum2; i++ )
        {
            if( rects2[i].bottom-rects2[i].top < lineHei/3 )
                continue;
            int j;
            for( j=0; j<rectNum; j++ )
            {
                if( rects[j].top <= 2 || rects[j].bottom >= lineHei-3 )
                    continue;       
                if( rects2[i].top >= rects[j].top && rects2[i].bottom <= rects[j].bottom && 
                    rects2[i].left >= rects[j].left && rects2[i].right <= rects[j].right )
                    break;
            }
            if( j == rectNum &&
                rects2[i].top >= 2 && rects2[i].bottom <= lineHei-3 )
            {
                //printf( "2: %d_%d_%d_%d(%d)\n", rects2[i].left, rects2[i].right, rects2[i].top, rects2[i].bottom, lineHei );
                valid2 ++;
            }
        
        }

        if( valid2 > 2 * valid1 )
        {
            int rectNum_temp    = rectNum2;
            rectNum2            = rectNum;
            rectNum             = rectNum_temp;
            SSeqLESegRect *rect_temp = rects2;
            rects2              = rects;
            rects               = rect_temp;
            unsigned char * tempBim = bim;
            bim = bim2;
            bim2 = tempBim;
            int *tempLabel = label;
            label = label2;
            label2 = tempLabel;

            if (line->isBlackWdFlag == 1) { 
                line->isBlackWdFlag = 0;
                //std::cout << "Change to line->isBlackWdFlag 0" << std::endl;
            }
            else
            {
                line->isBlackWdFlag = 1;
                //std::cout << "Change to line->isBlackWdFlag 1" << std::endl;
            }
        }

        
        if (rects2 != NULL) {
            delete []rects2;
            rects2 = NULL;
        }
    if (bim2 != NULL) {
        delete []bim2;
        bim2 = NULL;
    }
    if (label2 != NULL) {
        delete [] label2;
        label2 = NULL;
    }

    }

    
    /*
    for(int i=0; i<rectNum; i++)
    {
        rects[0].left = MAX_SEQLESEG(0,rects[0].left);
        rects[0].right = MAX_SEQLESEG(0,rects[0].right);
        rects[0].right = MIN_SEQLESEG(lineWid-1,rects[0].right);
        rects[0].top = MAX_SEQLESEG(0,rects[0].top);
        rects[0].bottom = MAX_SEQLESEG(0,rects[0].bottom);
        rects[0].bottom = MIN_SEQLESEG(lineHei-1,rects[0].bottom);
    }
*/
	SeqLESegNoisyRemoval( rects, rectNum, lineWid, lineHei, line->winSet, dirFlag );
	
    SeqLESegMergeRect( rects, rectNum, 0.6 );
    //std::cout << rectNum << "....." <<std::endl;
    
    SeqLESegSortRect( rects, rectNum );
    
    SeqLESegCollectCoarseSegmentInfor(rects, rectNum,lineWid,lineHei,infor);
    
    SeqLESegRect2Word( rects, rectNum,infor);
    
	
    //SeqLESegSortRect( rects, rectNum );

	//SeqLESegSplitRect( rects, rectNum, bim, lim, lineWid, lineHei, line->isBlackWdFlag );

	SeqLESegSortRect( rects, rectNum );

	SeqLESegCollectFineSegmentInfor( rects, rectNum, lineWid, lineHei, infor );

	if( rectNum > 0 )
	{
		SSeqLESegRect *tempRects = new SSeqLESegRect[rectNum];
		memcpy( tempRects, rects, sizeof(SSeqLESegRect)*rectNum );
		int goodNum=0;
		for( int i=0; i<rectNum; i++ )
		{
			/*if( i > 0 )
			{
				if( tempRects[i].left - tempRects[i-1].right > 1 )
					rects[i].left -= 2;
				else if( tempRects[i].left - tempRects[i-1].right > 0 )
					rects[i].left -= 1;
			}
			else
				rects[i].left -= 2;

			if( i < rectNum-1 )
			{
				if( tempRects[i+1].left - tempRects[i].right > 1 )
					rects[i].right += 2;
				else if( tempRects[i+1].left - tempRects[i].right > 0 )
					rects[i].right += 1;
			}
			else
				rects[i].right += 2;*/

/*			rects[i].left   -= 2;
			rects[i].right  += 2;
			rects[i].top    -= 2;
			rects[i].bottom += 2;
*/            if(rects[i].right - rects[i].left <5 && rects[i].bottom-rects[i].top<5)
                continue;
            if( rects[i].right - rects[i].left <0.5*infor.fineAvgBlockW && rects[i].bottom-rects[i].top<0.5*infor.fineAvgBlockH)
                continue;

			rects[goodNum].left   = MAX_SEQLESEG( 0, rects[i].left );
			rects[goodNum].right  = MIN_SEQLESEG( lineWid-1, rects[i].right );
			rects[goodNum].top    = MAX_SEQLESEG( 0, rects[i].top );
			rects[goodNum].bottom = MIN_SEQLESEG( lineHei-1, rects[i].bottom );
		    goodNum++;
		}
        rectNum = goodNum;

		if( tempRects != NULL )
		{
			delete []tempRects;
			tempRects = NULL;
		}
	}

//---------------------------------------------------
/*    char g_szNiblackImgName[256] = "test-";
    IplImage *lineImg = cvCreateImage( cvSize(lineWid, lineHei), 8, 1 );
    std::cout << lineWid <<" " <<lineHei <<" "<< std::endl; 
    for( int i=0; i<lineHei; i++ )
        for( int j=0; j<lineWid; j++ )
            lineImg->imageData[i*lineImg->widthStep+j] = lim[i*lineWid+j];
    char linefn[256];
    sprintf( linefn, "./%s_%d_%d_lineimg.jpg",G_thisImageName, line->top, line->left );
    cvSaveImage( linefn, lineImg );
    cvReleaseImage( &lineImg );

    
	IplImage *binImg  = cvCreateImage( cvSize(lineWid, lineHei), 8, 1 );

	for( int i=0; i<lineHei; i++ )
		for( int j=0; j<lineWid; j++ )
			binImg->imageData[i*binImg->widthStep+j] = bim[i*lineWid+j];
    char binfn[256];
    sprintf( binfn, "./%s_%d_%d_binimg.jpg",G_thisImageName, line->top, line->left );
    cvSaveImage( binfn, binImg );

	CvScalar colors[] = {0};
    for( int i=0; i<rectNum; i++ )
	{
        std::cout << " binRect i"<< i << " l "<< rects[i].left << " r " << rects[i].right \
            <<" t" << rects[i].top << " b" << rects[i].bottom << std::endl;
		if( rects[i].flag )
		{
			cvRectangle(binImg, cvPoint(rects[i].left, rects[i].top),
			cvPoint(rects[i].right, rects[i].bottom), colors[0], 1);
		}
	}

	printf( "segimg: %s\n", g_szNiblackImgName );
    sprintf( binfn, "./%s_%d_%d_binimg2.jpg", G_thisImageName, line->top, line->left );
    
    cvSaveImage( binfn, binImg );
	cvReleaseImage( &binImg );
*/
 //--------------------------------------   

	if( label != NULL )
	{
		delete []label;
		label = NULL;
	}
	if( lim != NULL )
	{
		delete []lim;
		lim = NULL;
	}
	if( bim != NULL )
	{
		delete []bim;
		bim = NULL;
	}
}




// calculate median value of all aspect ratios
int SeqLESegMedianWidth( SSeqLESegRect *rects, int rectNum, int wid, int hei )
{
	int i, w, h, arNum;
	int *ars, medAspectRatio;
    if(rectNum<1) return 0;

	ars = new int[rectNum];

	arNum = 0;
	for( i=0; i<rectNum; i++ )
	{
		w = rects[i].right - rects[i].left + 1;
		h = rects[i].bottom - rects[i].top + 1;

		if( h > hei / 5 && w > MAX_SEQLESEG(2, hei / 10) )
		{
			//printf( "%d: %d %d %d\n", arNum, w, h, hei/5 );
			ars[arNum] = w;
			arNum ++;
		}
	}

	std::sort( ars, ars+arNum );

	medAspectRatio = ars[arNum/2];

	if( ars != NULL )
	{
		delete []ars;
		ars = NULL;
	}

	return medAspectRatio;
}

// calculate the median value of the character heights
int SeqLESegMedianHeight( SSeqLESegRect *rects, int rectNum, int wid, int hei )
{
	int i, w, h, arNum;
	int *ars, medAspectRatio;
    if(rectNum<1) return 0;
	ars = new int[rectNum];

	arNum = 0;
	for( i=0; i<rectNum; i++ )
	{
		w = rects[i].right - rects[i].left + 1;
		h = rects[i].bottom - rects[i].top + 1;

		if( h > hei / 5 && w > MAX_SEQLESEG(2, hei / 10) )
		{
			//printf( "%d: %d %d %d\n", arNum, w, h, hei/5 );
			ars[arNum] = h;
			arNum ++;
		}
	}

	std::sort( ars, ars+arNum );

	medAspectRatio = ars[arNum/2];

	if( ars != NULL )
	{
		delete []ars;
		ars = NULL;
	}

	return medAspectRatio;
}

// calculate average rect height
int SeqLESegAvgHeight( SSeqLESegRect *rects, int rectNum, int wid, int hei )
{
	int i, w, h, num;
	int avgHeight;

	avgHeight = 0;
	num       = 0;
	for( i=0; i<rectNum; i++ )
	{
		w = rects[i].right - rects[i].left + 1;
		h = rects[i].bottom - rects[i].top + 1;

		if( h > hei / 5 )
		{
			avgHeight += h;
			num ++;
		}
	}
	if( num > 0 )
		avgHeight /= num;

	avgHeight = MAX_SEQLESEG( hei / 5, avgHeight );

	return avgHeight;
}

// estimate stroke width of the component
float SeqLESegStrokeWidth( unsigned char* image, int wid, int hei )
{
	int black, contour;
	black = 0;
	contour = 0;

	unsigned char d1, d2, d3, d4;
	int i, j;
	for( i=0; i<hei; i++ )
		for( j=0; j<wid; j++ )
		{
			if( image[i*wid+j] )
				continue;

			black ++;

			d1 = (i==0)?	1:image[(i-1)*wid+j];
			d2 = (i==hei-1)?	1:image[(i+1)*wid+j];
			d3 = (j==0)?	1:image[i*wid+j-1];
			d4 = (j==wid-1)?	1:image[i*wid+j+1];
			if( d1 || d2 || d3 || d4 )
				contour ++;
		}

	return 2.0*black/contour;
}


// split one rectangle region
void SeqLESegSplitOneRect( SSeqLESegRect *&rects, int &rectNum, int k, unsigned char *bim, unsigned char *gim, int wid, int hei, int medWidth, 
					 int avgHeight, float strokeWidth, int isBlackWdFlag )
{
	int i, j, rw, rh;
	unsigned char *rim, *cgim;	
	SSeqLESegRect rt;

	rt  = rects[k];
	rw  = rt.right - rt.left + 1;
	rh  = rt.bottom - rt.top + 1;
	rim = new unsigned char[rw*rh];
	cgim = new unsigned char[rw*rh];

	for( i=rt.top; i<=rt.bottom; i++ )
	{
		for( j=rt.left; j<=rt.right; j++ )
		{
			if( bim[i*wid+j] == 0 )
				rim[(i-rt.top)*rw+j-rt.left] = 0;
			else
				rim[(i-rt.top)*rw+j-rt.left] = 1;
			cgim[(i-rt.top)*rw+j-rt.left] = gim[i*wid+j];
		}
	}

	int cutNum, cuts[20];
	cutNum = 0;

	SeqLESegFindCutPoint( rim, cgim, rw, rh, cuts, cutNum, medWidth, avgHeight, strokeWidth, rects[k].segFlag, isBlackWdFlag );

	if( cutNum > 0 )
	{
		SeqLESegAddRect( rects, rectNum, k, rim, rw, rh, cuts, cutNum );
	}

	delete []rim;
	delete []cgim;
}

// find cut points
void SeqLESegFindCutPoint( unsigned char *bim, unsigned char *gim, int wid, int hei, int *cutPoints, int &cpNum, int medWidth, 
					 int avgHeight, float strokeWidth, int &segFlag, int isBlackWdFlag )
{
	int *projHist;
	//int *diffVal;
	int gaussLen;
	float *gaussCoefs;

	cpNum = 0;

	projHist = new int[wid];
	memset( projHist, 0, sizeof(int)*wid );

	SeqLESegCalcProjHist( bim, wid, hei, projHist, 1 );

	SeqLESegCalcGaussCoef( gaussCoefs, gaussLen, wid, hei, avgHeight );

	SeqLESegSmoothHist( projHist, wid, hei, gaussCoefs, gaussLen );

	int cutPos;
	float cutScore = 1;

	SeqLESegPPA( projHist, wid, hei, strokeWidth, cutPos, cutScore );

	//if( ( wid > /*1.3*/1.85 * medWidth && cutScore != FLOAT_INF ) || cutScore < 0.3 || segFlag == 1 )  
    if(  /*cutScore < 0.1 ||*/ segFlag == 1 )
	{
		cutPoints[0] = cutPos;
		cpNum ++;
	}
	/*else if( segFlag == -1 && IsAMultiCharBox( gim, wid, hei, isBlackWdFlag ) && wid >= 3 )
	{
		cutPoints[0] = cutPos;
		cpNum ++;
	}*/

	if( gaussCoefs != NULL )
	{
		delete []gaussCoefs;
		gaussCoefs = NULL;
	}
	if( projHist != NULL )
	{
		delete []projHist;
		projHist = NULL;
	}
}

// calculate Gaussian coefficient
void SeqLESegCalcGaussCoef( float *&gaussCoefs, int &gaussLen, int wid, int hei, int avgHeight )
{
	int i;
	float val, sum, sigma;

	gaussLen   = MAX_SEQLESEG( avgHeight/8, 1 );
	gaussCoefs = new float[gaussLen];
	sigma      = (float)gaussLen / 4;

	sum = 0;
	for( i=0; i<gaussLen; i++ )
	{
		val  = float(i)/sigma;
		gaussCoefs[i] = (float)exp(-0.5*val*val );

		if( i==0 )
			sum += gaussCoefs[i];
		else
			sum += 2*gaussCoefs[i];
	}

	for( i=0; i<gaussLen; i++ )
	{
		gaussCoefs[i] /= sum;
		//printf( "%d %f\n", avgHeight, gaussCoefs[i] );
	}
}

// histogram smoothing
void SeqLESegSmoothHist( int *hist, int wid, int hei, float *gaussCoef, int gaussLen )
{
	int i, j, *tempHist;
	float sum;

	tempHist = new int[wid];
	memcpy( tempHist, hist, sizeof(int)*wid );

	for( i=0; i<wid; i++ )
	{
		sum = gaussCoef[0]*hist[i];
		for( j=1; j<gaussLen; j++ )
		{
			sum += gaussCoef[j]* ( tempHist[MAX_SEQLESEG(0, i-j)] + tempHist[MIN_SEQLESEG(wid-1, i+j)] );
		}
		hist[i] = (int)sum;
	}

	delete []tempHist;
}

// calculate project histogram based on stroke density
void SeqLESegCalcProjHist( unsigned char *bim, int wid, int hei, int *projHist, int type )
{
	int i, j;

	if( type == 0 )
	{
		for( i=0; i<hei; i++ )
		{
			for( j=0; j<wid; j++ )
			{
				if( bim[i*wid+j] == 0 )
					projHist[j] ++;
			}
		}
	}
	else if( type == 1 )
	{
		for( i=0; i<wid; i++ )
		{
			int start, end;
			start = 0;
			end   = 0;
			for( j=0; j<hei; j++ )
			{
				if( bim[j*wid+i] == 0 )
				{
					start = j;
					break;
				}
			}
			for( j=hei-1; j>=0; j-- )
			{
				if( bim[j*wid+i] == 0 )
				{
					end = j;
					break;
				}
			}
			projHist[i] += end-start+1;
		}
	}
}

// project profile analysis
bool SeqLESegPPA( int *hist, int wid, int hei, float strokeWidth, int &cutPos, float &cutScore )
{
	int i, minP, keyP, startP, endP;
	double minV, v, minCP;
	
	keyP   = wid / 2;	
	startP = wid / 5;
	endP   = wid - startP;

	minV   = SEQLESEG_FLOAT_INF;
	minCP  = SEQLESEG_FLOAT_INF;
	minP   = -1;
	for( i=startP+1; i<endP-1; i++ )
	{
		//if( hist[i] > 3. * m_strokeWid )
			//continue;
		
		v = ((float)hist[i])/wid + ((float)abs( i - keyP ))/wid;
		if( v < minV )
		{
			minV  = v;
			minP  = i;
			minCP = ((float)hist[i])/strokeWidth;
		}
	}

	cutPos   = minP;
	cutScore = minCP;

	return true;
}

// add new rects to rects set
void SeqLESegAddRect( SSeqLESegRect *&rects, int &rectNum, int k, unsigned char *im, int wid, int hei, int *cuts, int cutNum )
{
	int i, oldRectNum;
	SSeqLESegRect *tempRects;

	oldRectNum = rectNum;
	tempRects  = rects;
	rects      = NULL;
	rectNum   += cutNum;
	rects      = new SSeqLESegRect[rectNum];
	memcpy( rects, tempRects, sizeof(SSeqLESegRect)*oldRectNum );

	rects[k] = SeqLESegNewRect( im, wid, hei, tempRects[k].left, tempRects[k].top, 0, cuts[0], 0, hei-1 );
	for( i=0; i<cutNum-1; i++ )
	{
		rects[oldRectNum+i] = SeqLESegNewRect( im, wid, hei, tempRects[k].left, tempRects[k].top, cuts[i]+1, cuts[i+1], 0, hei-1 );
	}
	rects[oldRectNum+i] = SeqLESegNewRect( im, wid, hei, tempRects[k].left, tempRects[k].top, cuts[i]+1, wid-1, 0, hei-1 );

	delete []tempRects;
	tempRects = NULL;
}

// extract new rect
SSeqLESegRect SeqLESegNewRect( unsigned char *im, int wid, int hei, int hs, int vs, int left, int right, int top, int bottom )
{
	int i, j;
	SSeqLESegRect rt;

	rt.left   = MIN_SEQLESEG( left, right );
	rt.right  = MAX_SEQLESEG( left, right );
	rt.top    = MIN_SEQLESEG( top, bottom );
	rt.bottom = MAX_SEQLESEG( top, bottom );

	for( i=top; i<=bottom; i++ )
	{
		for( j=left; j<=right; j++ )
		{
			if( im[i*wid+j] == 0 )
			{
				rt.top = i;
				break;
			}
		}
		if( j != right+1 )
			break;
	}

	for( i=bottom; i>=top; i-- )
	{
		for( j=left; j<=right; j++ )
		{
			if( im[i*wid+j] == 0 )
			{
				rt.bottom = i;
				break;
			}
		}
		if( j != right+1 )
			break;
	}

	for( i=left; i<=right; i++ )
	{
		for( j=top; j<=bottom; j++ )
		{
			if( im[j*wid+i] == 0 )
			{
				rt.left = i;
				break;
			}
		}
		if( j != bottom+1 )
			break;
	}

	for( i=right; i>=left; i-- )
	{
		for( j=top; j<=bottom; j++ )
		{
			if( im[j*wid+i] == 0 )
			{
				rt.right = i;
				break;
			}
		}
		if( j != bottom+1 )
			break;
	}

	rt.left   += hs;
	rt.right  += hs;
	rt.top    += vs;
	rt.bottom += vs;
	rt.flag    = true;
	rt.segFlag = -1;

	return rt;
}

// sort rect from left-top to right-bottom
void SeqLESegSortRect( SSeqLESegRect *rects, int rectNum )
{
	SSeqLESegRect temp;
	int i, j;

	for( i=0; i<rectNum-1; i++ )
		for( j=i+1; j<rectNum; j++ )
		{
			if( rects[i].left>rects[j].left )
			{
				temp = rects[i];
				rects[i] = rects[j];
				rects[j] = temp;
			}
		}
}

bool IntCmp( int a, int b )
{
    return ( a < b );
}

void SeqLESegRect2Word( SSeqLESegRect *&rects, int &rectNum, SSeqLEngSegmentInfor & infor )
{
    for( int i=0; i<rectNum; i++ )
        rects[i].gapConf = 1.0f;

    if( rectNum <= 1 )
        return;

    std::vector<int> gaps;

    for( int i=0; i<rectNum-1; i++ )
    {
        /*if( (rects[i].right-rects[i].left) > 1.5 * (rects[i].bottom-rects[i].top) )
            gaps.push_back( 0 );
        if( (rects[i].right-rects[i].left) > 3.0 * (rects[i].bottom-rects[i].top) )
            gaps.push_back( 0 );*/
        if( (rects[i].right-rects[i].left) > 2.5 * infor.coarseAvgBlockH )
            gaps.push_back( 0 );
        if( (rects[i].right-rects[i].left) > 4.0 * infor.coarseAvgBlockH )
            gaps.push_back( 0 );
        gaps.push_back( MAX_SEQLESEG( 0, (rects[i+1].left-rects[i].right ) ) );
        //printf( "%d %d %d\n", gaps[i], rects[i].right, rects[i+1].left );
    }

    sort( gaps.begin(), gaps.end(), IntCmp ); 
    float gapThr1 = gaps[gaps.size()/3];
    float gapThr2 = gaps[gaps.size()*2/3];
    //gapThr1 *= 0.9;
    //gapThr2 /= 0.9;
    
    if(rectNum>20){
       gapThr2 /= 1.2;
    }
    /*float gapThr1 = 0.5*infor.coarseAvgBlockH/2.2;
    float gapThr2 = infor.coarseAvgBlockW/1.4;*/
    


    //for( int i=0; i<gaps.size(); i++ )
    //    printf( "%d\n", gaps[i] );

    for( int i=0; i<rectNum-1; i++ )
    {    
        SSeqLESegRect *rt1 = rects + i; 
        if( !rt1->flag )
            continue;
     
        for( int j=i+1; j<rectNum; j++ )
        {    
            SSeqLESegRect *rt2 = rects + j; 
            if( !rt2->flag )
                continue;

            float gap = rt2->left - rt1->right;

            //printf( "gap: %d %d %d\n", int(gap), int(gapThr1), int(gapThr2) );
            if( ( gap < 2.2 * gapThr1 ) &&
                ( gap < 1.3 * gapThr2 ) /*&&
                ( rectNum<10 || gap < 1.2*infor.coarseAvgBlockSpace)*/)
            //if( 1 )
            {
                //printf( "right: %d -> %d\n", rt1->right, rt2->right );
                rt1->left   = MIN_SEQLESEG( rt1->left, rt2->left );
                rt1->right  = MAX_SEQLESEG( rt1->right, rt2->right );
                rt1->top    = MIN_SEQLESEG( rt1->top, rt2->top );
                rt1->bottom = MAX_SEQLESEG( rt1->bottom, rt2->bottom );
                rt2->flag   = false;
            }
            else
            {
                break;
            }

        }
    }
    
    SeqLESegRectCount( rects, rectNum );

    SeqLESegSortRect( rects, rectNum );

    for( int i=0; i<rectNum-1; i++ )
    {
        if( gapThr1 > 0 )
            rects[i].gapConf = (rects[i+1].left-rects[i].right)/gapThr1;
        else
            rects[i].gapConf = 1.0;
    }
    rects[rectNum-1].gapConf = 1.0f;
    for( int i=0; i<rectNum; i++ )
    {
        //printf( "gapconf: %f\n", rects[i].gapConf );
    }

    infor.gapThr1 = gapThr1;
    infor.gapThr2 = gapThr2;
}

// merge overlapped rects
void SeqLESegMergeRect( SSeqLESegRect *&rects, int &rectNum, float over_th )
{
	double degree;
	int i, j;
	
	for( i=0; i<rectNum-1; i++ )
	{
		SSeqLESegRect *rt1 = rects + i;
		if( !rt1->flag )
			continue;
		
		int rw1 = rt1->right - rt1->left + 1;

		for( j=i+1; j<rectNum; j++ )
		{
			SSeqLESegRect *rt2 = rects + j;
			if( !rt2->flag )
				continue;

			int rw2 = rt2->right - rt2->left + 1;
			float ow = SeqLESegHorOverlap( rt1, rt2 );
            
			if( ow > over_th * MIN_SEQLESEG( rw1, rw2 ) )
			{
				rt1->left   = MIN_SEQLESEG( rt1->left, rt2->left );
				rt1->right  = MAX_SEQLESEG( rt1->right, rt2->right );
				rt1->top    = MIN_SEQLESEG( rt1->top, rt2->top );
				rt1->bottom = MAX_SEQLESEG( rt1->bottom, rt2->bottom );
				rt2->flag   = false;
			}
		}
	}

	SeqLESegRectCount( rects, rectNum );
}

// noisy rect removal
void SeqLESegNoisyRemoval( SSeqLESegRect *&rects, int &rectNum, int imgWidth, int imgHeight, std::vector<SSeqLESegDetectWin> &winSet, bool dirFlag )
{
	for( int i=0; i<rectNum; i++ )
	{
		if( !rects[i].flag )
			continue;

		int cx = ( rects[i].left + rects[i].right ) / 2;
		int cy = ( rects[i].top + rects[i].bottom ) / 2;
		int rw = rects[i].right - rects[i].left + 1;
		int rh = rects[i].bottom - rects[i].top + 1;

		if( dirFlag )
		{
			
			float score = SeqLESegCompPosScore( rects+i, winSet, imgWidth, imgHeight );

			/*if( score >= 1.0f )
			{
				if( rh < 0.5 * imgHeight )
				{
					rects[i].flag = false;
				}
			}
			else if( score >= 0.5f )
			{
				if( rects[i].top <= 1 || rects[i].top >= imgHeight-2 )
					rects[i].flag = false;
				else if( rh < 0.1 * imgHeight )
				{
					rects[i].flag = false;
				}
			}

			if( rects[i].left <= 1 || rects[i].right >= imgWidth-2 )
				rects[i].flag = false;*/

			/*if( cy <= imgHeight / 4 || cy >= imgHeight * 3 / 4 )
			{
				if( rects[i].top <= 1 || rects[i].bottom >= imgHeight-2 )
					rects[i].flag = false;
				else
				{
					if( cy <= imgHeight / 4 )
					{
						if( !SeqLESegIsADotOfi( rects+i, i, rects, rectNum ) )
						{
							rects[i].flag = false;
						}
						else
							rects[i].flag = true;
					}
				}
			}*/

            if( cy <= imgHeight / 4 || cy >= imgHeight * 3 / 4  )
            {
                rects[i].flag = false;
            }
            else if( MAX( rw, rh ) * 7 < imgHeight )
            {
                rects[i].flag = false;
            }

			/*if( cy <= imgHeight / 4 || cy >= imgHeight * 3 / 4 )
            {    
                if( rects[i].top <= 1 || rects[i].bottom >= imgHeight-2 )
                    rects[i].flag = false;
                else 
                {    
                    if( cy <= imgHeight / 4 )
                    {    
                        if( !SeqLESegIsADotOfi( rects+i, i, rects, rectNum )  )
                        {    
                            rects[i].flag = false;
                        }    
                        else if( MIN( rw, rh ) * 20 < imgHeight || MAX( rw, rh ) * 10 < imgHeight )
                        {    
                            rects[i].flag = false;
                        }    
                        else 
                            rects[i].flag = true;
                    }    
                    else if( cy > imgHeight *3 / 4 )
                    {    
                        if( MIN( rw, rh ) * 20 < imgHeight || MAX( rw, rh ) * 10 < imgHeight )
                        {    
                            rects[i].flag = false;
                        }    
                    }    
                }    
            }*/
		}
	}

	SeqLESegRectCount( rects, rectNum );
}

// confidence score for each component based on the relationships with its surrounding windowss
float SeqLESegCompPosScore( SSeqLESegRect *rect, std::vector<SSeqLESegDetectWin> &winSet, int imgWidth, int imgHeight )
{
	float wt, swt, score;
	float cx, cy, rad;

	int rcx = ( rect->left + rect->right ) / 2;
	int rcy = ( rect->top + rect->bottom ) / 2; 

	cx  = 0;
	cy  = 0;
	rad = 0;
	score = 100.0f;
	swt = 0;
	for( int i=0; i<winSet.size(); i++ )
	{
		SSeqLESegDetectWin *win = &winSet[i];

		float ow = SeqLESegHorOverlap( rect, win );

		if( ow <= 0 )
			continue;

		int wcx  = ( win->left + win->right ) / 2;
		int wcy  = ( win->top + win->bottom ) / 2;
		int wwid = ( win->right - win->left + 1 );
		int whei = ( win->bottom - win->top + 1 );
		float wt = ( rcx - wcx ) * ( rcx - wcx );
		wt += ( rcy - wcy ) * ( rcy - wcy );
		wt  = sqrt( wt );
		if( wt > 0 )
			wt = 1.0f/wt;

		cx  += wt * wcx;
		cy  += wt * wcy;
		rad += wt * ( sqrt( wwid * whei ) ) / 2;
		swt += wt;
		//printf( "--- %f %f %f---\n", rad, swt, rad/swt );
	}

	if( swt > 0 )
	{
		cx  /= swt;
		cy  /= swt;
		rad /= swt;

		//printf( "%f (%d) %f (%d) %f\n", cx, rcx, cy, rcy, rad );
		score = fabs( cy - rcy ) / rad;
	}

	return score;
}


// estimate if the component is a dot of the letter "i"
bool SeqLESegIsADotOfi( SSeqLESegRect *rect, int id, SSeqLESegRect *rects, int rectNum )
{
	int rw = rect->right - rect->left + 1;

	for( int i=0; i<rectNum; i++ )
	{
		if( i == id )
			continue;

		float ow = SeqLESegHorOverlap( rect, rects+i );
		if( ow > 0.5 * rw )
		{
			int ref_w = rects[i].right - rects[i].left + 1;
			int ref_h = rects[i].bottom - rects[i].top + 1;

			//printf( "%d %d %d %d\n", ref_w, ref_h, rw, int(ow) );
			if( ref_h < 2 * ref_w )
				continue;

			if( ref_w < 0.5 * rw )
				continue;

			return true;
		}
	}

	return false;
}

// overlap distance between two rects in horizontal
float SeqLESegHorOverlap( SSeqLESegRect *rect1, SSeqLESegDetectWin *rect2 )
{
	float w;

	if( rect2->right<rect1->left )
		w = rect2->right - rect1->left + 1;
	else if( rect1->right<rect2->left )
		w = rect1->right - rect2->left + 1;
	else 
	{
		if( rect1->right<=rect2->right )
		{
			if( rect1->left>=rect2->left )
				w = rect1->right - rect1->left + 1;
			else
				w = rect1->right - rect2->left + 1;
		}
		else
		{
			if( rect1->left>=rect2->left )
				w = rect2->right - rect1->left + 1;
			else
				w = rect2->right - rect2->left + 1;
		}				
	}

	return w;
}

float SeqLESegHorOverlap( SSeqLESegRect *rect1, SSeqLESegRect *rect2 )
{
	float w;

	if( rect2->right<rect1->left )
		w = rect2->right - rect1->left + 1;
	else if( rect1->right<rect2->left )
		w = rect1->right - rect2->left + 1;
	else 
	{
		if( rect1->right<=rect2->right )
		{
			if( rect1->left>=rect2->left )
				w = rect1->right - rect1->left + 1;
			else
				w = rect1->right - rect2->left + 1;
		}
		else
		{
			if( rect1->left>=rect2->left )
				w = rect2->right - rect1->left + 1;
			else
				w = rect2->right - rect2->left + 1;
		}				
	}

	return w;
}

// Update the number of rects
void SeqLESegRectCount( SSeqLESegRect* rects, int& rectNum )
{
	int tnum = 0;
	for( int k=0; k<rectNum; k++ )
	{
		if( rects[k].flag )
		{
			if( tnum<k )
			{
				rects[tnum] = rects[k];
                rects[tnum].flag = true;
			}
			tnum ++;
		}
	}
	rectNum = tnum;
}

// character localization
SSeqLESegRect* SeqLESegRegionExtract( unsigned char *img, int wid, int hei, int *&lbl, int &rnum )
{
	int wh = wid*hei;
	int *que = new int[wh];

	// cache black pixels
	int len = SeqLESegCachePixels( wid, hei, img, 0, que ) ;

	// label 8-connectivity components
	lbl = new int[wh];
	int *sum = new int[len+1];
	rnum = SeqLESegLabelObjects( len, que, wid, img, lbl, sum );

	// get bounding rectangles of connected components
	SSeqLESegRect *rects = NULL;
	
	if( rnum > 0 )
	{
		rects = new SSeqLESegRect[rnum];
		SeqLESegBoundObjects( len, que, wid, lbl, rnum, rects );

		for( int i=0; i<rnum; i++ )
		{
			rects[i].flag    = true;
			rects[i].segFlag = -1;
		}
	}
	
	delete []sum;
	delete []que;

	return rects;
}

// local binarization
void SeqLESegLocalBin( unsigned char *im, int wid, int hei, unsigned char *bim, int isBlackWdFlag )
{
	long   *intMap  = NULL;
	double *int2Map = NULL;

	intMap  = new long[(wid+1)*(hei+1)];
	int2Map = new double[(wid+1)*(hei+1)];

	SeqLESegGenIntMap( im, wid, hei, intMap, int2Map );

	SeqLESegNiblack( im, bim, wid, hei, intMap, int2Map, MIN_SEQLESEG( wid, hei )/4.0 );

	int len = wid * hei;
	if( isBlackWdFlag == 0 )
	{
		for( int i=0; i<len; i++ )
		{
			if( bim[i] < 200 )
				bim[i] = 255;
			else
				bim[i] = 0;
		}
	}
	else if( isBlackWdFlag == 1 )
	{
		for( int i=0; i<len; i++ )
		{
			if( bim[i] > 50 )
				bim[i] = 255;
			else
				bim[i] = 0;
		}
	}

	if( intMap != NULL )
	{
		delete []intMap;
		intMap = NULL;
	}
	if( int2Map != NULL )
	{
		delete []int2Map;
		int2Map = NULL;
	}
}


// local binarization
void SeqLESegLocalBin2( unsigned char *im, int wid, int hei, unsigned char *bim, unsigned char *bim2, int isBlackGdFlag )
{
	long   *intMap  = NULL;
	double *int2Map = NULL;

	intMap  = new long[(wid+1)*(hei+1)];
	int2Map = new double[(wid+1)*(hei+1)];

	SeqLESegGenIntMap( im, wid, hei, intMap, int2Map );

	SeqLESegNiblack( im, bim, wid, hei, intMap, int2Map, MIN_SEQLESEG( wid, hei )/4.0 );

	int len = wid * hei;
	if( isBlackGdFlag == 0 )
	{
		for( int i=0; i<len; i++ )
		{
            bim2[i] = bim[i];
			if( bim[i] < 200 )
				bim[i] = 255;
			else
				bim[i] = 0;
            
            if( bim2[i] > 50 )
                bim2[i] = 255;
            else
                bim2[i] = 0;
		}
	}
	else if( isBlackGdFlag == 1 )
	{
		for( int i=0; i<len; i++ )
		{
            bim2[i] = bim[i];
			if( bim[i] > 50 )
				bim[i] = 255;
			else
				bim[i] = 0;

            if( bim2[i] < 200 )
                bim2[i] = 255;
            else
                bim2[i] = 0;
		}
	}
	if( intMap != NULL )
	{
		delete []intMap;
		intMap = NULL;
	}
	if( int2Map != NULL )
	{
		delete []int2Map;
		int2Map = NULL;
	}
}





// generate integral map for gray-level image and gray-level square image
void SeqLESegGenIntMap( unsigned char *im, int wid, int hei, long *intMap, double *int2Map )
{
    int i, j, mwid, mhei, offset, value;

	mwid = wid+1;
	mhei = hei+1;

	memset( intMap, 0, sizeof(long)*mwid*mhei );
	memset( int2Map, 0, sizeof(double)*mwid*mhei );

	intMap[mwid+1]  = im[0];
	int2Map[mwid+1] = im[0]*im[0];
	for( i=mwid+2; i<2*mwid; i++ )
	{
		value = im[i-mwid-1];
        intMap[i]  = intMap[i-1]+value;
		int2Map[i] = int2Map[i-1]+value*value;
	}

	for( i=2; i<mhei; i++ )
	{
		offset = i*mwid+1;
		value  = im[offset-mwid-i];
		intMap[offset]  = intMap[offset-mwid]+value;
		int2Map[offset] = int2Map[offset-mwid]+value*value;
		for( j=1; j<mwid-1; j++ )
		{   
			offset++;       
			value = im[offset-mwid-i];
			intMap[offset]  = intMap[offset-1]+intMap[offset-mwid]-intMap[offset-mwid-1]+value;
            int2Map[offset] = int2Map[offset-1]+int2Map[offset-mwid]-int2Map[offset-mwid-1]
				+value*value;			 
		}
	}
}

// Niblack's local binarization
void SeqLESegNiblack( unsigned char *im, unsigned char *bim, int wid, int hei, long *intMap, double *int2Map, float rad )
{
	int i, j, xrad, yrad, top, bottom, left, right, area;
	int h1, h2, offset;
	double m, v, std, th1, th2, k, gv;
	float sc;

	k       = 0.43;
	offset  = 0;

	m = (intMap[0]+intMap[(wid+1)*(hei+1)-1]
		-intMap[hei*(wid+1)]-intMap[wid])/wid/hei;
	v = (int2Map[0]+int2Map[(wid+1)*(hei+1)-1]
		-int2Map[hei*(wid+1)]-int2Map[wid])/wid/hei;
	v  = v-m*m;
	gv = 0.5*sqrt( v );
	gv = MAX_SEQLESEG( (double)5, gv );

	for( i=0; i<hei; i++ )
	{
		for( j=0; j<wid; j++ )
		{	
			yrad = rad;
			xrad = rad;

			top    = MAX_SEQLESEG( 0, i-yrad );
			bottom = MIN_SEQLESEG( hei-1, i+yrad );
			left   = MAX_SEQLESEG( 0, j-xrad );
			right  = MIN_SEQLESEG( wid-1, j+xrad );
			area   = ( bottom-top ) * ( right-left );
			
			h1 = top*(wid+1);
			h2 = bottom*(wid+1);
			
			m = (intMap[h1+left]+intMap[h2+right]
				-intMap[h1+right]-intMap[h2+left])/area;
			v = (int2Map[h1+left]+int2Map[h2+right]
				-int2Map[h1+right]-int2Map[h2+left])/area;
			v = v-m*m;
			
			std = sqrt( v );
			std = MAX_SEQLESEG( std, gv );
	
			th1 = m+k*std;
			th2 = m-k*std;
			
			if( im[offset]>=th1 )
				bim[offset] = 255;
			else if( im[offset]<=th2 )
				bim[offset] = 0;
			else
				bim[offset] = 100;
			
			offset ++;
		}
	}
}


/*
void SeqLESegAtoAdjustLineToWords(IplImage * pColorImg,SSeqLESegDecodeSScanWindow & line1, SSeqLESegDecodeSScanWindow &line2 )
{
	//printf("Begin SeqLESegAtoAdjustLineToWords\n");
	SSeqLESegDecodeSScanWindow line;
	line.left = std::min(line1.left, line2.left);	
	line.top = std::min(line1.top, line2.top);	
	line.right = std::max(line1.right, line2.right);
	line.bottom = std::max(line1.bottom, line2.bottom);
	line.isBlackWdFlag=line1.isBlackWdFlag;
	IplImage *grayImg  = cvCreateImage( cvSize(pColorImg->width, pColorImg->height), 8, 1 );
	cvCvtColor( pColorImg, grayImg, CV_BGR2GRAY );	
	// step1: split decoder
	unsigned char *gim = NULL;
	int gw = grayImg->width;
	int gh = grayImg->height;
	gim = new unsigned char[gw*gh];
	for( int i=0; i<grayImg->height; i++ )
		for( int j=0; j<grayImg->width; j++ )
			gim[i*gw+j] = grayImg->imageData[i*grayImg->widthStep+j];

	int lineWidth         = line.right - line.left+1;
	int lineHeight        = line.bottom - line.top+1;
	int initMinlineWidth  = lineWidth < lineHeight ? lineWidth : lineHeight ;
	if(initMinlineWidth<5) return;
	//int maxWordWidth,min_left,max_right,min_top,max_bottom,minShift,maxNodeNum;
	BEAMSEARCHDECODER_ENG tempdecoder;
	tempdecoder.rect.left = line.left;	
	tempdecoder.rect.right = line.right;	
	tempdecoder.rect.top = line.top;	
	tempdecoder.rect.bottom = line.bottom;	
	tempdecoder.rect.isBlackWdFlag=line.isBlackWdFlag;
	if( lineHeight > lineWidth )
	    tempdecoder.conf.isHorFlag = false;
	else
	    tempdecoder.conf.isHorFlag = true;
	
	if(tempdecoder.conf.isHorFlag==true)
	{
		vector<int> cutVector;
		EngGetWordCutPoints( tempdecoder, gim, grayImg->width, grayImg->height, cutVector);
		//printf("End GetWordCutPoints\n");
		//fflush(stdout);
		if(cutVector.size()>1)
		{
		//int meanCutPoint = cutVector[(cutVector.size())/2];
		int meanCutPoint = cutVector[0];
		int centBoundry = (std::max(line1.left,line2.left) + std::min(line1.right,line2.right))*0.5;
		for(int i=1; i<cutVector.size(); i++)
		{
			if(abs(meanCutPoint-centBoundry)> abs(cutVector[i]-centBoundry))
				meanCutPoint = cutVector[i];
		}
		
		//printf("Find CutPoint %d\n",meanCutPoint);
		//fflush(stdout);
		//line1=line;
		//line2=line;
		if(line1.left < line2.left)
		{
			line1.right = meanCutPoint;
			line2.left = meanCutPoint;
		}
		else
		{
			line2.right = meanCutPoint;
			line1.left = meanCutPoint;
		}
	
			line1.top = std::min(line1.top, (line.bottom-line.top)*(meanCutPoint-line.left)/(line.right-line.left)+line.top);
			line1.bottom = std::max(line1.bottom, (line.bottom-line.top)*(meanCutPoint-line.left)/(line.right-line.left)+line.top);
			line2.top = std::min(line2.top, (line.bottom-line.top)*(meanCutPoint-line.left)/(line.right-line.left)+line.top);
			line2.bottom = std::max(line2.bottom, (line.bottom-line.top)*(meanCutPoint-line.left)/(line.right-line.left)+line.top);
			
			
		}
		
	}
	
	if( gim != NULL )
	{
		delete []gim;
		gim = NULL;
	}
	cvReleaseImage(&grayImg);
}
*/

void SeqLESegWordLineAdjust(std::vector<SSeqLESegDecodeSScanWindow> &lineSet, IplImage * pColorImg)
{
		// Combine Lines
        std::vector<SSeqLESegDecodeSScanWindow> tempLineSet;
        std::vector<bool> beCombineFlag;
		for(int i=0; i<lineSet.size(); i++)
		{
			beCombineFlag.push_back(false);
		}
		for(int i=0;i<lineSet.size();i++)
		{
			if(beCombineFlag[i]==true) continue;
			SSeqLESegDecodeSScanWindow tempLine = lineSet[i];
			for(int j=i+1; j<lineSet.size(); j++)
			{
				if(beCombineFlag[j]==true) continue;
				if(lineSet[i].isBlackWdFlag!=lineSet[j].isBlackWdFlag) continue;

				int overHor = (tempLine.right-tempLine.left)
					+(lineSet[j].right-lineSet[j].left)-(std::max(tempLine.right,lineSet[j].right) - 					std::min(tempLine.left,lineSet[j].left));
				int overVer = (tempLine.bottom-tempLine.top) +(lineSet[j].bottom-lineSet[j].top)
					-(std::max(tempLine.bottom,lineSet[j].bottom) -
					std::min(tempLine.top,lineSet[j].top) );
				//if(overHor>-5 && (overVer>0.7*std::min((tempLine.bottom-tempLine.top),(lineSet[j].bottom-lineSet[j].top)) /* || ( overVer>0.3*std::min((tempLine.bottom-tempLine.top),(lineSet[j].bottom-lineSet[j].top) ) && (lineSet[j].bottom-lineSet[j].top)>0.7*(tempLine.bottom-tempLine.top) && (lineSet[j].bottom-lineSet[j].top)<1.3*(tempLine.bottom-tempLine.top))*/ ) )
				if(overHor>-0.3*std::min((tempLine.bottom-tempLine.top),(lineSet[j].bottom-lineSet[j].top) ) && ((overVer>0.7*std::min((tempLine.bottom-tempLine.top),(lineSet[j].bottom-lineSet[j].top)) && (lineSet[j].bottom-
				 lineSet[j].top)>0.5*(tempLine.bottom-tempLine.top) && (lineSet[j].bottom-lineSet[j].top)<1.5*(tempLine.bottom-tempLine.top)) || ( overVer>0.5*std::min((tempLine.bottom-tempLine.top),(lineSet[j].bottom-lineSet[j].top) ) && (lineSet[j].bottom
				 -lineSet[j].top)>0.7*(tempLine.bottom-tempLine.top) && (lineSet[j].bottom-lineSet[j].top)<1.3*(tempLine.bottom-tempLine.top)) ) )
				{
					if(1)
					{
						tempLine.left = std::min(tempLine.left, lineSet[j].left);
						tempLine.top = std::min(tempLine.top, lineSet[j].top);
						tempLine.right = std::max(tempLine.right, lineSet[j].right);
						tempLine.bottom = std::max(tempLine.bottom, lineSet[j].bottom);
						beCombineFlag[j] = true;
					}
					else
					{
						// auto adjust line to word 
						//SeqLESegAtoAdjustLineToWords(pColorImg,lineSet[i], lineSet[j] );
					}

					//printf("Find Cross Line\n");	
				}
				
			}
			tempLineSet.push_back(tempLine);
		}
		lineSet = tempLineSet;

}


void SeqLESegWordLineMerge( std::vector<SSeqLESegDecodeSScanWindow> &lineSet )
{
	for( int i=0; i<lineSet.size(); i++ )
	{
		SSeqLESegDecodeSScanWindow *line1 = &lineSet[i];
		int lw1 = line1->right - line1->left + 1;
		int lh1 = line1->bottom - line1->top + 1;
		for( int j=i+1; j<lineSet.size(); j++ )
		{
			SSeqLESegDecodeSScanWindow *line2 = &lineSet[j];

			if( line1->isBlackWdFlag != line2->isBlackWdFlag )
				continue;

			int lw2 = line2->right - line2->left + 1;
			int lh2 = line2->bottom - line2->top + 1;

			int overTop    = MAX( line1->top, line2->top );
			int overBottom = MIN( line1->bottom, line2->bottom );
			int overLeft   = MAX( line1->left, line2->left );
			int overRight  = MIN( line1->right, line2->right );
			int overWid    = overRight - overLeft + 1;
			int overHei    = overBottom - overTop + 1;

			if( ( overHei > 0.5 * MIN( lh1, lh2 ) && overHei > 0.3 * MAX( lh1, lh2 ) && overWid > -0.5 * MIN( lh1, lh2 ) && overWid < 0.5 * MIN( lw1, lw2 ) ) ||
				( overHei > 0.98 * MIN( lh1, lh2 ) && overHei > 0.3 * MAX( lh1, lh2 ) && overWid > 0 ) ||
				( overHei > 0.8 * MIN( lh1, lh2 ) && overHei > 0.6 * MAX( lh1, lh2 ) && overWid > 0 ) )
			{
				line1->left   = MIN( line1->left, line2->left );
				line1->right  = MAX( line1->right, line2->right );
				line1->top    = MIN( line1->top, line2->top );
				line1->bottom = MAX( line1->bottom, line2->bottom );
				lineSet.erase( lineSet.begin()+j );
				break;
			}
		}
	}
}


// xie shufu add
// character over-segmentation
// segment the row image and get the rectangles output
void seg_char_one_row(unsigned char *im, int wid, int hei, int minTop, int maxBot, \
    SeqLESegDecodeRect *line, bool dirFlag,\
    SSeqLESegRect *&rects, int &rectNum)  {

    unsigned char *lim = NULL;
    int line_wid = wid;
    int line_hei = hei;
    int ext = 0;

    // horizontal row
    if (dirFlag) {
        ext = (line->bottom - line->top + 1) / 8;
        ext = MAX_SEQLESEG(2, ext);
    }

    line->left   -= 2*ext;
    line->left    = MAX_SEQLESEG(0, line->left);
    line->right  += 2*ext;
    line->right   = MIN_SEQLESEG(wid - 1, line->right);
    line->top    -= ext;
    line->top     = MAX_SEQLESEG(minTop, line->top);
    line->bottom += ext;
    line->bottom  = MIN_SEQLESEG(maxBot, line->bottom);

    line_wid = line->right - line->left + 1;
    line_hei = line->bottom - line->top + 1;
    rectNum = 0;
    if (line_wid <= 0 || line_hei <= 0) {
        return;
    }
    
    // get the cropped column/row image data 
    const int image_dim = line_wid*line_hei;
    lim = new unsigned char[image_dim]; 
    for (int i = 0; i < line_hei; i++) {
        memcpy(lim + i * line_wid, im + (i + line->top) * wid + line->left,\
                sizeof(unsigned char) * line_wid);
    }
    
    find_line_image_rect(lim, line_wid, line_hei, line, rects, rectNum);
    noise_cc_removal(rects, rectNum, line_hei, dirFlag);	

    // only consider the horizontal rows
    if (dirFlag) {
        merge_rect_one_row(rects, rectNum);
    }
    merge_small_rect(rects, rectNum, line_hei);
    
    if (rectNum>0) {
        SSeqLESegRect *temp_rects = new SSeqLESegRect[rectNum];
        memcpy(temp_rects, rects, sizeof(SSeqLESegRect)*rectNum);
        int good_num = 0;
        for (int i = 0; i < rectNum; i++) {
            if (temp_rects[i].flag == false) {
                continue ;
            }
            rects[good_num].left   = MAX_SEQLESEG(0, temp_rects[i].left);
            rects[good_num].right  = MIN_SEQLESEG(line_wid - 1, temp_rects[i].right);
            rects[good_num].top    = MAX_SEQLESEG(0, temp_rects[i].top);
            rects[good_num].bottom = MIN_SEQLESEG(line_hei - 1, temp_rects[i].bottom);
            good_num++;
        }
        rectNum = good_num;
       
        SeqLESegSortRect(rects, rectNum);   
        if (temp_rects != NULL) {
            delete []temp_rects;
            temp_rects = NULL;
        }
    }
    
 //--------------------------------------   
    if (lim != NULL) {
        delete []lim;
        lim = NULL;
    }
}


// only consider the horizontal rows
// merge the rectangle in one row
void merge_rect_one_row(SSeqLESegRect *&rects, int &rectNum) {
    int i = 0;
    int j = 0;
	
    for (i = 0; i < rectNum; i++) {
        SSeqLESegRect *rt1 = rects + i;
        if (!rt1->flag) {
            continue;
        }
		
        int rw1 = rt1->right - rt1->left + 1;
        int rh1 = rt1->bottom - rt1->top + 1;
        bool bupdate_flag = false;

        // recursively check the relationship between this rectangle and other rectangles
        while (1) {
            bupdate_flag = false;

            for (j = 0; j < rectNum; j++) {
                SSeqLESegRect *rt2 = rects + j;
                if (!rt2->flag || j == i) {
                    continue;
                }

                // check whether two rectangles are horizontally overlaped
                int rw2 = rt2->right - rt2->left + 1;
                int rh2 = rt2->bottom - rt2->top + 1;
                int imin_x = MIN_SEQLESEG(rt1->left, rt2->left);
                int imax_x = MAX_SEQLESEG(rt1->right, rt2->right);
                int ix_len = imax_x - imin_x + 1;
                float foverlap_w = rw1+rw2-ix_len;
                
                //bool boverlap_flag = (foverlap_w >= -1)?true:false;
                int merge_thresh = (-1) *0.15 * (rh1 + rh2) / 2.0;
                bool boverlap_flag = (foverlap_w >= merge_thresh)?true:false;

                if (boverlap_flag) {
                    rt1->left   = MIN_SEQLESEG(rt1->left, rt2->left);
                    rt1->right  = MAX_SEQLESEG(rt1->right, rt2->right);
                    rt1->top    = MIN_SEQLESEG(rt1->top, rt2->top);
                    rt1->bottom = MAX_SEQLESEG(rt1->bottom, rt2->bottom);
                    rw1 = rt1->right - rt1->left + 1;
                    rt2->flag   = false;
                    bupdate_flag = true;
                }
            }
            if (!bupdate_flag) {
                break;
            }
        }
    }

    return ;
}

// merge those CCs which are too small
void merge_small_rect(SSeqLESegRect *&rects, int &rectNum, int iLineH)  {

    const double dMinAspectRatio = 0.8;
    const double dMinRect2LineHRatio = 0.3;

    for (int i = 0; i < rectNum; i++) {
        SSeqLESegRect *rt1 = rects + i;
        if (!rt1->flag) {
            continue;
        }

        int rw1 = rt1->right - rt1->left + 1;
        int rh1 = rt1->bottom - rt1->top + 1;
        double daspect_ratio = (double)rw1 / rh1;
        double drect_2_line_h_ratio = (double)rh1 / iLineH;
        if (daspect_ratio >= dMinAspectRatio && drect_2_line_h_ratio >= dMinRect2LineHRatio) {
            continue;
        }
        int imax_dist = 0;
        int imax_idx = -1;
        int idist = 0;

        for (int j = 0; j < rectNum; j++) {
            SSeqLESegRect *rt2 = rects + j;
            if (!rt2->flag || i == j) {
                continue;
            }
            if (rt1->left > rt2->left) {
                idist = abs(rt1->left - rt2->right);
            }
            else {
                idist = abs(rt2->left - rt1->right);
            }
            if (imax_dist == 0) {
                imax_dist = idist;
                imax_idx = j;
            }
            else if (imax_dist > idist) {
                imax_dist = idist;
                imax_idx = j;
            }
        }
        if (imax_idx != -1) {
            SSeqLESegRect *rt2 = rects + imax_idx;
            rt1->left   = MIN_SEQLESEG(rt1->left, rt2->left);
            rt1->right  = MAX_SEQLESEG(rt1->right, rt2->right);
            rt1->top    = MIN_SEQLESEG(rt1->top, rt2->top);
            rt1->bottom = MAX_SEQLESEG(rt1->bottom, rt2->bottom);
            rt2->flag = false;
        }

    }
}

// remove those CCs which are noise CC
void noise_cc_removal(SSeqLESegRect *&rects, int &rectNum,\
        int imgHeight, bool dirFlag) {

    for (int i = 0; i < rectNum; i++) {
	
        if (!rects[i].flag) {
            continue;
        }

        int cy = (rects[i].top + rects[i].bottom) / 2;
        int rw = rects[i].right - rects[i].left + 1;
        int rh = rects[i].bottom - rects[i].top + 1;
        int area = rw*rh;

        if (dirFlag) { // horizontal row
			
            if (cy <= imgHeight / 6 || cy >= imgHeight * 5 / 6) {
                rects[i].flag = false;
            }
            else if (MAX(rw, rh) * 7 < imgHeight) {
                rects[i].flag = false;
            } 
            else if (rw < 5 && rh < 5) {
                rects[i].flag = false;
            } 
        }
        if (!rects[i].flag) {
            continue;
        }

        // merge the CCs which are included by other CCs
        for (int j = 0; j < rectNum; j++) {
            if (!rects[j].flag || j == i) {
                continue;
            }
            int rw1 = rects[j].right - rects[j].left + 1;
            int rh1 = rects[j].bottom - rects[j].top + 1;
            int area1 = rw1 * rh1;
            if (area > area1) {
                if (rects[i].left <= rects[j].left && rects[i].right >= rects[j].right && \
                        rects[i].top <= rects[j].top && rects[i].bottom >= rects[j].bottom) {
                    rects[j].flag = false;
                }
            }
            else {
                if (rects[j].left <= rects[i].left && rects[j].right >= rects[i].right && \
                        rects[j].top <= rects[i].top && rects[j].bottom >= rects[i].bottom) {
                    rects[i].flag = false;
                }
            }
        }

    }

}

// get the CCs of one row image
void find_line_image_rect(unsigned char *lim, int lineWid, int lineHei, SeqLESegDecodeRect *line,\
        SSeqLESegRect *&rects, int &rectNum)  {
    const int image_dim = lineWid * lineHei;
    unsigned char *bim = new unsigned char[image_dim];
    unsigned char * bim2    = new unsigned char[lineWid * lineHei];

    rectNum = 0;
    SeqLESegLocalBin2(lim, lineWid, lineHei, bim, bim2, line->isBlackWdFlag);

    int *label = NULL;
    rects = SeqLESegRegionExtract(bim, lineWid, lineHei, label, rectNum);

    int rect_num2 = 0;
    SSeqLESegRect *rects2 = NULL; 
    int *label2 = NULL;
    rects2 = SeqLESegRegionExtract(bim2, lineWid, lineHei, label2, rect_num2);

    int valid1 = 0;
    for (int i = 0; i < rectNum; i++) {
        if (rects[i].bottom - rects[i].top < lineHei / 3) {
            continue;
        }
        int j = 0;
        for (j = 0; j < rect_num2; j++) {
            if (rects2[j].top <= 2 || rects2[j].bottom >= lineHei - 3) {
                continue;
            }

            if (rects[i].top >= rects2[j].top && rects[i].bottom <= rects2[j].bottom && 
                rects[i].left >= rects2[j].left && rects[i].right <= rects2[j].right) {
                break;
            }
        }
        if (j == rect_num2 && rects[i].top >= 2 && rects[i].bottom <= lineHei - 3) {
            valid1++;
        }
    }

    int valid2 = 0;
    for (int i = 0; i < rect_num2; i++) {
        if (rects2[i].bottom - rects2[i].top < lineHei / 3) {
            continue;
        }
        int j = 0;
        for (j = 0; j < rectNum; j++) {
            if (rects[j].top <= 2 || rects[j].bottom >= lineHei - 3) {
                continue;       
            }
            if (rects2[i].top >= rects[j].top && rects2[i].bottom <= rects[j].bottom && 
                rects2[i].left >= rects[j].left && rects2[i].right <= rects[j].right) {
                break;
            }
        }
        if (j == rectNum && rects2[i].top >= 2 && rects2[i].bottom <= lineHei-3) {
            valid2++;
        }
    }

    if (valid2 > 2 * valid1) {
        int rectnum_temp    = rect_num2;
        rect_num2            = rectNum;
        rectNum             = rectnum_temp;
        SSeqLESegRect *rect_temp = rects2;
        rects2              = rects;
        rects               = rect_temp;
    }

    if (rects2 != NULL) {
        delete []rects2;
        rects2 = NULL;
    }
    if (bim2 != NULL) {
        delete []bim2;
        bim2 = NULL;
    }
    if (label2!= NULL) {
        delete [] label2;
        label2 = NULL;
    }
    if (bim!=NULL) {
        delete []bim;
        bim = NULL;
    }
    if (label!=NULL) {
        delete []label;
        label = NULL;
    }

    return ;    
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
