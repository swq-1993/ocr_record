#!/usr/bin/env python
# -*- coding:utf-8 -*-
# Author:XXX
import sys,os
import string
#def ReadTable(table_path):
#     tfn = open(table_path)
#     cls_dic  = {}
#     while 1:
#        line = tfn.readline()
#        if not line:
#           break
#        line = line.strip('\n')
#        print line
#        #[unic, cls] = line.split('   ')
#        list = line.split('\t')
#        print list
#        unic = list[0]
#        cls = list[1]
#        print unic,cls
#        unic = unic.strip()
#        cls_dic[unic] = string.atoi(cls)
#     tfn.close()
#     return cls_dic

def ReadTable(table_path):
    tfn = open(table_path)
    cls_dic  = {}
    while 1:
        line = tfn.readline()
        if not line:
            break
        line = line.strip('\n')
        [unic, cls] = line.split()
        #print unic, cls
        unic = unic.strip()
        unic = int(unic)
        cls_dic[unic] = int(cls)
    tfn.close()
    return cls_dic

def group_input(img_list_path):
    width=[]
    height=[]
    img_path=[]
    label_name=[]
    tfn=open(img_list_path)
    while 1:
        line=tfn.readline()
        if not line :
            break
       # print line
        re=line.split()
       # print re
        if int( re[0])>1000 or int(re[0])<50 :
            continue
        width.append(re[0])
        height.append(re[1])
        img_path.append(re[2])
       # print(re[3:])
        length=len(re)
        label_for_record=" ".join(re[3:length-1])
        #print label_for_record
        label_name.append(label_for_record)
        
    return width,height,img_path,label_name


def B2Q(uchar):
     inside_code=int(hex(ord(uchar)),16)
     #print '***',inside_code
     if inside_code<0x20 or inside_code>0x7e:
        #print '&&&',inside_code
        return inside_code
     if inside_code==0x0020:
        inside_code=0x3000
     else:
        inside_code+=0xfee0
        #print '!!!',inside_code
     return inside_code





def trans_decode(label_name,cls_dic):
        #print cls_dic
       # print len(cls_dic)
        labelVector = []
        NotFind = 0
        label_vec_txt = label_name
       # print ("**"+label_vec_txt )
        label_vec_txt_decode = label_vec_txt.decode('utf8')
        label_vec_len = len(label_vec_txt_decode)
       
        str_info = ""
        tmp_code = label_vec_txt_decode
        decode_len = len(tmp_code)
        for idx in range(decode_len):
          # print "tmp_code:",tmp_code[idx]
           code = B2Q(tmp_code[idx])
           #print "code:",code
           if long(code)==12288:
               code=127
           #if long(code) > 65248:
            #   code=str(long(code)-65248)
        #   print ("code"+code)
           if not (cls_dic.has_key(code)):
              NotFind = 1
              print "not find",code
             # print code.encode("utf-8")
              continue
           labelVector.append(cls_dic[code])
        return labelVector,NotFind


if __name__ == '__main__':
    if(len(sys.argv)!=4):
       print "./main imgdir saveDir out_txt_name/"
       sys.exit(0)
 
    imgdir   = sys.argv[1]
    savedir  = sys.argv[2]
    txt_name = sys.argv[3]
    cls_dic = ReadTable("./table_eng_97")
    file_list_path = os.path.join(savedir,txt_name)
    save_file = open(file_list_path, "wt")
    tfn=open(imgdir)
    while 1:
        line=tfn.readline()
        if not line :
            break
       # print line
        re=line.split()
        print "re:",re
        if int( re[0])>1000 or int(re[0])<50 :
            continue
        width=(re[0])
        height=(re[1])
        img_path=(re[2])
       # print(re[3:])
        length=len(re)
        label_for_record=" ".join(re[3:length-1])

        print "label_for_record:",label_for_record
        #print label_for_record
        #label_name.append(label_for_record)
   # width, height, img_path, label_name=group_input(imgdir)

    #    for i in range(length):
        labelVector,NOTFind=trans_decode(label_for_record,cls_dic)
        if NOTFind==1:
        #    print "notfind_"
            continue
        line_info=width+" "+height+" "+img_path+" "
        lab_len = len(labelVector)
        label_info=""
        #print 'labelVector',labelVector
        for j in range(lab_len):
            label_id = labelVector[j]
            if j < lab_len - 1:
                label_info += str(label_id) + ","
            else:
                label_info += str(label_id)
        line_info += label_info + "\n"
       # print line_info
        save_file.write(line_info)
    save_file.close()
