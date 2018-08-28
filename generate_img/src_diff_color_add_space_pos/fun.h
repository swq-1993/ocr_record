#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <iconv.h>
#include "opencv2/opencv.hpp"
#include <glog/logging.h>
#include "json.h"
#include <stdio.h>
#include <unistd.h>

using namespace std;
using namespace cv;
using namespace google;
using namespace json;

enum ERRORNUM
{
		ERRORPARAMETERNUM=-1,
		EMPTYINPUTFILE=-2,
		ERRORFONTSIZE=-3,
		ERRORFONTPATH=-4,
		ERRORDECODEJSON=-5,
		ERRORBACKGRANDKEY=-6,
		ERRORBACKGRANDVALUE=-7,
		UNKNOWPARAMETER=-8,
		ERRORINITFREETYPE=-100,
		ERRORLOADFACE=-101,
		ERRORSETFONTSIZE=-102,

};

//utf8->ucs-4
class CodeConverter {
   private: 
      iconv_t cd;
   public:
   // 构造
   CodeConverter(const char * from_charset, const char * to_charset) {
    cd = iconv_open(to_charset,from_charset);
   }

   // 析构
   ~CodeConverter() {
     iconv_close(cd);
   }

 // 转换输出
 int convert(char *inbuf,size_t inlen, char *outbuf, size_t outlen) {
    memset(outbuf,0,outlen*sizeof(char));
    return iconv(cd, &inbuf, &inlen, &outbuf, &outlen);
   }
};

void SetEndian(void);

int GetUnicodeIdx(const char* p);

typedef struct Letter{
	int top;
	int left;
	int width;
	int height;
	wchar_t  letter; //linux sizeof(wchar_t)==4
	Letter(wchar_t myletter, int mytop, int myleft, int mywidth, int myheight )\
	              :top(mytop),left(myleft),width(mywidth),height(myheight),letter(myletter){}
};

typedef struct Pixel{
	int px;
	int py;
	uchar color;
	Pixel(int mypy, int mypx, uchar mycolor):px(mypx),py(mypy),color(mycolor){}
};

typedef struct RectLean
{
	struct Pixel top_left;
	struct Pixel top_right;
	struct Pixel bot_left;
	struct Pixel bot_right;
	RectLean():top_left(Pixel(0,0,0)),top_right(Pixel(0,0,0)),bot_left(Pixel(0,0,0)),bot_right(Pixel(0,0,0)){};
};

class GenData{ 
public:
	//write jpg file and label file
    GenData(){};

public:
	void SetPixel(int py, int px, uchar color);
	void SetLetter(wchar_t letter, int mytop, int myleft, int mywidth, int myheight);
	void SetImgWidth(int width);
    void SetImgHeight(void);
	bool serialize(string& name);
    
    //cyy.
    int get_Sen_size();

private:
	int width;
	int height;
	vector<Letter>  Sentence;      //letters
	vector<Pixel> pixels;          //pixels

public: 
	static Value jsonConfig;
    static vector<IplImage*> foreground_background_imgs;
    static vector<IplImage*> fushion_imgs;
 private:
   void addPadding();
   IplImage* add_backgrand();
   IplImage* add_no_backgrand();
   IplImage* add_fushion_backgrand();
   IplImage* add_clear_bg_fushion_backgrand();
   IplImage* add_forgrand_backgrand();
   IplImage* add_finegrand_forgrand_backgrand();
   void add_blur(IplImage *pImg);
   IplImage* add_shift(IplImage* src, vector<Letter>& Sentence, int fontsize);
   IplImage* add_resize(IplImage* src, vector<Letter>& Sentence);
   IplImage* add_pepper_noise(IplImage* src);
   IplImage* add_rotate(IplImage* src, vector<Letter>& Sentence);
   void addPerspective();
   
};


