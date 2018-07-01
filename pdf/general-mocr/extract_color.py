import base64
import json
import urllib2
import socket
import time
# import util
import random
# from conf import conf
import os, sys
import cv2
import numpy as np
img_file = "img3.jpg"
pre_bg_color = None
pre_line_inpaint = None
with open('json.txt', 'rb') as f:
    result = json.load(f)
# if 'result' in res_json:
    # print res_json['result']
# result = json.loads(res_json['result'])
img = cv2.imread(img_file)
#img_copy = img.copy()
img = resize(img, (1051, 1500))
cnt = 0
for r in result['ret']:
    left = int(r['rect']['left'])
    top = int(r['rect']['top'])
    width = int(r['rect']['width'])
    height = int(r['rect']['height'])
    word = r['word']
    print word

    bottom = top + height
    right = left + width

    top = top - 1
    if top < 0:
        top = 0

    left = left - 2
    if left < 0:
        left = 0

    bottom += 1
    if bottom > img.shape[0]:
        bottom = img.shape[0]

    right += 2
    if right > img.shape[1]:
        right = img.shape[1]
    img_copy = img.copy()
    img_copy = cv2.rectangle(img_copy, (left, top), (right, bottom), (0, 0, 255))

    line = img[top: bottom, left: right].copy()
    line_ori = line.copy()

    # start = time.time()

    # print
    # '****begin**********'
    # start_1 = time.time()
    if height > 30 and width > img.shape[1] / 4:
        line = cv2.resize(line, (int(line.shape[1] * 0.3), int(line.shape[0] * 0.3)))
    # end_1 = time.time()
    # print
    # 'thres 1      ', (end_1 - start_1) * 1000

    # start_1_1 = time.time()
    gray = cv2.cvtColor(line, cv2.COLOR_BGR2GRAY)
    _, thresh = cv2.threshold(gray, 0, 1, cv2.THRESH_BINARY | cv2.THRESH_OTSU)
    # thresh = cv2.ximgproc.niBlackThreshold(gray, 1, cv2.THRESH_BINARY, 3, cv2.ximgproc.BINARIZATION_NIBLACK)
    # end_1_1 = time.time()
    # print
    # 'cvtcolor     ', (end_1_1 - start_1_1) * 1000

    # sum = thresh[0][0] + thresh[0][thresh.shape[1] / 2] + thresh[0][thresh.shape[1] - 1] + \
    #       thresh[thresh.shape[0] - 1][0] + thresh[thresh.shape[0] - 1][thresh.shape[1] / 2] \
    #       + thresh[thresh.shape[0] - 1][thresh.shape[1] - 1]
    sum = thresh[1][1] + thresh[1][thresh.shape[1] / 2] + thresh[1][thresh.shape[1] - 1] + \
          thresh[thresh.shape[0] - 1][1] + thresh[thresh.shape[0] - 1][thresh.shape[1] / 2] \
          + thresh[thresh.shape[0] - 1][thresh.shape[1] - 1]

    flag = True if 6 - sum > sum else False

    # start_2 = time.time()
    if flag == False:
        _, thresh = cv2.threshold(gray, 0, 1, cv2.THRESH_BINARY_INV | cv2.THRESH_OTSU)
        # thresh = cv2.ximgproc.niBlackThreshold(gray, 1, cv2.THRESH_BINARY, 3, cv2.ximgproc.BINARIZATION_NIBLACK)
    # end_2 = time.time()
    # print
    # 'thresh 2     ', (end_2 - start_2) * 1000

    # if height > 35 and width > img.shape[1] / 4:
    #    line = cv2.resize(line, (int(line.shape[1] * 0.2), int(line.shape[0] *  0.2)))
    #    thresh = cv2.resize(thresh, (int(thresh.shape[1] * 0.2), int(thresh.shape[0] *  0.2)))

    mask = thresh
    # mask = np.zeros((thresh.shape[0], thresh.shape[1]), dtype = np.uint8)
    # mask[10: mask.shape[0] - 12, 10: mask.shape[1] - 12 ] = 1

    # start_3 = time.time()
    fg_color = cv2.mean(line, mask)[0:3]
    print 'fg_color:', fg_color
    bg_color = cv2.mean(line, cv2.bitwise_not(mask))
    print 'bg_color:', bg_color
