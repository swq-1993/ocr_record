# -*- coding: utf-8 -*-
################################################################################
#
# Copyright (c) 2016 Baidu.com, Inc. All Rights Reserved
#
################################################################################
"""
This module provide test cases for pornfilter service

Authors: lichao32(lichao32@baidu.com)
Date:    2016/05/14 11:55:03

message GeneralClassifySvcRequest {
    optional uint64 logid = 1;
    optional string clientip = 2;
    optional string appid = 3;
    optional string from = 4;
    optional string format = 5;
    optional string cmdid = 6;
    optional bytes data = 7;
};

// result: binary data and encode with base64
//      type:
//          general_classify_client_pb2.GeneralClassifyResponse()
//      parse:
//          1. base64.decodestring()
//          2. string to protobuf
//          3. protobuf to json
message GeneralClassifySvcResponse {
    optional int32 err_no = 1;
    optional string err_msg = 2;
    optional bytes result = 3;
    optional string result_type = 4;
};

service GeneralClassifyService {
    rpc classify(GeneralClassifyRequest) returns (GeneralClassifySvcResponse);
};
"""

import base64
import json
import urllib2
import socket
import time
import util
import random
from conf import conf
import cv2
import os, sys
#from draw import draw_line_result_and_save
from PIL import Image

def img_resize(img, max_size):
    """resize image and ensure the max size is max_size
    Args:
        img: input image
        max_size: the max size of the return image

    Returns:
        img: the resized image
    """
    height, width = img.shape[:2]
    print ("height = %d, width = %d, channel = %d" %(img.shape[:3]))
    if (height > max_size or width > max_size):
        scale = float(max_size) / max(height, width)
        img = cv2.resize(img, None, fx=scale, fy=scale, interpolation = cv2.INTER_CUBIC)
        print ("height = %d, width = %d, channel = %d" %(img.shape[:3]))

    return img


def prepare_img_data(conf):
    """ prepare img_data for request

    Args:
        conf: conf file

    Returns:
        img_data: data read from file
    """

    img_file = None
    data_path = None
    img_files = None

    if 'data_file' in conf:
        img_file = conf['data_file']

    if 'data_path' in conf:
        data_path = conf['data_path']
        img_files = util.get_all_file_names(data_path)

    if img_files is not None and len(img_files) > 0:
        img_file = img_files[random.randint(0, len(img_files) - 1)]
    
    #temp_img_file = "./temp/temp_image_" + str(random.randint(0, 10000000)) + ".jpg"
    #img = cv2.imread(img_file, 1)
    #img = img_resize(img, 500)
    #cv2.imwrite(temp_img_file, img)
    #img_data = util.read_file(temp_img_file, 'rb')
    img_data = util.read_file(img_file, 'rb')

    return img_data

def prepare_data_single(img_file):
    data = ""
    #request_str = "type=st_ocrapi_all&locate_type=v2&appid=10005&clientip=x.x.x.x&fromdevice=pc&version=1.0.0&detecttype=LocateRecognize&encoding=1&recg_type=seq&international=1&languagetype=CHN_ENG&is_formulate_output_as_chn=true&eng_granularity=word" #&locate_type=v2"
    #request_str = "type=st_ocrapi&locate_type=v2&appid=10005&clientip=x.x.x.x&fromdevice=pc&version=1.0.0&detecttype=LocateRecognize&encoding=1&recg_type=seq&international=1&languagetype=CHN_ENG&object_type=pdf" #&locate_type=v2"
    request_str = "type=st_ocrapi_all&locate_type=v2&appid=10005&clientip=x.x.x.x&fromdevice=pc&version=1.0.0&detecttype=LocateRecognize&encoding=1&recg_type=seq&international=1&languagetype=CHN_ENG&object_type=pdf" #&locate_type=v2"
    #request_str = "object_type=pdf&detecttype=LocateRecognize&imgDirection=setImgDirFlag" #&locate_type=v2"
    #request_str = "type=st_ocrapi_all&locate_type=v2&appid=10005&clientip=x.x.x.x&fromdevice=pc&version=1.0.0&detecttype=Recognize&encoding=1&recg_type=seq&international=1&languagetype=CHN_ENG" #&is_formulate_output_as_chn=true"   #&eng_granularity=word" #&locate_type=v2"
    img_data = util.read_file(img_file, 'rb')
    data = request_str + "&image=" + base64.b64encode(img_data)
    return data
    

def prepare_request_single(img_file):

    logid = (1111111, 1111111);#random.randint(1000000, 100000000)
    data = prepare_data_single(img_file)
    req_array = {
                    'appid': '123456',
                    'logid': logid,
                    'format': 'json',
                    'from': 'test-python',
                    'cmdid': '123',
                    'clientip': '0.0.0.0',
                    'data': base64.b64encode(data),
                }
    req_json = json.dumps(req_array)

    return req_json


def choose_server(addrs):
    """ choose a addr for rpc call, for the purpose of load-balancing

    Args:
        addrs: single or list

    Returns:
        addrs: single addr for rpc call
    """

    if type(addrs) == type([]):
        return addrs[random.randint(0, len(addrs) - 1)]
    else:
        return addrs


def prepare_serve_addr(conf):
    """ prepare serve addr for rpc call

    Args:
        conf: conf file

    Returns:
        server_addr: server addr for rpc call
    """

    addrs = conf['addrs']
    addr = choose_server(addrs)
    service = conf['service']
    method = conf['method']

    server_addr = "http://" + addr + "/" \
                + service + "/" + method

    return server_addr

def parse_ocr_ret_all(ocr_ret):

    line_rects  = []
    ocr_results = []
    char_results = []
    
    ocr_decode_ret = json.loads(ocr_ret)
    #print ocr_decode_ret["ret"]
    for ocr_line_item in ocr_decode_ret['ret']:
        if not ocr_line_item.has_key('word') or not ocr_line_item.has_key('rect'):
            continue
        ocr_result = ocr_line_item['word']
        ocr_results.append(ocr_result.encode('utf8'))

        line_rect  = ocr_line_item['rect']
        left   = int(line_rect['left'])
        top    = int(line_rect['top'])
        width  = int(line_rect['width'])
        height = int(line_rect['height'])
        line_rects.append((left, top, width, height))
    
        char_rect = ocr_line_item['charset']
        cur_char_line = []
        for item in char_rect:
            cur_char_rect = item['rect']
            cur_char_result = []
            cur_char_result.append(int(cur_char_rect['left']))
            cur_char_result.append(int(cur_char_rect['top']))
            cur_char_result.append(int(cur_char_rect['width']))
            cur_char_result.append(int(cur_char_rect['height']))
            cur_char_result.append(item['word'])
            cur_char_line.append(cur_char_result)
        char_results.append(cur_char_line)
    return (line_rects, ocr_results, char_results)

def save_ocr_ret_all(line_rects, ocr_results, char_results, save_path):
    file_name = save_path.split('/')[-1]
    file_save = open(save_path, "w")
    head_str = file_name + '\t100\t100\t\n'
    file_save.write(head_str)
    line_num = len(line_rects)
    for line_idx in range(line_num):
        line_pos = line_rects[line_idx]
        line_info = ocr_results[line_idx]
        save_row = str(line_pos[0]) + "\t" + str(line_pos[1]) + "\t" + str(line_pos[2]) + "\t" + \
               str(line_pos[3]) + "\t" + line_info + "\t"
        #print char_results[line_idx]
        for char_idx in range(len(char_results[line_idx])):
            #print char_results[line_idx][char_idx]
            save_row += str(char_results[line_idx][char_idx][0]) + "\t"
            save_row += str(char_results[line_idx][char_idx][1]) + "\t"
            save_row += str(char_results[line_idx][char_idx][2]) + "\t"
            save_row += str(char_results[line_idx][char_idx][3]) + "\t"
            save_row += char_results[line_idx][char_idx][4].encode('utf8') + "\t"
        save_row += "\n"
        file_save.write(save_row)
    file_save.close()
    return

def GeneralClassifySvc(str_url, req_json):
    """ do http request on a baidu-rpc service

    Args:
        str_url: which service to access
        req_json: an json object which contains image info

    Returns:
        res_json: response object in json format
    """

    req = urllib2.Request(str_url)
    req.add_header('Content-Type', 'application/json')
    try:
        response = urllib2.urlopen(req, req_json, 1000).read()
    except urllib2.HTTPError as e:
        print e

    res_json = json.loads(response)
    print res_json
    if res_json['err_no'] == 0:
        res_json['result'] = base64.decodestring(res_json['result'])
        line_rects, ocr_results, char_results = parse_ocr_ret_all(res_json['result'])   

    #return res_json
    return res_json, line_rects, ocr_results, char_results


def process(model):
    """ do a rpc calling to server

    Args:
        model: service type, for example:
            "antiporn_service"

    Returns:
        res_json: response info in json
    """

    req_json = prepare_request(conf[model])
    str_url = prepare_serve_addr(conf[model])
    print str_url
    res_json = GeneralClassifySvc(str_url, req_json)
    return req_json, res_json

def process_all(model, prefix, out_dir):

    data_path = conf[model]['data_path']
    img_files = util.get_all_file_names(data_path)

    img_num = len(img_files)
    for i in range(img_num):
        img_file = img_files[i]
        req_json = prepare_request_single(img_file)
        str_url = prepare_serve_addr(conf[model])
        print str_url, img_file
        #try:    
        res_json, line_rects, ocr_results, char_results = GeneralClassifySvc(str_url, req_json)
        name = img_file.split('/')[-1]
        add_pos = name.rfind('.')
        new_name = name[0:add_pos] + prefix + name[add_pos:] + ".txt"
        new_path = os.path.join(out_dir, new_name)
        save_ocr_ret_all(line_rects, ocr_results, char_results, new_path)
        print res_json['result']

        #pimg = Image.open(img_file)
        #new_name = name[0:add_pos] + prefix + name[add_pos:]
        #output_name = os.path.join(out_dir, new_name)
        #draw_line_result_and_save(pimg, line_rects, ocr_results, output_name)



if __name__ == '__main__':
    if len(sys.argv) != 4:
        print "test.py img_dir prefix out_dir"
        exit(-1)
    img_dir = sys.argv[1]
    prefix = sys.argv[2]
    out_dir = sys.argv[3]
    model = conf['perf_conf']['task_module']
    conf[model]['data_path'] = img_dir
    start = time.time()
    #req_json, res_json = process(model)
    if 1:
        process_all(model, prefix, out_dir)
    cost = int(1000 * (time.time() - start))

    print "[cost:%sms]" % (cost)
    #print res_json
    #print res_json['result']

