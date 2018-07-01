import os
import cv2

img_path = "/home/vis/sunwanqi/evaluate/865img"

old_path = "/home/vis/sunwanqi/evaluate/865_gt"
new_path = "/home/vis/sunwanqi/evaluate/865_gt_fix"
new_line = []
for root, dirs, files in os.walk(old_path):
    for file_name in files:
        old_file = os.path.join(old_path, file_name)
        new_file = os.path.join(new_path, file_name)
        with open(new_file, 'w') as fw:
            with open(old_file, 'r') as f:
                print os.path.join(img_path, file_name.split('.', 1)[0]) + '.jpg'
                img = cv2.imread(os.path.join(img_path, file_name.split('.', 1)[0]) + '.jpg')
                if img is None:
                    continue
                else:
                    img_height = img.shape[0]
                    img_width = img.shape[1]
                    new_line.append(file_name.split('.', 1)[0] + '.jpg')
                    new_line.append(str(img_width))
                    new_line.append(str(img_height))
                    write_line = ('\t').join(new_line)
                    fw.write(write_line + '\n')
                    new_line = []
                for line in f:
                    content = line.split()
                    if len(content) is 6:
                        left = content[0]
                        top = content[1]
                        width = content[2]
                        height = content[3]
                        label = content[4]
                        content_str = content[5]
                        new_line.append(left)
                        new_line.append(top)
                        new_line.append(left)
                        new_line.append(str(int(top) + int(height)))
                        new_line.append(str(int(left) + int(width)))
                        new_line.append(str(int(top) + int(height)))
                        new_line.append(str(int(left) + int(width)))
                        new_line.append(top)
                        new_line.append('#')
                        new_line.append('1')
                        new_line.append(content_str)
                        new_line.append(left)
                        new_line.append(top)
                        new_line.append(width)
                        new_line.append(height)
                        new_line.append(content_str)
                        write_line = ('\t').join(new_line)
                    else:
                        left = content[0]
                        top = content[1]
                        width = content[2]
                        height = content[3]
                        label = content[4]

                        new_line.append(left)
                        new_line.append(top)
                        new_line.append(left)
                        new_line.append(str(int(top) + int(height)))
                        new_line.append(str(int(left) + int(width)))
                        new_line.append(str(int(top) + int(height)))
                        new_line.append(str(int(left) + int(width)))
                        new_line.append(top)
                        new_line.append('#')
                        new_line.append('1')
                        write_line1 = ('\t').join(new_line)
                        new_line = []
                        new_line.append(left)
                        new_line.append(top)
                        new_line.append(width)
                        new_line.append(height)
                        # new_line.append(content_str)
                        write_line2 = ('\t').join(new_line)
                        write_line = write_line1 + "\t" + ' ' + "\t" + write_line2 + "\t" + " "
                    print(write_line)
                    fw.write(write_line + '\t\n')
                    new_line = []
            f.close()
        fw.close()

