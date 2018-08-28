#include "ft2build.h"
#include FT_FREETYPE_H 
#include FT_GLYPH_H

#include "opencv2/opencv.hpp"

#include "glog/logging.h"

#include <stdio.h>
#include <unistd.h>
#include <string>
#include <fstream>
#include <vector>
#include <iostream>

using namespace std;
using namespace cv;
using namespace google;

void showHelp(char* exe){
	cout<<"Usage: "<<exe<<" -i my.ttf -u 67 -s 24 -o letter.jpg"<<endl;
	cout<<"\t-h :show this help."<<endl;
	cout<<"\t-i :input ttf file"<<endl;
	cout<<"\t-u :input unicode letter"<<endl;
	cout<<"\t-s :input font pixel size"<<endl;
	cout<<"\t-o :output img file"<<endl;	
}

int main(int argc, char* argv[]){
  //init 
  string ttf;
  int font_size;
  string output;
  long idx;
 int  ps=0;
 //init glog
 google::InitGoogleLogging(argv[0]);
 //google::SetLogDestination(google::INFO,"../log/singleletter.log");
 FLAGS_logbufsecs = 0;      //日志实时输出
 FLAGS_max_log_size = 1024; // max log size is 1024M

  int option;
  while((option = getopt(argc,argv,"hi:o:s:u:"))!=-1){
	switch (option)
		{
			case 'i':
			    ttf=optarg;
			    PCHECK(access(ttf.c_str(),R_OK)==0)<<" :input file can not be read";
			    ps++;
				break;
			case 's': 
                font_size = atoi(optarg);
                LOG_IF(FATAL,font_size<=0)<<" :input font size is <0";
                ps++;
				break;
			case 'o': 
                output = optarg;
                ps++;
				break;
			case 'u': 
                idx = atoi(optarg);
                LOG_IF(FATAL,idx<=0)<<" :input unicode num is <0";
                ps++;
				break;
			case 'h': 
                showHelp(argv[1]);
				return -1;
			default:
				LOG(WARNING)<<" :unknown input parameter.";
				showHelp(argv[0]);
				return -1;
		}
  }

  if(ps!=4){
  	showHelp(argv[0]);
  	return -1;
  }


 //test freetype
 FT_Library     pFTLib        =  NULL;  
 FT_Face        pFTFace       =  NULL;  
 FT_Error       error         =   0 ;  

 //Init FreeType Lib to manage memory  
 error  =  FT_Init_FreeType( & pFTLib);  
 if (error) {  
    pFTLib  =   0 ;  
    printf( " There is some error when Init Library " );  
    return   - 1 ;  
 }  
  
 //create font face from font file  
 error  =  FT_New_Face(pFTLib,  ttf.c_str() ,  0 ,  & pFTFace);  
 if ( ! error)  
 {  
    error = FT_Set_Pixel_Sizes(pFTFace, 0, font_size);
    LOG_IF(FATAL, error!=0)<<" :set pixel size failed"; 
    //  load glyph  
    FT_Load_Glyph(pFTFace, FT_Get_Char_Index(pFTFace,  idx ), FT_LOAD_DEFAULT);  
    error = FT_Render_Glyph(pFTFace->glyph, FT_RENDER_MODE_NORMAL);
	if (error) {
           LOG(FATAL)<<" \t:render glyph failed"; 
	} else {  
        int rows = pFTFace->glyph->bitmap.rows;
		int width = pFTFace->glyph->bitmap.width;
		int left = pFTFace->glyph->bitmap_left;
		int top = pFTFace->glyph->bitmap_top;
		int letter_width=pFTFace->glyph->metrics.horiAdvance>>6;

		//save img
        IplImage* pimg=cvCreateImage(cvSize(letter_width,font_size),IPL_DEPTH_8U,1);
        cvSetZero(pimg);
        for ( int  i = 0 ; i < rows;  ++ i)   
             for ( int  j = 0 ; j < width;  ++ j)  //if it has gray>0 we set show it as 1, o otherwise  
                 if(pFTFace->glyph->bitmap.buffer[i * width + j]>0)
                 	 if(i<top && j+left < letter_width) //pimg[i+font_size-top][j+left]=255;
                 	    *(uchar*)(pimg->imageData+(i+font_size-top)*pimg->widthStep+(j+left))=255;

        //show image
       for ( int  i = 0 ; i < font_size;  ++ i){ 
          uchar* Mi=(uchar*)(pimg->imageData+i*pimg->widthStep);
          for ( int  j = 0 ; j < letter_width;  ++ j) { 
               if(Mi[j]==0) 
                   cout<<'0';  
               else
               	   cout<<'1';
             } 
            cout<<endl; 
        } 

        cvSaveImage(output.c_str(),pimg);
        cvReleaseImage(&pimg); 
    }  
     //  free face  
    FT_Done_Face(pFTFace);  
    pFTFace  =  NULL;  
}  
  
 //  free FreeType Lib  
 FT_Done_FreeType(pFTLib);  
 pFTLib  =  NULL;  

 return 0;
}
