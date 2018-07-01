/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file SeqLEngWordSegProcessor.cpp
 * @author hanjunyu(com@baidu.com)
 * @date 2014/06/20 13:58:22
 * @brief 
 *  
 **/

#include "SeqLEngWordSegProcessor.h"
#include <iostream>
#include "SeqLDecodeDef.h"
#include "mul_row_segment.h"
#include "dll_main.h"

extern char g_pimage_title[256];
extern char g_psave_dir[256];
extern int g_iline_index;

int CSeqLEngWordSegProcessor :: loadFileWordSegRects( std::string fileName ){
  FILE *fd = fopen( fileName.c_str(), "r");
  if( fd == NULL){
    return -1;
  }

  fileName_ = fileName;
  inSegLineRectVec_.clear();
  wordLineRectVec_.clear();
 
  char maxStr[8192];
  while( fgets(maxStr, 8190, fd)) {
    std::cout << maxStr << std::endl;
    char * readStr = maxStr;
    SSeqLEngLineRect tempLineRect;
    while(true){
        int left, top, wid, hei, readNum;
        int num = sscanf( readStr, "%d\t%d\t%d\t%d\t%n", &left,\
                &top, &wid, &hei, &readNum);
        std::cout << num << std::endl;
        if( num < 1){
            break;
        }
        readStr += readNum - 1;
        SSeqLEngRect tempWin;
        tempWin.left_ = left; 
        tempWin.top_  = top;
        tempWin.wid_  = wid;
        tempWin.hei_  = hei;
        tempLineRect.rectVec_.push_back(tempWin);
    }
    inSegLineRectVec_.push_back(tempLineRect);
  }
  
  fclose( fd );
  return 0;
}

int CSeqLEngWordSegProcessor::set_file_detect_line_rects(\
        const std::vector<SSeqLESegDecodeSScanWindow> &lineDectVec) {
    
    fileName_ = "null";
    inLineDectVec_.clear();
    inSegLineRectVec_.clear();
    wordLineRectVec_.clear();
    inLineDectVec_ = lineDectVec;

    return 0;
}

// interface for different modules
// it needs to set the suitable parameter values
// otherwise, the default parameter is used
void CSeqLEngWordSegProcessor::set_exd_parameter(bool extend_flag) {
    
    _m_extend_flag = extend_flag;
    return ;
}

void CSeqLEngWordSegProcessor::set_row_seg_parameter(bool row_segment_flag) {
    _m_row_segment_flag = row_segment_flag;
    return ;
}

int CSeqLEngWordSegProcessor::loadFileDetectLineRects( std::string fileName ){
    fileName_ = fileName;
    inLineDectVec_.clear();
    inSegLineRectVec_.clear();
    wordLineRectVec_.clear();
	FILE *fd = fopen(fileName.c_str(),"r");
	if(fd==NULL) 
	{	
	    return -1; 
	}

	int readLeft,readRight,readTop,readBottom,readIsBlack,blockNum;
	while(1)
	{

		int temp = fscanf(fd,"%d\t%d\t%d\t%d\t%d\t%d\t", &readLeft,&readRight,&readTop,&readBottom,&readIsBlack
			,&blockNum);
        //std::cout << "read temp " << temp << std::endl;
		if(temp<6) break;
        
        // check the validity of the information
        if (readLeft<0 || readRight<0 || readTop<0 || readBottom<0) {
            continue;
        }

		SSeqLESegDecodeSScanWindow tempLine;
		tempLine.left = readLeft;
		tempLine.right= readRight;
		tempLine.top  = readTop;
		tempLine.bottom=readBottom;
		tempLine.detectConf = blockNum;
		tempLine.isBlackWdFlag = readIsBlack;
        
		fscanf( fd, "\n" );
		inLineDectVec_.push_back(tempLine);
	}
	fclose(fd);
    
    /*for (int i = 0; i < inLineDectVec_.size(); i++) {
        std::cout << "Line " << i << "\tL" << inLineDectVec_[i].left\
            << "\tR" << inLineDectVec_[i].right \
            << "\tT" << inLineDectVec_[i].top << "\tN"\
            << inLineDectVec_[i].bottom \
            << "\t" << inLineDectVec_[i].detectConf << "\t"\
            << inLineDectVec_[i].isBlackWdFlag << std::endl;
    }*/

	return 0; 
}

int CSeqLEngWordSegProcessor::gen_dect_line_rect(int iWidth, int iHeight){
   
    SSeqLESegDecodeSScanWindow temp_line;
    temp_line.left = 0;
    temp_line.right = iWidth-1;
    temp_line.top  = 0;
    temp_line.bottom = iHeight-1;
    temp_line.detectConf = 0;
    temp_line.isBlackWdFlag = 0;
    inLineDectVec_.push_back(temp_line);
	
    return 0; 
}

void CSeqLEngWordSegProcessor::processOneImage(const IplImage * pOrgColorImg,\
        std::vector<SSeqLESegDecodeSScanWindow> & indectVec) {
    inSegLineRectVec_.clear();
    if(pOrgColorImg->width<SEQL_MIN_IMAGE_WIDTH || pOrgColorImg->height<SEQL_MIN_IMAGE_HEIGHT \
            || pOrgColorImg->width>SEQL_MAX_IMAGE_WIDTH || pOrgColorImg->height>SEQL_MAX_IMAGE_HEIGHT){
        return;
    }
    
    IplImage * pColorImg = cvCreateImage( cvSize(pOrgColorImg->width, pOrgColorImg->height), \
            pOrgColorImg->depth, pOrgColorImg->nChannels );
    cvCopy(pOrgColorImg, pColorImg, NULL);
	
    SeqLESegWordLineAdjust(indectVec,pColorImg);
	SeqLESegWordLineMerge( indectVec );

    IplImage *grayImg  = cvCreateImage( cvSize(pColorImg->width, pColorImg->height), 8, 1 );
    int nChannels = pColorImg->nChannels;
    if(nChannels!=1){
        cvCvtColor( pColorImg, grayImg, CV_BGR2GRAY );
    }
    else{
        cvCopy(pColorImg, grayImg, NULL);
    }
    unsigned char *gim = NULL;
    int gw = grayImg->width;
    int gh = grayImg->height;
    gim = new unsigned char[gw*gh];
    for( int i=0; i<grayImg->height; i++ ){
        for( int j=0; j<grayImg->width; j++ ){
            gim[i*gw+j] = grayImg->imageData[i*grayImg->widthStep+j];
        }
    }

    inSegLineRectVec_.resize(indectVec.size());
    segInforVec_.resize(indectVec.size());
    for( int linei =0; linei<indectVec.size(); linei++){
        SeqLESegDecodeRect tempLine;
        tempLine.left   = indectVec[linei].left;
        tempLine.right  = indectVec[linei].right;
        tempLine.top    = indectVec[linei].top;
        tempLine.bottom = indectVec[linei].bottom;
        tempLine.isBlackWdFlag = indectVec[linei].isBlackWdFlag;
        
        /*std::cout << "Input tempLine L" << tempLine.left << " R" << tempLine.right << \
            " T" << tempLine.top << " B" << tempLine.bottom << "tempLine.isBlackWdFlag" \
            <<  tempLine.isBlackWdFlag << std::endl; */ // for debug
        
	    int lineWidth         = tempLine.right - tempLine.left+1;
	    int lineHeight        = tempLine.bottom - tempLine.top+1;
	    int initMinlineWidth  = lineWidth < lineHeight ? lineWidth : lineHeight ;
        bool isHorFlag = true;
        
	    if( lineHeight > lineWidth ){
            isHorFlag = false;
            std::cerr<< "Is vertical Line, forbid english segment" << std::endl;
            continue;
        }else if(lineHeight<SEQL_MIN_IMAGE_HEIGHT || lineWidth<SEQL_MIN_IMAGE_WIDTH){
            isHorFlag = true;
            std::cerr<< "Is too thin Line, forbid english segment" << std::endl;
            continue;
        }
        else{        
            isHorFlag = true;
        }

        SSeqLESegRect *rects = NULL;
        int rectNum  = 0;
        seqle_seg_char_segment(gim, gw, gh, &tempLine, _m_extend_flag,
            isHorFlag, rects, rectNum, segInforVec_[linei]);
        
      if(0)
      {
	    printf( "line: %d %d\n", tempLine.left, tempLine.top );
        char g_szWordSegName[256];
		strcpy( g_szWordSegName, "./wordSegDir/" );
        strcat( g_szWordSegName, fileName_.c_str() );
	    if( access( g_szWordSegName, 0 ) != 0 )
    	{	    
        	printf( "not exist!%s, rectNum %d \n", g_szWordSegName, rectNum);
        	FILE *fp = fopen( g_szWordSegName, "wt" ); 
        	for( int i=0; i<rectNum; i++ )
        	{    
            	fprintf( fp, "%d\t%d\t%d\t%d\t%f\t", rects[i].left+tempLine.left, rects[i].top+tempLine.top, \
                        (rects[i].right-rects[i].left+1), (rects[i].bottom-rects[i].top+1), rects[i].gapConf );
        	}    
        	fclose( fp );
    	}    
    	else 
    	{    
        	/*printf( "exist! %s, rectNum %d\n", g_szWordSegName, rectNum);*/ //for debug
        	FILE *fp = fopen( g_szWordSegName, "a" );     
        	for( int i=0; i<rectNum; i++ )
        	{    
           		if( i==0 ) 
               			fprintf( fp, "\n%d\t%d\t%d\t%d\t%f\t", rects[i].left+tempLine.left, rects[i].top+tempLine.top,\
                                (rects[i].right-rects[i].left+1), (rects[i].bottom-rects[i].top+1), rects[i].gapConf );
           		else 
               			fprintf( fp, "%d\t%d\t%d\t%d\t%f\t", rects[i].left+tempLine.left, rects[i].top+tempLine.top,\
                                (rects[i].right-rects[i].left+1), (rects[i].bottom-rects[i].top+1), rects[i].gapConf );
        	}    
        	fclose( fp );
    	} 
      }
        
      SSeqLEngLineRect &tempSegLine = inSegLineRectVec_[linei];
      tempSegLine.isBlackWdFlag_ = tempLine.isBlackWdFlag;
      //std::cout << "  tempSegLine.isBlackWdFlag_" <<   tempSegLine.isBlackWdFlag_ <<std::endl;
      
      tempSegLine.left_ = tempLine.left;
      tempSegLine.top_ = tempLine.top;
      tempSegLine.wid_ = tempLine.right - tempLine.left + 1;
      tempSegLine.hei_ = tempLine.bottom - tempLine.top + 1;

      tempSegLine.rectVec_.resize(rectNum);
      for( int i=0; i<rectNum; i++ )
      {
        tempSegLine.rectVec_[i].left_ = rects[i].left+tempLine.left;
        tempSegLine.rectVec_[i].wid_ = (rects[i].right-rects[i].left+1);
        
        tempSegLine.rectVec_[i].top_ = rects[i].top+tempLine.top;
        tempSegLine.rectVec_[i].hei_ = (rects[i].bottom-rects[i].top+1);
        
        /*tempSegLine.rectVec_[i].top_ = tempLine.top;
        tempSegLine.rectVec_[i].hei_ = tempLine.bottom - tempLine.top + 1;*/
      }
      if(false && (tempLine.bottom -tempLine.top)>  0.3* pOrgColorImg->height \
              && ( tempLine.bottom - tempLine.top)*7 > (tempLine.right - tempLine.left) ){
        tempSegLine.rectVec_.resize(rectNum+1);
        tempSegLine.rectVec_[rectNum].left_ = tempLine.left;
        tempSegLine.rectVec_[rectNum].wid_ = (tempLine.right-tempLine.left+1);
        tempSegLine.rectVec_[rectNum].top_ = tempLine.top;
        tempSegLine.rectVec_[rectNum].hei_ = tempLine.bottom - tempLine.top + 1;        
      }
     
     delete [] rects;

    }
    cvReleaseImage( & pColorImg);
    cvReleaseImage( & grayImg);
    delete [] gim;
}

void CSeqLEngWordSegProcessor::process_image_eng_ocr(const IplImage * image,\
        std::vector<SSeqLESegDecodeSScanWindow> & indectVec) {

    inSegLineRectVec_.clear();
    if (image->width < SEQL_MIN_IMAGE_WIDTH || image->height < SEQL_MIN_IMAGE_HEIGHT \
            || image->width > SEQL_MAX_IMAGE_WIDTH || image->height > SEQL_MAX_IMAGE_HEIGHT) {
        return;
    }
    
    IplImage * pcolor_img = cvCreateImage(cvSize(image->width, image->height), \
            image->depth, image->nChannels);
    cvCopy(image, pcolor_img, NULL);
    
    IplImage *gray_img  = cvCreateImage(cvSize(pcolor_img->width, pcolor_img->height), 8, 1);
    int channels = pcolor_img->nChannels;
    if (channels != 1) {
        cvCvtColor(pcolor_img, gray_img, CV_BGR2GRAY);
    } else {
        cvCopy(pcolor_img, gray_img, NULL);
    }
    unsigned char *gim = NULL;
    int gw = gray_img->width;
    int gh = gray_img->height;
    gim = new unsigned char[gw*gh];
    for (int i = 0; i < gray_img->height; i++) {
        memcpy(gim + i * gw,\
                gray_img->imageData + i * gray_img->widthStep,\
                sizeof(unsigned char) * gw);
    }

    inSegLineRectVec_.resize(indectVec.size());
    segInforVec_.resize(indectVec.size());
    for (int linei = 0; linei < indectVec.size(); linei++) {
        SeqLESegDecodeRect temp_line;
        temp_line.left   = indectVec[linei].left;
        temp_line.right  = indectVec[linei].right;
        temp_line.top    = indectVec[linei].top;
        temp_line.bottom = indectVec[linei].bottom;
        temp_line.isBlackWdFlag = indectVec[linei].isBlackWdFlag;
        
        int line_width         = temp_line.right - temp_line.left + 1;
        int line_height        = temp_line.bottom - temp_line.top + 1;
        bool hor_flag = true;
        
        if (line_height > line_width) {
            hor_flag = false;
            std::cerr << "Is vertical Line, forbid english recognize" << std::endl;
            continue;
        } else if (line_height < SEQL_MIN_IMAGE_HEIGHT || line_width < SEQL_MIN_IMAGE_WIDTH) {
            hor_flag = true;
            std::cerr << "Is too thin Line, forbid english recognize" << std::endl;
            continue;
        } else{        
            hor_flag = true;
        }

        SSeqLESegRect *rects = NULL;
        int rect_num  = 0;
        seqle_seg_char_segment(gim, gw, gh, &temp_line, _m_extend_flag, hor_flag,\
                rects, rect_num, segInforVec_[linei]);
        
        SSeqLEngLineRect &temp_seg_line = inSegLineRectVec_[linei];
        temp_seg_line.isBlackWdFlag_ = temp_line.isBlackWdFlag;
      
        temp_seg_line.left_ = temp_line.left;
        temp_seg_line.top_ = temp_line.top;
        temp_seg_line.wid_ = temp_line.right - temp_line.left + 1;
        temp_seg_line.hei_ = temp_line.bottom - temp_line.top + 1;

        temp_seg_line.rectVec_.resize(rect_num);
        for (int i = 0; i < rect_num; i++) {
            temp_seg_line.rectVec_[i].left_ = rects[i].left + temp_line.left;
            temp_seg_line.rectVec_[i].wid_ = (rects[i].right - rects[i].left + 1);
        
            temp_seg_line.rectVec_[i].top_ = rects[i].top + temp_line.top;
            temp_seg_line.rectVec_[i].hei_ = (rects[i].bottom - rects[i].top + 1);
        
        }
     
        delete []rects;
    }
    cvReleaseImage(& pcolor_img);
    cvReleaseImage(& gray_img);
    delete [] gim;
}

void CSeqLEngWordSegProcessor :: EstimateSpaceTh(SSeqLEngSegmentInfor & segInfor, \
        SSeqLEngSpaceConf & spaceConf  )
{
    spaceConf.highSpaceTh1_ = std::min(0.65*segInfor.coarseAvgBlockH, 0.95*segInfor.coarseAvgBlockW);
    spaceConf.highSpaceTh2_ = segInfor.coarseAvgBlockSpace*1.9;
    spaceConf.highSpaceTh_ = std::min(spaceConf.highSpaceTh1_, spaceConf.highSpaceTh2_);
    //spaceConf.lowSpaceTh1_ = segInfor.coarseAvgBlockSpace*1.2;
    //spaceConf.lowSpaceTh2_ = std::max(segInfor.coarseAvgBlockH*0.46,4.0);
    //spaceConf.lowSpaceTh_ = std::max(spaceConf.lowSpaceTh1_, spaceConf.lowSpaceTh2_);
    //spaceConf.geoWordSpaceTh_ = std::max(spaceConf.highSpaceTh_, spaceConf.lowSpaceTh_);
    spaceConf.geoWordSpaceTh_ = spaceConf.highSpaceTh_ ;
    spaceConf.meanBoxWidth_ = segInfor.coarseAvgBlockW;
    spaceConf.meanBoxSpace_ = segInfor.coarseAvgBlockSpace;
    //std::cout << "geo word space th " << spaceConf.geoWordSpaceTh_ << std::endl;
    //std::cout << "mean box width " << spaceConf.meanBoxWidth_ << std::endl;
    //std::cout << "mean box height " << segInfor.coarseAvgBlockH << std::endl;
    //std::cout << "mean box space " << spaceConf.meanBoxSpace_ << std::endl;
}


bool CSeqLEngWordSegProcessor :: isRealWordCutPoint(SSeqLEngRect & a, SSeqLEngRect & b, SSeqLEngSpaceConf & spaceConf){
    if( (b.left_ -(a.left_+a.wid_)) > 2.0*spaceConf.meanBoxSpace_ /* 1.5*spaceConf.geoWordSpaceTh_*/
    //if( (b.left_ -(a.left_+a.wid_)) >  1.5*spaceConf.geoWordSpaceTh_
    /*        && (b.left_ -(a.left_+a.wid_)) > 1.5*spaceConf.meanBoxWidth_ */
            /*&& b.wid_>3*spaceConf.meanBoxWidth_ && a.wid_>3*spaceConf.meanBoxWidth_*/ ){
      return true;
    }
    else
    {
      return false;
    }
}

bool leftSeqLRectFirst( const SSeqLEngRect & r1, const SSeqLEngRect & r2 ){
   return r1.left_ < r2.left_;
}

void CSeqLEngWordSegProcessor :: wordSegProcessGroupNearby( int groupWidth ){
    std::vector<SSeqLEngLineRect> orgSegLineVec = inSegLineRectVec_; 
    // sort
    for( unsigned int i=0; i<orgSegLineVec.size(); i++){
       sort( orgSegLineVec[i].rectVec_.begin(), orgSegLineVec[i].rectVec_.end(), leftSeqLRectFirst );
    }
    // grounp the word win
    
    wordLineRectVec_.resize(orgSegLineVec.size());
    spaceConfVec_.resize( orgSegLineVec.size());

    //std::cout << "org seg line vec size: " << orgSegLineVec.size() << std::endl;
    for( unsigned int i=0; i<orgSegLineVec.size(); i++){
        EstimateSpaceTh(segInforVec_[i], spaceConfVec_[i]);
        SSeqLEngLineRect & tempOrgLine = orgSegLineVec[i];
        SSeqLEngLineRect & groupTempLine = wordLineRectVec_[i];
        groupTempLine.isBlackWdFlag_ = tempOrgLine.isBlackWdFlag_;
        groupTempLine.left_ = tempOrgLine.left_;
        groupTempLine.top_ = tempOrgLine.top_;
        groupTempLine.wid_ = tempOrgLine.wid_;
        groupTempLine.hei_ = tempOrgLine.hei_;
        
        /*std::cout <<"tempOrgLine rect " << i <<" L "<< tempOrgLine.left_ << " T" << tempOrgLine.top_ \
            << " W" << tempOrgLine.wid_ << " H" << tempOrgLine.hei_ << std::endl;*/ // for debug

        groupTempLine.rectVec_.clear();
        


        float boxSpaceSum = 0;
        //std::cout << "temp org seg line vec size: "<< tempOrgLine.rectVec_.size() << std::endl;
        for( unsigned int j=1; j<tempOrgLine.rectVec_.size(); j++){
            boxSpaceSum += (tempOrgLine.rectVec_[j].left_ - (tempOrgLine.rectVec_[j-1].left_+\
                        tempOrgLine.rectVec_[j-1].wid_-1));
        }
        float minBoxSpace = 0.5*boxSpaceSum/(tempOrgLine.rectVec_.size()-1+0.1);

        //std::cout << "temp org seg line vec size: "<< tempOrgLine.rectVec_.size() << std::endl;
        for( unsigned int j=0; j<tempOrgLine.rectVec_.size(); j++){
          SSeqLEngRect & rectA = tempOrgLine.rectVec_[j];
          int tempTop = rectA.top_;
          int tempLeft = rectA.left_;
          int tempRight  = rectA.left_+rectA.wid_;
          int tempBottom = rectA.top_+rectA.hei_;

          //std::cout << "ractA l" << tempLeft <<" t"<< tempTop << " r" << tempRight 
          //  << " b" << tempBottom << std::endl; // for debug
          //int extGroupSize = 2;
          //if(tempOrgLine.rectVec_.size()<10){ extGroupSize = 2;}
          int extGroupSize = groupWidth;
          for(unsigned int k=0; k<groupWidth+extGroupSize; k++){
             if( (j+k) >= tempOrgLine.rectVec_.size()) {
               break;
             }

             if(k>=groupWidth){
              if(isRealWordCutPoint(tempOrgLine.rectVec_[j+k-1], tempOrgLine.rectVec_[j+k], \
                          spaceConfVec_[i]) ){
                break;
              }
             }
             
             if(false && (tempOrgLine.rectVec_.size()>10) && k>0 && k<(groupWidth+extGroupSize-2)){
                if(tempOrgLine.rectVec_[j+k].left_+tempOrgLine.rectVec_[j+k].wid_ >
                        tempOrgLine.rectVec_[j+k+1].left_-minBoxSpace){
                    break;
                }
             }

             SSeqLEngRect & rectB = tempOrgLine.rectVec_[j+k];
             tempTop = std::min(tempTop, rectB.top_);
             tempLeft = std::min(tempLeft, rectB.left_);
             tempRight = std::max(tempRight, rectB.left_+rectB.wid_);
             tempBottom = std::max(tempBottom, rectB.top_+rectB.hei_);
             SSeqLEngRect tempRect;
             tempRect.left_ = tempLeft;
             tempRect.top_  = tempTop;
             tempRect.wid_  = tempRight - tempRect.left_;
             tempRect.hei_  = tempBottom - tempRect.top_;
             tempRect.compNum_ = k+1;
             tempRect.score_ = 0;
             //if(tempRect.wid_ > tempRect.hei_*12){
             //    continue;
             //    break;
             //}
             
             if(j==0){
                tempRect.leftSpace_ = 3*spaceConfVec_[i].meanBoxSpace_;
             }
             else
             {
                tempRect.leftSpace_ = tempOrgLine.rectVec_[j].left_ - \
                                      (tempOrgLine.rectVec_[j-1].left_+tempOrgLine.rectVec_[j-1].wid_);
             }

             if((j+k)==(tempOrgLine.rectVec_.size()-1)){
               tempRect.rightSpace_ = 3*spaceConfVec_[i].meanBoxSpace_;
             }
             else
             {
                tempRect.rightSpace_ = tempOrgLine.rectVec_[j+k+1].left_ - \
                                      (tempOrgLine.rectVec_[j+k].left_+tempOrgLine.rectVec_[j+k].wid_);
             }
            
             if(tempRect.leftSpace_>1.2*(spaceConfVec_[i].meanBoxSpace_+1) && \
                     tempRect.rightSpace_>1.2*(spaceConfVec_[i].meanBoxSpace_+1) ){
                tempRect.sigWordFlag_ = true;
             }
             else
             {
                tempRect.sigWordFlag_ = false;
             }
             
             for(unsigned int l=1; l<k; l++){
               tempRect.score_ += (spaceConfVec_[i].meanBoxSpace_ - (tempOrgLine.rectVec_[j+l].left_ - \
                                  (tempOrgLine.rectVec_[j+l-1].left_ + \
                                   tempOrgLine.rectVec_[j+l-1].wid_)) )/spaceConfVec_[i].meanBoxSpace_ ;
             }
             if(k==0 || (tempRect.wid_< 0.8*tempRect.hei_) ){
                //tempRect.bePredictFlag_ = true;
                tempRect.bePredictFlag_ = false;
             }
             else
             {
                //tempRect.bePredictFlag_ = true;
                tempRect.bePredictFlag_ = false;
             }
             //if( /*tempRect.wid_ > 0.3*tempRect.hei_*/ /*&& tempRect.wid_ < 10*tempRect.hei_*/ )
                groupTempLine.rectVec_.push_back( tempRect );   
        
          }
            if(groupTempLine.rectVec_.size()>0){
                groupTempLine.rectVec_[groupTempLine.rectVec_.size()-1].bePredictFlag_ = true;
            }
        }
        
        if( false && groupTempLine.rectVec_.size()>1){
                
              int orgRectSize = groupTempLine.rectVec_.size();
              int minTop = groupTempLine.rectVec_[0].top_;
              int maxBottom =  groupTempLine.rectVec_[0].top_ + groupTempLine.rectVec_[0].hei_;
              for(int l=1; l<orgRectSize; l++){
                minTop = std::min(minTop, groupTempLine.rectVec_[l].top_);
                maxBottom = std::max(maxBottom, groupTempLine.rectVec_[l].top_ + groupTempLine.rectVec_[l].hei_);
              }
              if(0){
                groupTempLine.rectVec_.resize(2*orgRectSize);
                //std::cout<< minTop << " " << maxBottom << std::endl; 
                for(int l=0; l<orgRectSize; l++){
                  groupTempLine.rectVec_[l+orgRectSize] = groupTempLine.rectVec_[l];
                  groupTempLine.rectVec_[l+orgRectSize].top_ = minTop;
                  groupTempLine.rectVec_[l+orgRectSize].hei_ = maxBottom - minTop;
                }
              }
              else
              {
                for(int l=0; l<orgRectSize; l++){
                  groupTempLine.rectVec_[l].top_ = minTop;
                  groupTempLine.rectVec_[l].hei_ = maxBottom - minTop;
                }
              }
        }
    }
}

int CSeqLEngWordSegProcessor::save_file_word_wins(std::string fileName) {
    FILE *fd = fopen(fileName.c_str(), "w");
    if (fd == NULL) {
        return -1;
    }
    for (unsigned int i = 0; i < wordLineRectVec_.size(); i++) {
        for (unsigned int j = 0; j < wordLineRectVec_[i].rectVec_.size(); j++) {
            fprintf(fd, "%d\t%d\t%d\t%d\t%d\t\n",\
                 i, wordLineRectVec_[i].rectVec_[j].left_, \
                 wordLineRectVec_[i].rectVec_[j].top_,\
                 wordLineRectVec_[i].rectVec_[j].wid_,\
                 wordLineRectVec_[i].rectVec_[j].hei_);  
        }
    }
    fclose(fd);
    return 0;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */

