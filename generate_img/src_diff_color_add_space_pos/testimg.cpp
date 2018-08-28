#include  "fun.h"
#include <unistd.h>

void showHelp(char* exe){
	cout<<"Usage: "<<exe<<endl<<"\t-h :show this help."<<endl;
	cout<<"\t-i :input img file"<<endl;
	cout<<"\t-l :input label file"<<endl;
	cout<<"\t-o :output img file"<<endl;
}

int main(int argc, char* argv[]){
  IplImage* pimg;
  char* label_file;
  char* output;
  int option;
  int cout=0;
  //init google log
  google::InitGoogleLogging(argv[0]);
  google::SetLogDestination(google::INFO,"../log/testimg.log");
  google::SetStderrLogging(google::INFO);
  FLAGS_logbufsecs = 0;      //日志实时输出
  FLAGS_max_log_size = 1024; // max log size is 1024M
  while((option = getopt(argc,argv,"hi:l:o:"))!=-1)
	{
		switch (option)
		{
			case 'i':
				pimg=cvLoadImage(optarg);
				LOG_IF(FATAL,pimg==NULL)<<"read image failed";
        cout++;
				break;
			case 'l': 
        label_file = optarg;
        PCHECK(access(label_file,R_OK)==0)<<"read label file failed";
        cout++;
				break;
			case 'o': 
        output = optarg;
        LOG_IF(WARNING,access(output,W_OK)==0)<<"output img exist";
        cout++;
				break;  
     case 'h': 
        showHelp(argv[0]);
        return 0;
			default:
				LOG(WARNING)<<"unknown input parameter.";
				showHelp(argv[0]);
				return -1;
		}
  }
   
  if(cout!=3){
    LOG(WARNING)<<"input parameters number not equal 3 or 1";
    showHelp(argv[0]);
    return -1;
  }

  //read rectangles
  ifstream in(label_file);
  string line;
  for(int i=0;getline(in,line);i++){
  	 if(i%2==1) continue; //words continue
  	  int top, left, width, height;
      sscanf(line.c_str(),"%d %d %d %d", &top, &left, &width, &height);
  	  //plot rectangles
      if(0==i)
        cvRectangle(pimg, cvPoint(top,left), cvPoint(top+width,left+height), cvScalar(0,0,255),2);
      else
        cvRectangle(pimg, cvPoint(top,left), cvPoint(top+width,left+height), cvScalar(0,255,0));
  }
  
  //save image
  cvSaveImage(output,pimg);
  cvReleaseImage(&pimg);
  return 0;
  
}
