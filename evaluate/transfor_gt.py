import os

#old_file = "en_zh1516683453978.txt"
#new_file = old_file.split('.')[0] + '.jpg.txt'
old_path = "/home/vis/sunwanqi/evaluate/gt_char"
new_path = "/home/vis/sunwanqi/evaluate/gt_char_fix"
# print(new_file)
count = 1
new_line = []
for root, dirs, files in os.walk(old_path):
    for file_name in files:
        count = 1
        old_file = os.path.join(old_path, file_name)
        new_file = os.path.join(new_path, file_name.split('.')[0] + '.jpg.txt')
        with open(new_file, 'w') as fw:
            with open(old_file, 'r') as f:
                print old_file
                for line in f:
                    if count is 1:
                        first_line = line.split()
                        img_width = first_line[1]
                        img_height = first_line[2]
                        new_line.append(file_name.split('.')[0] + '.jpg')
                        #new_line.append(file_name)
                        new_line.append(img_width)
                        new_line.append(img_height)
                        write_line = ("\t").join(new_line)
                        count += 1
                        fw.write(write_line + '\n')
                        
                    else:
                        content = line.split()
                        if len(content) is 6:
                            print len(content)
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
                            #if int(label) is 2:
                            #    new_line.append('4')
                            #else:
                            #    new_line.append(label)
                            new_line.append('1')
                            new_line.append(content_str)
                            new_line.append(left)
                            new_line.append(top)
                            new_line.append(width)
                            new_line.append(height)
                            new_line.append(content_str)
                            write_line = ("\t").join(new_line)
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
                            #if int(label) is 2:
                            #    new_line.append('4')
                            #else:
                            #    new_line.append(label)
                            new_line.append('1')
                            write_line1 = ("\t").join(new_line)
                            new_line = []
                            new_line.append(left)
                            new_line.append(top)
                            new_line.append(width)
                            new_line.append(height)
                            #new_line.append(content_str)
                            write_line2 = ("\t").join(new_line)
                            write_line = write_line1 + "\t" +  " " + "\t" + write_line2 + "\t" + " "
                        count += 1
                        print(write_line)
                        fw.write(write_line + '\t\n')
                    new_line = []
                print(count)
           # f.close()
        #fw.close()

