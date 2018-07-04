#-*- coding: UTF-8 -*-
import os, sys, string, codecs
import os.path
import binascii
import random
from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont
#"""author:xieshufu @baidu.com 2015.03.22"""
#"""show the ocr result on the image"""
 
def trans(left, top, width, height, dir, im_wid, im_hei):
    #if dir == 1 or dir == 3:
    #    tmp = im_hei
    #    im_hei = im_wid
    #    im_wid = tmp
    tmp_left = left
    tmp_top  = top
    tmp_right  = left + width - 1
    tmp_bottom = top + height - 1
    if dir == 1:
        tmp_left = im_wid - top
        tmp_top  = left
        tmp_right  = tmp_left - height + 1
        tmp_bottom = tmp_top + width - 1
    elif dir == 2:
        tmp_left = im_wid - left
        tmp_top  = im_hei - top
        tmp_right  = tmp_left - height + 1
        tmp_bottom = tmp_top - width + 1
    elif dir == 3:
        tmp_left = top
        tmp_top  = im_hei - left
        tmp_right  = tmp_left + height - 1
        tmp_bottom = tmp_top - width + 1 

    new_left = min(tmp_left, tmp_right)
    new_top  = min(tmp_top, tmp_bottom)
    new_right  = max(tmp_left, tmp_right)
    new_bottom = max(tmp_top, tmp_bottom) 
    return new_left, new_top, new_right, new_bottom       


def DrawImageRst(input_img_path, rst_list, line_num, output_img_path):
    im = Image.open(input_img_path).convert("L")
    im_wid = im.size[0]
    im_hei = im.size[1]
    max_hei = max(int(im_hei*2.2), 60)
    im_draw = Image.new("RGB", (im_wid, max_hei), (255,255,255))
    im_draw.paste(im, (0,0))
    
    font_face = ImageFont.truetype('msyh.ttf', 15)
    fill_color = (255,0,0)
    line_color_box = (0,255,0)
    line_color = (0,0,255)
    draw = ImageDraw.Draw(im_draw)
    line_hei = 0
    dir = 0
    if line_num>0:
        line_hei = im_hei/line_num
    for i in range(0, line_num):
        line_rst = rst_list[i]
        infor_len = len(line_rst)
        if i == 0:
            #dir = int(float(line_rst[3]))
            dir = 0
            if dir == 1:
                im = im.transpose(Image.ROTATE_270)
            elif dir == 2:
                im = im.transpose(Image.ROTATE_180)
            elif dir == 3:
                im = im.transpose(Image.ROTATE_90)
            
            im_wid = im.size[0]
            im_hei = im.size[1]
            max_hei = max(int(im_hei*2.2), 60)
            im_draw = Image.new("RGB", (im_wid, max_hei), (255,255,255))
            im_draw.paste(im, (0,0))
            draw = ImageDraw.Draw(im_draw)
            continue
        if(infor_len<5):
            continue
        char_left = int(line_rst[0])
        char_top = int(line_rst[1])
        char_width = int(line_rst[2])
        char_height = int(line_rst[3])
        char_right = char_left + char_width - 1 
        char_bottom = char_top + char_height - 1 
        char_left, char_top, char_right, char_bottom = trans(char_left, char_top, char_width, char_height,\
                                                             dir, im_wid, im_hei)
        #char_right = char_left + char_width - 1 
        #char_bottom = char_top + char_height - 1 
        char_txt = line_rst[4]
        left_all = char_left
        top_all = char_top
        draw.line([(char_left, char_top),(char_right, char_top)], line_color)
        draw.line([(char_left, char_top),(char_left, char_bottom)], line_color)
        draw.line([(char_right, char_bottom),(char_right, char_top)], line_color)
        draw.line([(char_right, char_bottom),(char_left, char_bottom)], line_color)
        try:
                unicode_txt = unicode(char_txt, 'utf-8')
        except Exception,e:
                unicode_txt = char_txt
        text_color = (255, 0, 0)
        draw.text((char_left, im_hei+char_top), unicode_txt, font=font_face, fill=text_color)
    im_draw.save(output_img_path, 'JPEG')
    return
    
def LoadOCRResult(result_file_path):
    file_result = open(result_file_path, 'r')

    rst_list = []
    line_index = 0
    line = file_result.readline()
    while line:
        line_strip = line.strip()
        try:
            #line_utf_decode = line_strip.decode('gbk')
            line_utf_decode = line_strip.decode('utf-8')
            line_utf_encode = line_utf_decode.encode('utf-8')
        except Exception,e:
            line_utf_encode = line_strip

        line_str = line_utf_encode.split('\t')  
        rst_list.append(line_str)
        line_index = line_index + 1
        #print str(line_index) 
        
        line = file_result.readline()   
        #for element in line_str:
        #   print element       
    
    file_result.close()
    return (rst_list, line_index)
        
    

if __name__=='__main__':
    if(len(sys.argv) != 5):
        print "usage: ShowOCRRst.py input_img_dir ocr_result_dir ext_name save_dir"
        sys.exit(0)

    input_img_dir = sys.argv[1]
    ocr_rst_dir  = sys.argv[2]
    ext_name = sys.argv[3]
    save_dir  = sys.argv[4]

    img_list = os.listdir(input_img_dir)
    if(img_list == []):
        print "no image file found!"
        sys.exit(0)
        
    for img_name in img_list:
        img_name_len = len(img_name)
        print "img_name_len:", img_name_len
        dot_pos = img_name.rfind('.')
        img_ext_type = img_name[(dot_pos+1):img_name_len]
        img_title = img_name[0:dot_pos]
        #input_img_path = input_img_dir + img_name
        input_img_path = os.path.join(input_img_dir, img_name)
        if(img_ext_type!="jpg" and img_ext_type!="png" and img_ext_type!="jpeg"):
            continue
            
        print " title: " + img_title
        #output_img_path = save_dir + img_title + ext_name + ".jpg"
        output_img_path = os.path.join(save_dir, img_title + ext_name + ".jpg")
        #input_rst_path = ocr_rst_dir + img_name + ".txt"
        input_rst_path = os.path.join(ocr_rst_dir, img_name + ".txt")
        print "input_rst_path:", input_rst_path
        #input_rst_path = ocr_rst_dir + img_name
        # check whether the result path exists
        file_exist = os.path.exists(input_rst_path);
        if(not file_exist):
            print "result_path does not exist"
            continue 

        (rst_list, rst_num) = LoadOCRResult(input_rst_path)
        DrawImageRst(input_img_path, rst_list, rst_num, output_img_path)    
