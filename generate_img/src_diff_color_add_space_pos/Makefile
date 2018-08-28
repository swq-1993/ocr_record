INCLUDE =   -I ../thirdparty/glog/include \
            -I ../thirdparty/opencv/include \
			-I ../thirdparty/freetype/include/freetype2
			
LIBRARY =   -L ../thirdparty/glog/lib \
            -lglog -static \
			-L ../thirdparty/opencv/lib \
			-lopencv_highgui -lopencv_imgproc -lopencv_core \
			-L ../thirdparty/opencv/3rdparty/lib \
			-lrt -lz -llibjasper -lpthread \
			-lpng -ljpeg -ltiff \
			-L ../thirdparty/freetype/lib \
			-lfreetype -lz -lbz2 \

CC = g++

all : main.cpp fun.cpp
	$(CC)  -Wno-deprecated main.cpp fun.cpp json.cpp -o ocr_tool $(INCLUDE) $(LIBRARY)
	cp ./ocr_tool ../bin/
#-ggdb
.PHONY : clean
clean : 
	find . -maxdepth 1 -name "*.o"  -exec rm {} \;
	find . -maxdepth 1 -name "main" -exec rm {} \;

