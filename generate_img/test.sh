#test.py corpus min_font max_font font_list bg_list output_dir
nohup python ./gen_vir_data.py ./process_result2.txt 110 160 ./selected_font_list.txt ./gray_back.list ./punc_train > gen_vir_data.log 2>&1 &


#for((i=1;i<=20000;i++));
#do
#python ./gen_vir_data.py ./new7.txt 40 70 ./font/font_1_list.txt ./0809_5.3w_bg_without_words.list ./output_train
#done
