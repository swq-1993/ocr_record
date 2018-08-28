/*==================================================================================================
CHANGELOG:
==========
10/30/2014: v1.1
----------
* 修改了label文件，整行数据跳过空格的问题
==========
3/1/2015: v2.0
----------
* 注意输入图像是非bom格式的utf8文件，同时保证不含有除\n外的不可见字符
==================================================================================================*/
#include "ft2build.h"
#include FT_FREETYPE_H 
#include FT_GLYPH_H

#include "opencv2/opencv.hpp"
#include "glog/logging.h"
#include "json.h"
#include "fun.h"

#include <stdio.h>
#include <unistd.h>
#include <string>
#include <strstream>
#include <fstream>
#include <vector>
#include <ctime>

using namespace std;
using namespace cv;
using namespace google;
using namespace json;

std::string g_img_list;
std::string g_fusion_bg_imgs = "";

void showHelp(char* exe){
	cout<<"Usage: "<<exe<<" -i utf8.txt  -s 32 -t STFANGSO.TTF -j test.json"<<endl;
	cout<<"\t-v :show the version"<<endl;
	cout<<"\t-h :show this help."<<endl;
	cout<<"\t-o :the output dir"<<endl;
	cout<<"\t-i :the input utf-8 file, each line contains an example"<<endl;
	cout<<"\t-l :the input utf-8 string"<<endl;
	cout<<"\t-s :set font size in pixel, please note if it is set too small(usually < 5), the result"<<endl;
	cout<<"\t    of the rendered glyph may be effected"<<endl;
	cout<<"\t-t :the input font file, most true type format is supported "<<endl;
	cout<<"\t-j :the input json conf file "<<endl;
}


void readBgImgs(string bg_img, vector<IplImage*>& imgs){
  //load all the bg imgs
  ifstream in(bg_img.c_str());
  if(NULL==in){
     LOG(ERROR)<<"read backgrand image list failed wirh filename = "<< bg_img;
   }

  //read all the images
  string line;
  while(getline(in,line)){ //read all the images one time
  	 IplImage* pimg_bgimg=cvLoadImage(line.c_str(),CV_LOAD_IMAGE_COLOR); //will auto release before exit
	 if(pimg_bgimg==NULL){
		LOG(WARNING)<<"load backgrand image failed with file = "<<line;
		cvReleaseImage(&pimg_bgimg);
		continue;	
	   }
     imgs.push_back(pimg_bgimg);
  }  
}

void readoneBgImgs(vector<IplImage*>& imgs) {
    IplImage* pimg_bgimg=cvLoadImage(g_fusion_bg_imgs.c_str(),CV_LOAD_IMAGE_COLOR); //will auto release before exit
    if(pimg_bgimg==NULL){
        LOG(WARNING)<<"load backgrand image failed with file = " << g_fusion_bg_imgs;
		cvReleaseImage(&pimg_bgimg);
    }
    imgs.push_back(pimg_bgimg);
}


int checkJson(const char* jsonfile, Value& jsonConfig){
  //read json file content
  filebuf *pbuf;
  ifstream filestr;
  long size;
  char * buffer;
  filestr.open (jsonfile, ios::binary);
  //文件打开失败

  pbuf=filestr.rdbuf();
  
  // get file size
  size=pbuf->pubseekoff (0,ios::end,ios::in);
  pbuf->pubseekpos (0,ios::in);
   
  //allocate memery
  buffer=new char[size+1];
  
  // get file content
  pbuf->sgetn(buffer,size);
  filestr.close();
  string jsonString(buffer);
  delete [] buffer;
  
  //decode json file
  jsonConfig=Deserialize(jsonString);
  if(jsonConfig.GetType()==NULLVal){ //json decode failed
    LOG(ERROR)<<"input invaild json format";
    return ERRORDECODEJSON;
  }

 //check the json value ** not complete, if the key name or type is errer, the program will core ** 
 if(!jsonConfig.HasKey("backgrand")){
 	LOG(ERROR)<<"the json file has not the key backgrand";
 	return ERRORBACKGRANDKEY;
 }
 Value backgrand = jsonConfig["backgrand"];
 if(backgrand.GetType()!=ObjectVal){
    LOG(ERROR)<<"the backgrand is not object type";
 	return ERRORBACKGRANDVALUE;
 }

 float no_backgrand_problity=jsonConfig["backgrand"]["no_backgrand"]["probility"];
 float foreground_background_problity=jsonConfig["backgrand"]["foreground_background"]["probility"];
 string foreground_background_list=jsonConfig["backgrand"]["foreground_background"]["list"];
 float fusion_background_problity=jsonConfig["backgrand"]["fusion"]["probility"];
 string fusion_background_list=jsonConfig["backgrand"]["fusion"]["list"];
 float fusion_background_a=jsonConfig["backgrand"]["fusion"]["a"];
 float fusion_background_b=jsonConfig["backgrand"]["fusion"]["b"];
 float fusion_background_c=jsonConfig["backgrand"]["fusion"]["c"];
 float rotation=jsonConfig["rotation"];
 int Gaussian_blur_kernal_size=jsonConfig["Gaussian_blur_kernal_size"];
 float resize_rate=jsonConfig["resize_rate"];
 float pepper_noise=jsonConfig["pepper_noise"];
 int shift=jsonConfig["shift"];

 //read all the backgrand images
 /*if (g_fusion_bg_imgs.length() == 0) {
    readBgImgs(foreground_background_list, GenData::foreground_background_imgs);
    readBgImgs(fusion_background_list, GenData::fushion_imgs);
 }
 else {*/
 readoneBgImgs(GenData::foreground_background_imgs);
 readoneBgImgs(GenData::fushion_imgs);
 //}
}


/*
errer code:
num: success generate line num(num>=1)
-1 : input parameter num errer
-2 : 

*/

int main(int argc, char* argv[])
{
	//init input parameters
	string input_file;
	string ttf_path;
    string input_string = "";
	int fontSize;
    string ext_name = "";
	Value myjsonConfig;
    string foreground_background_imgs_str;
	//init google log
	google::InitGoogleLogging(argv[0]);
	google::SetLogDestination(google::INFO, "../log/log_main");
	google::SetStderrLogging(google::INFO);
	FLAGS_logbufsecs = 0;      //日志实时输出
	FLAGS_max_log_size = 64; // max log size is 64M
    string outputname1="../output/";
    
    /*if(argc!=2&&argc!=9){
       LOG(ERROR)<<"input parameter num is not equal to 2 or 9";
       showHelp(argv[0]);
       return ERRORPARAMETERNUM;
    }*/

	//get options
	int option;
    int rt = 0;
	while((option = getopt(argc,argv,"vhi:s:t:j:o:l:e:m:"))!=-1)
	{
		switch (option)
		{
			case 'i':
			    LOG(INFO)<<"input utf-8 file "<<optarg;
				input_file = optarg;
				/*if(0!=access(input_file.c_str(),R_OK)){
                  LOG(ERROR)<<"input utf-8 file can not be read";
                  return EMPTYINPUTFILE;
				}*/
				break;
            // lx added
			case 'o':
			    LOG(INFO)<<"output path "<<optarg;
				outputname1 = optarg;
				if(outputname1[outputname1.size()-1] != '/'){
                    outputname1 += "/";
				}
				break;
			case 'l':
			    LOG(INFO)<<"input utf-8 file "<<optarg;
				input_string = optarg;
				if(input_string.length()<=0){
                  LOG(ERROR)<<"input string can not be read";
                  return EMPTYINPUTFILE;
				}
				break;
			case 'e':
			    LOG(INFO)<<"the result start id is "<<optarg;
				ext_name = optarg;
				break;
            case 'm':
			    LOG(INFO)<<"bg_imgs "<<optarg;
				g_fusion_bg_imgs = optarg;
				if(g_fusion_bg_imgs.length()<=0){
                  LOG(ERROR)<<"bg imgs can not be read";
                  return EMPTYINPUTFILE;
				}
				break;
			case 's':
			    LOG(INFO)<<"input fontSize is "<<optarg;
                fontSize=atoi(optarg);
                if(fontSize<=5){
                  LOG(ERROR)<<"input font size is less than 5";
                  return ERRORFONTSIZE;
				}	
				break;
			case 't':
			    LOG(INFO)<<"input ttf path is "<<optarg;
                ttf_path=optarg;
                if(access(ttf_path.c_str(),R_OK)!=0){
                  LOG(ERROR)<<"input font file can not be read";
                  return ERRORFONTPATH;
				}
				break;
			case 'j':
			     LOG(INFO)<<"input json config path is "<<optarg;
			     //decode json and check value and read all the bgimgs
			     rt=checkJson(optarg,myjsonConfig); 
			     if(rt<0)
			     	return rt;

			     //set the json file
			     GenData::jsonConfig=myjsonConfig;
				 break;
			case 'h':
			    showHelp(argv[0]);
			    return 0;
			case 'v':
			    cout<<"Version 2.2, Released by 2015.3.25"<<endl;
			    return 0;
			default:
				LOG(WARNING)<<"unknown input parameter";
				showHelp(argv[0]);
				return UNKNOWPARAMETER;
		}
	}
    
	//init codec and set endian
	CodeConverter cc = CodeConverter("utf-8","ucs-4");
	SetEndian();

	//init free type
	FT_Library freetype;
	if (FT_Init_FreeType(&freetype)){
	 LOG(ERROR)<<"Initialize the FreeType library failed";
     return ERRORINITFREETYPE;
	}
    

	// Create face object
	FT_Face face;
	int error = FT_New_Face(freetype, ttf_path.c_str(), 0, &face);
	if(error){
	  LOG(ERROR)<<"Load font file faild, Unknown File Format";	
	  return ERRORLOADFACE;	
	}

   
	//set font size
	error = FT_Set_Pixel_Sizes(face, 0, fontSize);
	if(error){
	  LOG(ERROR)<<"set font size failed";
	  return ERRORSETFONTSIZE;
	}
	//lx added
    //string outputname1="../output/";
    g_img_list = outputname1 + "img_list.txt";

	//generate jpg and label

    ifstream in;
    if (0!=access(input_file.c_str(),R_OK)) {
	    in.open(input_file.c_str());
    }
    else if (input_string.length() <= 0) {
        LOG(ERROR)<<"Input file and string error";
        return EMPTYINPUTFILE;
    }
	string line;
	for(int num=1; input_string.length()>0 || getline(in,line); num++)
	{   
        //lx added, to allow the input string
        if (input_string.length()>0) {
            line = input_string;
        }
        if (input_string.length()>0 && num>1) {
            break;
        }

        DLOG(INFO)<<"start generate line num ：" << num;
	    //num 记录行号
		//len 表示ucs-4的最大长度
		int len=4*line.size();
		if(len==0) 
		{
			LOG(WARNING)<<"skip empty line : "<<num;
			continue;
		}

		//引用计数，最好用c++的boost库
		bool is_del=false;
		char* out= new char [len];
		size_t rt=cc.convert((char*)line.c_str(),line.size(), out, len);
		if(rt==-1)
		{
			LOG(WARNING)<<"translate utf-8 to ucs-4 failed with line num : "<<num;
			if(is_del==false)
			{
				delete [] out; 
				is_del=true;    	
			}
			continue;
		}

		//init  data generation object
		GenData gd;
		// cyy reviewed at 201.12.8. 起始坐标偏移(1,1),总宽度偏移+1
		int X = 1, Y = 1;
		
		int ignore_line=false; //一行中有一个字结果不对，要跳过该行
		int all_blanks=true;   //一行中全部都是空格或者回车，跳过
		for (int i=0; i<len/4; i++) //at most len/4 letters
		{
			int idx=GetUnicodeIdx(out+4*i);
			DLOG(INFO)<<"load letter unicode4 charidx= " <<idx;
			
			//convent end
			if(idx==0)
			{
				if(is_del==false)
				{
					delete [] out; 
					is_del=true;    	
				}

				//cyy. 
				break;
			}
			
			if(idx<0)
			{
				LOG(WARNING)<<"get ucs4 index error with line " << num <<" and idx = "<<idx;
				if(is_del==false)
				{
					delete [] out; 
					is_del=true;    	
				}
				ignore_line=true;
				break;
			}

			//get letter
			wchar_t letter=*(wchar_t*)(out+4*i);
			int charidx=FT_Get_Char_Index(face,idx);

			if(charidx==idx || charidx==0)
			{
				LOG(WARNING)<<"load index from font file failed with line = "<< num << "and letter num = "<<i+1;
				if(is_del==false)
				{
					delete [] out; 
					is_del=true;    	
				}
				ignore_line=true;
				break;
			}

			int error = FT_Load_Glyph(face, charidx, FT_LOAD_DEFAULT);
			if(error)
			{
				LOG(WARNING)<<"load glyph failed with line = "<<num<<"and letter num = "<<i+1; 
				if(is_del==false)
				{
					delete [] out; 
					is_del=true;    	
				}
				ignore_line=true;
				break;
			}
		
			error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
			//error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_MONO);
			if (error)
			{
				LOG(WARNING)<<"render glyph failed with line = "<<num<<"and letter num = "<<i+1; 
				if(is_del==false)
				{
					delete [] out; 
					is_del=true;    	
				}
				ignore_line=true;
				break;
			}

			//draw to image
			int x, y;
			int rows = face->glyph->bitmap.rows;
			int width = face->glyph->bitmap.width;
			int left = face->glyph->bitmap_left;
			int top = face->glyph->bitmap_top;
			// cout<<"left = "<<left<<", width = "<<width<<", gap = "<<(face->glyph->metrics.horiAdvance>>6)<<endl; 
			//set letter
			// cyy: sometimes, left < 0
			if(left < 0)
				gd.SetLetter(letter, Y+fontSize-top, X , width+left, rows);	
			else
				gd.SetLetter(letter, Y+fontSize-top, X+left , width, rows);	

			if(0 == rows || 0 == width)//if in the range of word , it is wrong 
			{
				//move X
				X += std::max((int)(face->glyph->metrics.horiAdvance>>6), left * 2);
				// cyy.
				DLOG(INFO)<<"letter_unicode = "<< idx <<" can not plot with width or height = 0";
				//LOG(WARNING)<<"letter_unicode = "<< idx <<" can not plot with width or height = 0";
				//break;
				continue;
			}

			all_blanks=false;
			
			if(left < 0)
				for (y=0; y<rows; y++)
				{
					for (x=-left; x<width; x++)
					{
						int px = X + left + x;
						int py = Y + fontSize - top + y;
						uchar color = 255-face->glyph->bitmap.buffer[y*width+x];
						gd.SetPixel(py, px, color);
					}
				}
			else
				for (y=0; y<rows; y++)
				{
					for (x=0; x<width; x++)
					{
						int px=X + left + x;
						int py=Y + fontSize - top + y;
						uchar color = 255-face->glyph->bitmap.buffer[y*width+x];
						
						/*
						if(py == -1)
							printf("\n Y = %d, fontSize = %d, top = %d, y = %d.\n", Y, fontSize, top, y);
						if(px == -1)
							printf("\n X = %d, left = %d, x = %d.\n", X, left, x);
						*/
						gd.SetPixel(py, px, color);
					}
				}

			//move X
			X += face->glyph->metrics.horiAdvance>>6;
		}  
        
        if(ignore_line==true || all_blanks == true){
        	LOG(WARNING)<<"the line = "<< num <<" contains unrendering letter or all the words are blanks";
            continue;
        }
        	
	   // cyy_reviewed at 2014.12.8.
		gd.SetImgWidth(X+2);
		
		//cyy.
		if(gd.get_Sen_size() == 0)
		{ 
			DLOG(ERROR)<<"this line owns 0 chars!";
			if(is_del==false)
			{
				delete [] out; 
				is_del=true;
				
				// cyy;
				//return 0;
			}
			continue;
		}

		//generate output file name
        string outputname = outputname1;
		outputname+=basename(input_file.c_str());
        if (ext_name.size() > 0) {
		    outputname+="_";
		    outputname+=ext_name;
        }
        else {
            outputname+="_";
            strstream ss;
            string s;
            ss << num;
            ss >> s;
            outputname+=s;
        }
		bool gdrt=gd.serialize(outputname);

		//release
		if(is_del==false)
		{
			delete [] out; 
			is_del=true;    	
		}
		if(gdrt)
		  LOG(INFO)<<"scuess to generate line num = "<< num;
	    else
	      LOG(WARNING)<<"failed to generate line num = "<< num;	
	}

	//  free face  
	FT_Done_Face(face);  
	face  =  NULL; 

	//  free FreeType Lib  
	FT_Done_FreeType(freetype);  
	freetype  =  NULL; 
	return 0;
}

