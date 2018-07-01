import os
import cv2

#img_list_path = "./punc_output/result_merge_shuf_fix.txt"
#img_list_path = "./lss_eng_fluid_233w.list"
img_list_path= "./eng_add_punc_train_fix.list"
count = 0
with open("miss_path.txt", 'w') as f:
    list_file = open(img_list_path, "r")
    img_list = list_file.readlines()
    list_file.close()
    for line in img_list:
        line = line.strip()
        line_info = line.split(" ")
        if len(line_info) != 4:
            print line_info[2]
            f.write(line_info[2] + '\n')
        img_path = line_info[2]
        img = cv2.imread(img_path)
        if img is None:
            f.write(img_path+'\n')
        else:
            count += 1
        line_label = line_info[3].split(',')
        if len(line_label) == 0:
            print img_path
            f.write(img_path + '\n')
        for i in range(len(line_label)):
            if int(line_label[i]) < 0 or int(line_label[i]) > 96:
                print img_path
                f.write(img_path + '\n')
    print count
    f.close()
