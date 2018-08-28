/*==================================================================================================
CHANGELOG:
==========
10/30/2014: v1.1
----------
* 修改了label文件，整行数据跳过空格的问题
==================================================================================================*/
#include "fun.h"

// cyy reviewed at 2014.11.13.
#include <time.h>
#include <math.h>

//lx added
extern std::string g_img_list;

Value GenData::jsonConfig;
vector<IplImage*> GenData::foreground_background_imgs;
vector<IplImage*> GenData::fushion_imgs;

int write_single_ucs4_to_utf8(ofstream& out, wchar_t letter){
    CodeConverter cc = CodeConverter("ucs-4","utf-8");
    //len表示单个utf-8的最大长度
    if(sizeof(wchar_t)!=4){
      LOG(FATAL)<<"sizeof(wchar_t)!=4";
      return -1;
    }
    char p[5]={0};
    memcpy(p,(char*)&letter,4);
    //UTF-8用1到6个字节编码UNICODE字符
    char myout[8]={0};
    size_t rt=cc.convert(p, 4, myout, 8);
    if(rt==-1){
      LOG(WARNING)<<"convert ucs4-utf8 failed";
      return -1;
    }
    out<<myout;  
    return 0; 
}

int write_single_ucs4_to_utf8(ofstream& out, ofstream& g_out, wchar_t letter){
    CodeConverter cc = CodeConverter("ucs-4","utf-8");
    //len表示单个utf-8的最大长度
    if(sizeof(wchar_t)!=4){
      LOG(FATAL)<<"sizeof(wchar_t)!=4";
      return -1;
    }
    char p[5]={0};
    memcpy(p,(char*)&letter,4);
    //UTF-8用1到6个字节编码UNICODE字符
    char myout[8]={0};
    size_t rt=cc.convert(p, 4, myout, 8);
    if(rt==-1){
      LOG(WARNING)<<"convert ucs4-utf8 failed";
      return -1;
    }
    out<<myout;  
    g_out<<myout;
    return 0; 
}

// cyy reviewed at 2014.11.13.
void GenData::add_blur(IplImage *pImg)
{	
	IplImage * pBlurImg = cvCreateImage( cvGetSize(pImg), pImg->depth, pImg->nChannels );
 
	cvCopy(pImg, pBlurImg, NULL);
  
	int radBlur[4] = {1, 3, 5, 7};
	int radIndex = rand() % 4;
	int nRad = radBlur[radIndex];

	printf("  Blur: kernal of Gaussian is %d.\n", nRad);
	
	cvSmooth( pBlurImg, pImg, CV_GAUSSIAN, nRad, nRad );

	cvReleaseImage( &pBlurImg );
}

unsigned long GetTickCount()  
{  
    struct timespec ts;  
  
    clock_gettime(CLOCK_MONOTONIC, &ts);  
  
    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);  
}
 
IplImage* GenData::add_rotate(IplImage* src, vector<Letter>& Sentence)  
{  
	float rotate=jsonConfig["rotation"];
	int angle_max = int(20 * rotate);
	if(angle_max<=0){
		printf("  Rotate: Angle is 0.");
		return NULL;		
	}

	float angle = rotate - rand()%angle_max/10.0;
		
    IplImage* dst = NULL;  
    printf("  Rotate: Angle is %f.\n", angle);
	
	int width =  int (
        (double)(src->height * sin(abs(angle) * CV_PI / 180.0)) +  
        (double)(src->width * cos(angle * CV_PI / 180.0 )) + 1 );  
		
    int height =  int (
        (double)(src->height * cos(angle * CV_PI / 180.0)) +  
        (double)(src->width * sin(abs(angle) * CV_PI / 180.0 )) + 1);  
	
	// printf("angle is %d, new_height is %d, new_width is %d.\n", angle, height, width);
	// printf("angle is %d, old_height is %d, old_width is %d.\n", angle, src->height, src->width);
	
    int tempLength = int(sqrt((double)src->width * src->width + src->height * src->height) + 10);  
    int tempX = (tempLength + 1) / 2 - src->width / 2;  
    int tempY = (tempLength + 1) / 2 - src->height / 2;   
  	
    dst = cvCreateImage(cvSize(width, height), src->depth, src->nChannels);  
    cvZero(dst);  
	cvNot(dst, dst);
    IplImage* temp = cvCreateImage(cvSize(tempLength, tempLength), src->depth, src->nChannels);  
    cvZero(temp);  
	cvNot(temp,temp);
   	
    cvSetImageROI(temp, cvRect(tempX, tempY, src->width, src->height));  
    cvCopy(src, temp, NULL);  
    cvResetImageROI(temp);  
    
    float m[6];  
	 
    m[0] = (float) cos(angle * CV_PI / 180.);  
    m[1] = (float) sin(angle * CV_PI / 180.);  
    m[3] = -m[1];  
    m[4] = m[0];  
    // 将旋转中心移至图像中间  
    m[2] = tempLength * 0.5f;  
    m[5] = tempLength * 0.5f;  
	
    CvMat M = cvMat(2, 3, CV_32F, m);  
	
	// 矩阵变换前后，都是temp尺寸的图;
	// M = m[0] m[1] m[2]
	//     m[3] m[4] m[5]
	
    cvGetQuadrangleSubPix(temp, dst, &M);  
	
    cvReleaseImage(&temp); 
	// matrixarr = ( a11  a12 | b1 )   dst(x,y) <- src(A[x y]' + b)               
	//             ( a21  a22 | b2 )   位移要再计算一下,
	// x' = cosθ(x-a) + sinθ(y-b) + a;
	// y' = sinθ(x-a) + cosθ(y-b) + b;
	// x' = (m[4]*x - m[1]*y - m[2]*m[4] + m[1]*m[5])/(m[0]*m[4] - m[1]*m[3]);
	// y' = (m[3]*x - m[0]*y - m[2]*m[3] + m[0]*m[5])/(m[1]*m[3] - m[0]*m[4]);
	
	vector<struct RectLean> leans(Sentence.size()); // 一次旋转结果, 倾斜矩形;
	
	// printf("tempX is %d, tempY is %d.\n", tempX, tempY);
	// printf("m[0] - m[5] : %f, %f, %f, %f, %f, %f.\n",m[0], m[1], m[2], m[3], m[4], m[5]);
	
	int radius = int(tempLength * 0.5f);
	m[2] = radius * (1 - m[0] - m[1]);
	m[5] = radius * (1 - m[0] + m[1]);
	
	int gshift_x = (tempLength + 1) / 2 - dst->width / 2;
	int gshift_y = (tempLength + 1) / 2 - dst->height / 2;
	
	for (int i = 0; i < Sentence.size(); ++i)
	{
		// 在tmp大图中的位置;
		int x1 = Sentence[i].left + tempX;
		int x2 = Sentence[i].left + Sentence[i].width + tempX;
		int y1 = Sentence[i].top + tempY;
		int y2 = Sentence[i].top + Sentence[i].height + tempY;
		
		// printf("  before: left,top,width,height:%d, %d, %d, %d.\n", Sentence[i].left,Sentence[i].top,Sentence[i].width,Sentence[i].height);
		
		leans[i].top_left.px = int ((m[4]*x1 - m[1]*y1 - m[2]*m[4] + m[1]*m[5])/(m[0]*m[4] - m[1]*m[3]) - gshift_x);
		leans[i].top_left.py = int((m[3]*x1 - m[0]*y1 - m[2]*m[3] + m[0]*m[5])/(m[1]*m[3] - m[0]*m[4]) - gshift_y);
		leans[i].top_right.px = int((m[4]*x2 - m[1]*y1 - m[2]*m[4] + m[1]*m[5])/(m[0]*m[4] - m[1]*m[3]) - gshift_x);
		leans[i].top_right.py = int((m[3]*x2 - m[0]*y1 - m[2]*m[3] + m[0]*m[5])/(m[1]*m[3] - m[0]*m[4]) - gshift_y);
		
		leans[i].bot_left.px = int((m[4]*x1 - m[1]*y2 - m[2]*m[4] + m[1]*m[5])/(m[0]*m[4] - m[1]*m[3]) - gshift_x);
		leans[i].bot_left.py = int((m[3]*x1 - m[0]*y2 - m[2]*m[3] + m[0]*m[5])/(m[1]*m[3] - m[0]*m[4]) - gshift_y);
		leans[i].bot_right.px = int((m[4]*x2 - m[1]*y2 - m[2]*m[4] + m[1]*m[5])/(m[0]*m[4] - m[1]*m[3]) - gshift_x);
		leans[i].bot_right.py = int((m[3]*x2 - m[0]*y2 - m[2]*m[3] + m[0]*m[5])/(m[1]*m[3] - m[0]*m[4]) - gshift_y);
						
		Sentence[i].left = angle < 0 ? leans[i].top_left.px : leans[i].bot_left.px;	
		Sentence[i].width = angle < 0 ? leans[i].bot_right.px : leans[i].top_right.px;
		Sentence[i].width = Sentence[i].width - Sentence[i].left;
		Sentence[i].top = angle < 0 ? leans[i].top_right.py : leans[i].top_left.py;
		Sentence[i].height = angle < 0 ? leans[i].bot_left.py : leans[i].bot_right.py;
		Sentence[i].height = Sentence[i].height - Sentence[i].top;
		
		// printf("  after : left,top,width,height:%d, %d, %d, %d.\n", Sentence[i].left,Sentence[i].top,Sentence[i].width,Sentence[i].height);
	}
    return dst;  
}

// 三通道彩色图
IplImage* GenData::add_pepper_noise(IplImage* src)
{
	float pepper_noise=jsonConfig["pepper_noise"];
	int tmp = int(pepper_noise * 100);
	if(tmp<=0){
	  printf("  Pepper_nose: percentage is 0.\n");
	  return NULL;		
	}

	float ratio = (rand()%tmp)/100.0;
	printf("  Pepper_nose: percentage is %0.2f.\n", ratio);
	int height = src->height;
	int width = src->width;

	IplImage* pEmptyImg = cvCreateImage( cvSize(width, height), src->depth, src->nChannels);
	cvZero(pEmptyImg);  
	cvNot(pEmptyImg, pEmptyImg);

	//add noise to a blank img.
	int NoiseNum = (int)(height * width * ratio);
	int i = 0, j = 0;
	
	for(int i = 0; i < NoiseNum; i++)
	{
		int randW = rand() % (width);
		int randH = rand() % (height);
		
		if ( rand()%2 == 0 )
		{
			(pEmptyImg->imageData + randH * pEmptyImg->widthStep)[randW * 3] = 0;
			(pEmptyImg->imageData + randH * pEmptyImg->widthStep)[randW * 3 + 1] = 0;
			(pEmptyImg->imageData + randH * pEmptyImg->widthStep)[randW * 3 + 2] = 0;
		}
	}
	// blur the pepper points.
	IplImage* pENblurImg = cvCreateImage( cvSize(width, height), pEmptyImg->depth, pEmptyImg->nChannels);	
	cvSmooth( pEmptyImg, pENblurImg, CV_GAUSSIAN, 7, 0 );

	// blur the src img lightly.
	IplImage* pSrcBlur = cvCreateImage( cvSize(width, height), src->depth, src->nChannels);
	if(tmp > 10)
		cvSmooth( src, pSrcBlur, CV_GAUSSIAN, 3, 0 );
	else
		cvSmooth( src, pSrcBlur, CV_GAUSSIAN, 1, 0 );
	// add chars to the blank img.
	for (i = 0; i < height; i++)
	{	
		for (j = 0; j < width; j++)
		{
			if((unsigned char)*(pSrcBlur->imageData + i * pSrcBlur->widthStep + 3 * j) < 225)
			{	
				*(pENblurImg->imageData + i * pENblurImg->widthStep + j * 3) = *(pSrcBlur->imageData + i * pSrcBlur->widthStep + j * 3);
				*(pENblurImg->imageData + i * pENblurImg->widthStep + j * 3 + 1) = *(pSrcBlur->imageData + i * pSrcBlur->widthStep + j * 3 + 1);
				*(pENblurImg->imageData + i * pENblurImg->widthStep + j * 3 + 2) = *(pSrcBlur->imageData + i * pSrcBlur->widthStep + j * 3 + 2);
			}
		}
	}
	
	cvReleaseImage(&pEmptyImg);
	cvReleaseImage(&pSrcBlur);
		
	return pENblurImg;
}

IplImage* GenData::add_shift(IplImage* src, vector<Letter>& Sentence, int fontsize)// fontsize希望能从上一级得到输入;
{
	int parts = jsonConfig["shift"]; // shift_denominator;
	if(parts<=0){
	  printf("  Shift: shift is 0.");
	  return NULL;	
	}

	int c_top = fontsize/3 - rand() % (fontsize*2/3);
	int c_bot = fontsize/3 - rand() % (fontsize*2/3);
	int c_lef = fontsize - rand() % (2 * fontsize);
	int c_rig = fontsize - rand() % (2 * fontsize);

    int n = Sentence.size();
    c_lef = MAX(c_lef, 0 - Sentence[0].left - Sentence[0].width/parts);
    c_rig = MAX(c_rig, 0 - src->width + Sentence[n - 1].left + Sentence[n - 1].width*(parts - 1)/parts);
    
    c_top = MAX(c_top, 0 - Sentence[0].top - Sentence[0].height/parts);
    c_top = MAX(c_top, 0 - Sentence[n - 1].top - Sentence[n - 1].height/parts);
    
    c_bot = MAX(c_bot, 0 - src->height + Sentence[0].top + Sentence[0].height*(parts - 1)/parts);
    c_bot = MAX(c_bot, 0 - src->height + Sentence[n - 1].top + Sentence[n - 1].height*(parts - 1)/parts);  
      
	int new_width = src->width + c_lef + c_rig;
	int new_height = src->height + c_top + c_bot;
	
	if(new_width <= 5 || new_height <= 5 || c_rig + src->width < 5 || c_lef + src->width < 5 || c_top + src->height < 5 || c_bot + src->height < 5) // too small;
	{
		IplImage* dstimg = cvCreateImage(cvSize(src->width, src->height), src->depth, src->nChannels);
		cvCopy(src, dstimg);
		return dstimg;
	}

	// printf("width is %d, height is %d.\n", src->width, src->height);
	printf("  Shift: c_lef, c_rig, c_top, c_bot: %d, %d, %d, %d.\n", c_lef, c_rig, c_top, c_bot);
	// printf("The shift is l-r-t-b:%d, %d, %d, %d.\n", c_lef, c_rig, c_top, c_bot);
	
	IplImage* dstimg = cvCreateImage(cvSize(new_width, new_height), src->depth, src->nChannels);
	cvZero(dstimg);  
	cvNot(dstimg, dstimg);

	// printf("The dst rect is from (%d, %d), size(%d, %d).\n", MAX(c_lef, 0), MAX(c_top, 0), src->width + MIN(c_lef, 0) + MIN(c_rig, 0), src->height + MIN(c_top, 0) + MIN(c_bot, 0));
	// printf("src->width is %d, src->height is %d, c_lef is %d, c_top is %d, c_rig is %d, c_bot is %d.\n", src->width, src->height,\
	c_lef, c_top, c_rig, c_bot);
	cvSetImageROI(dstimg, cvRect(MAX(c_lef, 0), MAX(c_top, 0), src->width + MIN(c_lef, 0) + MIN(c_rig, 0), \
	src->height + MIN(c_top, 0) + MIN(c_bot, 0))); 
	
	cvSetImageROI(src, cvRect(MAX(0 - c_lef, 0), MAX(0 - c_top, 0), src->width + MIN(c_lef, 0) + MIN(c_rig, 0), \
	src->height + MIN(c_top, 0) + MIN(c_bot, 0)));
	
	// printf("The src rect is from (%d, %d).\n", MAX(0 - c_lef, 0), MAX(0 - c_top, 0));
	
	cvCopy(src, dstimg);
    cvResetImageROI(dstimg);
	cvResetImageROI(src);
	
	for (int i = 0; i < Sentence.size(); ++i)
	{
		int left = Sentence[i].left + c_lef;
		int top = Sentence[i].top + c_top;
		int right = left + Sentence[i].width;
		int bottom = top + Sentence[i].height;
		
		if(left > new_width - Sentence[i].width/4 || right < Sentence[i].width/4) // 剩余部分少于1/4, 该字母不再标注;
		{
			Sentence[i].left = -1; // 做标记;
			continue;
		}
		else
		{
			Sentence[i].left = MAX(left, 0);
			Sentence[i].width = MIN(right, MIN(Sentence[i].width, new_width - 1 - Sentence[i].left));
		}
		
		if(top > new_height - Sentence[i].height/4 || bottom < Sentence[i].height/4)
		{
			Sentence[i].left = -1; // 做标记;
			continue;
		}
		else
		{
			Sentence[i].top = MAX(top, 0);
			Sentence[i].height = MIN(bottom, MIN(Sentence[i].height, new_height - 1 - Sentence[i].top));
		}
		
		// printf("left, top, width, height: %d, %d, %d, %d.|| %d, %d.\n", Sentence[i].left, Sentence[i].top, Sentence[i].width, Sentence[i].height,Sentence[i].height,new_height - 1 - Sentence[i].top);
	}
	return dstimg;
}

IplImage* GenData::add_resize(IplImage* src, vector<Letter>& Sentence)
{
	float resize_max=jsonConfig["resize_rate"];
	int range = int(200*resize_max);
	if(range<=0){
		printf("  Resize is 0.");
		return NULL;		
	}

	float k = (rand()%range)/100.0;
	k = k + 1 - resize_max;
		
	int new_width = int(src->width * k);
	int new_height = int(src->height * k);
	
	// printf("width is %d, height is %d. k is %f.\n", src->width, src->height, k);
	printf("  Resize: new_width is %d, new_height is %d.\n", new_width, new_height);
	
	IplImage* dst = cvCreateImage(cvSize(new_width,new_height),src->depth, src->nChannels);
	
	cvResize(src, dst, CV_INTER_LINEAR );
	
	for (int i = 0; i < Sentence.size(); ++i)
	{
		Sentence[i].left = int(k * Sentence[i].left);
		Sentence[i].top = int (k * Sentence[i].top);
		Sentence[i].width = int (k * Sentence[i].width);
		Sentence[i].height = int (k * Sentence[i].height);
	}
	return dst;
}

IplImage* GenData::add_no_backgrand(){
	IplImage* pimg = cvCreateImage(cvSize(width,height), IPL_DEPTH_8U,3);
	cvSet(pimg, cvScalar(255,255,255)); 
 	//set image value
    for(int i=0; i<pixels.size(); i++){
	  if(pixels[i].py<0 || pixels[i].py>=height || pixels[i].px >= width || pixels[i].px<0){
		LOG(WARNING)<<"pixel out of image range";
		LOG(WARNING)<<"\tpy = " <<pixels[i].py<<endl;
		LOG(WARNING)<< "\tpx = " <<pixels[i].px<<endl;
		LOG(WARNING)<< "\twidth = " <<width<<endl;
		LOG(WARNING)<< "\theight = " <<height<<endl;
		cvReleaseImage(&pimg);
		return NULL; 
	}
	uchar* pdata=(uchar*)(pimg->imageData+pixels[i].py*pimg->widthStep+3*pixels[i].px);
	pdata[0]=pixels[i].color;
	pdata[1]=pixels[i].color;
	pdata[2]=pixels[i].color;
  } 
  return pimg;
}

/*
   add fushion failed will return pimg
*/
IplImage* GenData::add_fushion_backgrand(){
  IplImage* pimg = add_no_backgrand();
  if(fushion_imgs.empty()){
    LOG(WARNING)<<"fushion backgrand imgs are empty.";
  	return pimg;
  }

  //random get an image
  int idx=rand()%fushion_imgs.size();
  IplImage* pimg_bgimg=fushion_imgs[idx];

  //check the channel and depth
  if(pimg_bgimg->nChannels != pimg->nChannels || pimg_bgimg->depth!= pimg->depth){
	LOG(WARNING)<<"chnnels or depth is different";   
	return pimg;
  }
  
  //check the width and height
  if(pimg_bgimg->width <= width  || pimg_bgimg->height <= height){
	LOG(WARNING)<<"backgrand image's width or height is less than img"; 
	return pimg;  
  }

  //random a position to get a fushion
  //lx modified, to encoquer more space
  float ratio = (rand()%100)/100 + 1.5;
  int new_width = (int)ratio*width;
  int new_height = (int)ratio*height;

  if(pimg_bgimg->width - new_width-5 < 0 || pimg_bgimg->height - new_height - 5 < 0){
	LOG(WARNING)<<"backgrand image's width or height is less than img"; 
	IplImage* tmp = cvCreateImage(cvSize(width,height), IPL_DEPTH_8U,3);
	cvSet(tmp, cvScalar(rand()%200+50,rand()%200+50,rand()%200+50)); 
    float alpha=jsonConfig["backgrand"]["fusion"]["a"];
    float beta=jsonConfig["backgrand"]["fusion"]["b"];
    float gamma=jsonConfig["backgrand"]["fusion"]["c"];
    cvAddWeighted(pimg,alpha,tmp,beta,gamma,pimg);
    cvReleaseImage(&tmp);
	return pimg;  
  }

  int start_x=rand()%(pimg_bgimg->width-new_width-5); //lx modified, to make more resduial
  int start_y=rand()%(pimg_bgimg->height-new_height-5);

  std::cout << "bgimg size " << pimg_bgimg->width << ", " << pimg_bgimg->height << std::endl;
  CvRect ROIrect = cvRect(start_x, start_y, new_width, new_height);
  cvSetImageROI(pimg_bgimg , ROIrect );
  IplImage *new_img = cvCreateImage(cvSize(width, height), pimg->depth, pimg->nChannels);
  cvResize(pimg_bgimg, new_img, CV_INTER_CUBIC);
  
  ROIrect = cvRect(0, 0, width, height);
  cvSetImageROI(new_img , ROIrect );
  float alpha=jsonConfig["backgrand"]["fusion"]["a"];
  float beta=jsonConfig["backgrand"]["fusion"]["b"];
  float gamma=jsonConfig["backgrand"]["fusion"]["c"];
  cvAddWeighted(pimg,alpha,new_img,beta,gamma,pimg);
  cvResetImageROI(pimg_bgimg);
  cvResetImageROI(new_img);
  cvReleaseImage(&new_img);

  return pimg;      
}

/*
  if add forgrand backgrand fail will, it will return no backgrand result 
*/
IplImage* GenData::add_forgrand_backgrand(){
  if(foreground_background_imgs.empty()){
    LOG(WARNING)<<"foreground_background_imgs are empty.";
  	return add_no_backgrand();
  }

  //random get an image
  int idx=rand()%foreground_background_imgs.size();
  IplImage* pimg_bgimg=foreground_background_imgs[idx];

  //check the channel and depth
  if(pimg_bgimg->nChannels != 3 || pimg_bgimg->depth != 8){
	LOG(WARNING)<<"chnnels is not equal to 3 or depth is not equal to 8";   
	return add_no_backgrand();
  }

  //check the width and height
  if(pimg_bgimg->width <= width  || pimg_bgimg->height <= height){
	LOG(WARNING)<<"backgrand image's width or height is less than img"; 
	return add_no_backgrand();
  }

  //random a position to get a fushion
  int start_x=rand()%(pimg_bgimg->width-width);
  int start_y=rand()%(pimg_bgimg->height-height);
  CvRect ROIrect = cvRect(start_x, start_y, width, height);
  IplImage* pimg = cvCreateImage(cvSize(width,height), IPL_DEPTH_8U,3);
  cvSetImageROI(pimg_bgimg , ROIrect);
  //cvCopyImage(pimg_bgimg, pimg);
  for(int i=0;i<height;i++){
  	unsigned char* px=(unsigned char*)(pimg->imageData+i*pimg->widthStep);
  	unsigned char* p_bg=(unsigned char*)(pimg_bgimg->imageData+i*pimg_bgimg->widthStep);
  	for(int j=0;j<width;j++){
     px[j]=p_bg[j];
     px[j+1]=p_bg[j+1];
     px[j+2]=p_bg[j+2];
  	}
  }

  //set image value
  for(int i=0; i<pixels.size(); i++){
	if(pixels[i].py<0 || pixels[i].py>=height || pixels[i].px >= width || pixels[i].px<0){
		LOG(WARNING)<<"pixel out of image range";
		LOG(WARNING)<<"\tpy = " <<pixels[i].py<<endl;
		LOG(WARNING)<< "\tpx = " <<pixels[i].px<<endl;
		LOG(WARNING)<< "\twidth = " <<width<<endl;
		LOG(WARNING)<< "\theight = " <<height<<endl;
		cvReleaseImage(&pimg);
		return NULL; 
   }
   uchar* pdata=(uchar*)(pimg->imageData+pixels[i].py*pimg->widthStep+3*pixels[i].px);
   pdata[0]=pixels[i].color;
   pdata[1]=pixels[i].color;
   pdata[2]=pixels[i].color;
 }   
  cvResetImageROI(pimg_bgimg);
  return pimg;   
}


IplImage* GenData::add_backgrand(){
  // select a method to generate backgrand
  srand((unsigned)GetTickCount());
  float no_backgrand_problity=jsonConfig["backgrand"]["no_backgrand"]["probility"];
  float foreground_background_problity=jsonConfig["backgrand"]["foreground_background"]["probility"];
  float fusion_background_problity=jsonConfig["backgrand"]["fusion"]["probility"];
  float ticket=rand()%100/100.0;
  if(ticket<no_backgrand_problity){
  	 printf("  add no backgrand.\n");
	 return add_no_backgrand();
	}
  else if(ticket >= no_backgrand_problity && ticket < no_backgrand_problity+foreground_background_problity){
     printf("  add forgrand backgrand.\n");
     return add_forgrand_backgrand();
   }
  else {
  	printf("  add fushion backgrand.\n");
    return add_fushion_backgrand();
 }
}

bool GenData::serialize(string& name)
{
	//write image
	SetImgHeight();

	if(width<=0 || height<=0 || Sentence.size() == 0)
	{
		LOG(WARNING)<<" :height or width <=0 with name = "<<name;
		return false;
	}

	// cyy reviewed at 2014.11.13
	int blur_flag = 1;
	int rotate_flag = 1;
	int pepper_flag = 1;
	int shift_flag = 1;
	int resize_flag = 1;
	int line_flag = 1;
	
	printf("########## %s ##########\n", name.c_str());
	//add backgrand
    IplImage* pimg=add_backgrand();
    if(pimg==NULL){
    	LOG(ERROR)<<"add backgrand failed";
    	return false;
    }
	printf("  Origin size: width is %d, height is %d.\n", pimg->width, pimg->height);
	float addline_problity=jsonConfig["addline_problity"];
    printf("  add line: problity is %f.\n", addline_problity);

   //shuffle noise 
    vector<int> v;
    for(int i=1;i<=6;i++)
      v.push_back(i);
   random_shuffle(v.begin(), v.end());
   for(int i=0;i<v.size();i++){
	if(resize_flag == 1 && v[i] == 1)
	{
		IplImage * tpImg = pimg;
		pimg = add_resize(tpImg, Sentence);
		if(pimg==NULL)
		  pimg=tpImg;
		else
		cvReleaseImage(&tpImg);
	}
	if(rotate_flag == 1 && v[i] == 2)
	{
		IplImage* tpImg = pimg;
		pimg = add_rotate(tpImg, Sentence);
		if(pimg==NULL)
			pimg=tpImg;
		else
		cvReleaseImage(&tpImg);
	}
	if(shift_flag == 1 && v[i] == 3) // put it at the last.
	{
		IplImage * tpImg = pimg;
		pimg = add_shift(tpImg, Sentence, 16);
		if(pimg==NULL)
		  pimg=tpImg;
		else
		cvReleaseImage(&tpImg);
	}
	if(pepper_flag == 1 && v[i] == 4)
	{
		IplImage * tpImg = pimg;
		pimg = add_pepper_noise(tpImg);
		if(pimg==NULL)
			pimg=tpImg;
		else
		cvReleaseImage(&tpImg);
	}
	if(blur_flag == 1 && v[i] == 5)
		add_blur(pimg);
    
    if(line_flag == 1 && v[i] == 6 && rand()%100 <= int(100*addline_problity) ){
      int start1 = rand()%(pimg->width/4);
      int start2 = 3*pimg->width/4+rand()%(pimg->width/4);
      int end1=rand()%pimg->height;
      int end2=rand()%pimg->height;
      int thickness=rand()%2+1;
      cvLine( pimg, cvPoint(start1,end1), cvPoint(start2,end2), cvScalar(rand()%100,rand()%100,rand()%100), thickness);
    }
	  
   }
	printf("####################\n");
	
	string ipg_name=name+".jpg";
    // lx added for the imglist
    int new_hei = 48;
    double scale = (double)new_hei / pimg->height;
    int new_wid = (int)(scale * pimg->width);
    IplImage *new_img = cvCreateImage(cvSize(new_wid, new_hei), pimg->depth, pimg->nChannels);
    cvResize(pimg, new_img, CV_INTER_CUBIC);
	cvSaveImage(ipg_name.c_str(),new_img);
	cvReleaseImage(&pimg);
	cvReleaseImage(&new_img);

	//write label output ucs-4
	string out_name=name+".label";
	//ofstream out(out_name.c_str());
    ofstream g_out(g_img_list.c_str(), std::ios::app);
    g_out << new_wid << " "
          << new_hei << " "
          << ipg_name << " ";
	
	//计算整个外框的对角点
	int xmin=100000;
	int ymin=100000;
	int xmax=-1;
	int ymax=-1;
	//write in the head
	for (int i = 0; i < Sentence.size(); ++i)
	{
		// cyy reviewed at 2014.12.02.
		if(Sentence[i].left == -1) continue;
		
		//output retangle 
		int x1=Sentence[i].left;
		int x2=Sentence[i].left+Sentence[i].width;
		int y1=Sentence[i].top;
		int y2=Sentence[i].top+Sentence[i].height;
		if(x1<xmin) xmin=x1;
		if(x2>xmax) xmax=x2;
		if(y1<ymin) ymin=y1;
		if(y2>ymax) ymax=y2; 
	}
	//out<<xmin<<" "<<ymin<<" "<<xmax-xmin+1<<" "<<ymax-ymin+1<<"\n";
	
	//cyy
	/*int char_c = 0;
	for (int i = 0; i < Sentence.size(); ++i)
	{
		// cyy reviewed at 2014.12.02.
		if(Sentence[i].left == -1) continue;
	
		write_single_ucs4_to_utf8(out, Sentence[i].letter);
		char_c++;
	} 
	  
	out<<"\n";
	
	//cyy
	if(char_c == 0)// no valid chars. remove files.
	{
		char cmd[400];
		sprintf(cmd, "rm -f %s", ipg_name.c_str());
		system(cmd);
		sprintf(cmd, "rm -f %s", out_name.c_str());
		system(cmd);
		printf("No chars to record.\n");
		return false;
	}*/
	
	
	
	//write to utf-8 file
	for (int i = 0; i < Sentence.size(); ++i)
	{
		// cyy reviewed at 2014.12.02.
		if(Sentence[i].left == -1) continue;
		
		//if it is tab or blank, continue;
		if(Sentence[i].width==0 || Sentence[i].height==0)
			continue;

		//output retangle 
		int x1=Sentence[i].left;
		int y1=Sentence[i].top;
		//out<<x1<<" "<<y1<<" "<<Sentence[i].width<<" "<<Sentence[i].height<<endl;
		
		//write letter
		//write_single_ucs4_to_utf8(out, g_out, Sentence[i].letter);
		write_single_ucs4_to_utf8(g_out, Sentence[i].letter);
		//out<<endl;
	}
    g_out<<endl;
	return true;
}

void GenData::SetImgWidth(int mywidth){
   width=mywidth;
}

void GenData::SetImgHeight(void){
  height=0;
  for(int i=0; i<pixels.size(); i++){
    if(pixels[i].py>height)
      height=pixels[i].py;
  }
  height++;
  //give some padding
  height+=5;
}

void GenData::SetPixel(int py, int px, uchar color){
   pixels.push_back(Pixel(py,px,color));
}

void GenData::SetLetter(wchar_t letter, int mytop, int myleft, int mywidth, int myheight){
  Sentence.push_back(Letter(letter,mytop,myleft,mywidth,myheight));
}

//返回值：大端返回1，小端返回0
int IS_BIG_ENDIAN=0;
void SetEndian(void){
  int i=0x12345678;
  char *c=(char *)&i;
  IS_BIG_ENDIAN=(*c==0x12)?1:0;
}

int GetUnicodeIdx(const char* p){
   char data[4];
   for(int i=0;i<4;i++)
     if(IS_BIG_ENDIAN==0) { //little endian
         data[i]=p[3-i];
       } else {  //big endian
         data[i]=p[i];
      }
    int idx=*(int*)data;
    return idx;
}

int GenData::get_Sen_size()
{
    return Sentence.size();
}
