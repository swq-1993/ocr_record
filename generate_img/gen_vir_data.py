#!/usr/bin/env Python
# coding=utf-8

import os, sys, random

def ReadTable(table_path):
   tfn = open(table_path)
   cls_dic  = {}
   while 1:
      line = tfn.readline()
      if not line:
         break
      line = line.strip('\n')
      [unic, cls] = line.split()
      unic = unic.strip()
      unic = int(unic)
      cls_dic[unic] = int(cls)
   tfn.close()
   return cls_dic

def ReadFont(font_txt_path):
#return the font list of this font
    fn = open(font_txt_path)
    res_list = []
    while 1:
        line = fn.readline()
        if not line:
            break
        line = line.strip()
        line = line.split('\t')
        res_list.append(int(line[3]))
    return res_list

def remove_special_char(string):
#only cotain chn, eng and num
    cur_str = string.decode('utf8')
    res_str = ""
    for cur_char in cur_str:
        if ord(cur_char) >= 19968 and ord(cur_char) <= 40869: #chn
            res_str += cur_char
            continue
        if ord(cur_char) >= 48 and ord(cur_char) <= 57:
            res_str += cur_char #num
            continue
        if ord(cur_char) >= 65 and ord(cur_char) <= 90:
            res_str += cur_char #upper
            continue
        if ord(cur_char) >= 97 and ord(cur_char) <= 122:
            res_str += cur_char #lower
            continue
    return res_str

if len(sys.argv) != 7:
    print "test.py corpus min_font max_font font_list bg_list output_dir"
    exit(-1)

corpus = open(sys.argv[1])
min_font = int(sys.argv[2])
max_font = int(sys.argv[3])
font_list = open(sys.argv[4])
bg_list = open(sys.argv[5])
output_dir = sys.argv[6]
error_fn = open(os.path.join(output_dir, "non_output.txt"), 'w')

font_char_list = [] # the tabel of it contains which char
font_lines = font_list.readlines()
for font_line in font_lines:
    font_line = font_line.strip()
    cur_font_list = ReadFont(font_line+'.txt')
    font_char_list.append(cur_font_list)
font_list.close()
font_num = len(font_lines)
print "====== finish the font table ======="

cls_dict = ReadTable('./table_eng_97')
corpus_lines = corpus.readlines()
corpus.close()
print "====== finish the corpus ======="

bg_lines = bg_list.readlines()
bg_num = len(bg_lines)
bg_list.close()

font_count_dict = {} #contain the cumsum
num_id = 1
for line in corpus_lines:
    # break lines
   # if num_id <0:
    #    num_id=num_id +1
     #   continue
   
   
    line = line.strip()
    font_size = random.randint(min_font, max_font)
    #input_string = remove_special_char(line)  # the input string
    print line
    input_string = line.decode('utf8')

    valid_font_list = []
    for font_id in range(font_num):
        is_valid = True
        cur_font_list = font_char_list[font_id]
        #for cur_char in input_string:
        #    unicode = ord(cur_char)
        #    if unicode >= 33 and unicode <= 126:
        #        unicode += 65248
        #    if not cls_dict.has_key(unicode):
        #        is_valid = False
        #        break
        #    cls = cls_dict[unicode]
        #    if cur_font_list[cls] == 0:
        #        is_valid = False
        #        break
        if is_valid:
            valid_font_list.append(font_id)
    if len(valid_font_list) == 0:
        print >> error_fn, input_string.encode('utf8')
        continue    

    # construct the cum_sum
    cum_sum = []
    sum = 0
    for id in range(len(valid_font_list)):
        font_id = valid_font_list[id]
        if font_count_dict.has_key(font_id):
            sum += 1
            sum += font_count_dict[font_id]
            cum_sum.append(font_count_dict[font_id]+1)
        else:
            sum += 1
            cum_sum.append(1)

    for id in range(len(cum_sum)):
        cum_sum[id] = int(sum/cum_sum[id])

    for id in range(1,len(cum_sum)):
        cum_sum[id] += cum_sum[id] + cum_sum[id-1]
    sum = cum_sum[-1]
    del cum_sum[-1]
    cum_sum.insert(0,0) 

    # select the font
    select_id = random.randint(0,sum-1)
    select_font_id = -1
    for id in range(1, len(cum_sum)):
        if select_id < cum_sum[id]:
            select_font_id = valid_font_list[id-1]
            break
    if select_font_id < 0:
        select_font_id = valid_font_list[-1]
    
    # add the font num
    if font_count_dict.has_key(select_font_id):
        font_count_dict[select_font_id] += 1
    else:
        font_count_dict[select_font_id] = 1
        
    # select the font
    font_ttf = font_lines[select_font_id]
    font_ttf = font_ttf.strip()
    font_path_list = font_ttf.split('/')
    font_flag = font_path_list[-2] + "_" + font_path_list[-1].split('.')[0]
    print "====== the font is %s =======" % font_flag

    bg_img = bg_lines[random.randint(0, bg_num-1)]
    print "bg_img:" 
    print bg_img
    bg_img = bg_img.strip()

    cmd = ( "./ocr_tool -i ./vir_cn_%s -o %s -s %d -t %s -m %s -l \"%s\" -e %s -j example.json" 
        %(font_flag, output_dir, font_size, font_ttf, bg_img, input_string.encode('utf8'), num_id) )
    #print cmd
    os.system(cmd)
    num_id += 1
output_fn = open(os.path.join(output_dir, "font_count.list"), 'w')
for k,v in font_count_dict.items():
    output_fn.write('{}\t{}\n'.format(font_lines[k].strip(), v))
output_fn.close()

