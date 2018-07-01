#include "ini_file.h"
#include <stdexcept>
#include <fstream>

int IniFile::init_ini_file() {
    _m_ini_info_vec.clear();
    IniInfo info1("globals", "MUL_ROW_MODE", TYPE_INT, "0", "2", "1");
    _m_ini_info_vec.push_back(info1);
     
    IniInfo info2("globals", "OVERLAP_MODE", TYPE_INT, "0", "1", "1"); 
    _m_ini_info_vec.push_back(info2);
    // 0: use chn and English OCR; 1: use the chn OCR only; 2: use the eng ocr only
    IniInfo info3("globals", "CHN_ENG_MODEL", TYPE_INT, "0", "2", "0"); 
    _m_ini_info_vec.push_back(info3);
    // the line format of English line
    // 0: the original English format; 1: the Chinese format
    IniInfo info4("globals", "EN_LINE_FORMAT", TYPE_INT, "0", "1", "1");
    _m_ini_info_vec.push_back(info4);

    // parameter for the line word segmentation
    // -1 denotes the whole row will be recognized
    IniInfo info5("globals", "SEG_ROW_NB_CHAR_SIZE", TYPE_INT, "-1", "6", "4");  
    _m_ini_info_vec.push_back(info5);
    
    // number of candidate words
    IniInfo info6("globals", "OCR_RECOG_CAND_NUM", TYPE_INT, "1", "10", "5");  
    _m_ini_info_vec.push_back(info6);
    
    // extend asia word ratio
    IniInfo info6_1("globals", "CHN_RESIZE_W_RATIO", TYPE_DOUBLE, "0.5", "1.5", "1.05");  
    _m_ini_info_vec.push_back(info6_1);
    
    // line index for debug
    // -1 denotes that all the rows will be processed
    IniInfo info7("globals", "DEBUG_ROW_INDEX", TYPE_INT, "-1", "100", "-1"); 
    _m_ini_info_vec.push_back(info7);

    // setting the weight for the decoding function
    IniInfo info8("globals", "DECODE_LANG_MODEL_W", TYPE_DOUBLE, "0", "10", "0.6");  
    _m_ini_info_vec.push_back(info8);
    IniInfo info9("globals", "DECODE_LANG_MODEL0_W", TYPE_DOUBLE, "0", "10", "0.0");  
    _m_ini_info_vec.push_back(info9);
    IniInfo info10("globals", "DECODE_REG_MODEL_W", TYPE_DOUBLE, "0", "10", "1.0");  
    _m_ini_info_vec.push_back(info10);
    IniInfo info11("globals", "DECODE_OCCUPY_MODEL_W", TYPE_DOUBLE, "0", "10", "1.0");  
    _m_ini_info_vec.push_back(info11);
    IniInfo info12("globals", "DECODE_SPACE_MODEL_W", TYPE_DOUBLE, "0", "10", "0.5");  
    _m_ini_info_vec.push_back(info12);
    IniInfo info13("globals", "DECODE_LANG_CATEGORY_W", TYPE_DOUBLE, "0", "10", "0.8");  
    _m_ini_info_vec.push_back(info13);
    IniInfo info14("globals", "DECODE_CH_WIDTH_W", TYPE_DOUBLE, "0", "10", "0.5");  
    _m_ini_info_vec.push_back(info14);
    IniInfo info15("globals", "DECODE_CH_OCCUPY_MODEL_W", TYPE_DOUBLE, "0", "10", "0.5");  
    _m_ini_info_vec.push_back(info15);

    // the flag for show the result
    IniInfo info16("globals", "DRAW_LINE_DECT_RST", TYPE_INT, "0", "1", "0");  
    _m_ini_info_vec.push_back(info16);
    IniInfo info17("globals", "DRAW_LINE_SEG_RST", TYPE_INT, "0", "1", "0");  
    _m_ini_info_vec.push_back(info17);
    IniInfo info18("globals", "DRAW_LINE_CC_RST", TYPE_INT, "0", "1", "0");  
    _m_ini_info_vec.push_back(info18);
    IniInfo info19("globals", "DRAW_ALL_CC_RST", TYPE_INT, "0", "1", "0");  
    _m_ini_info_vec.push_back(info19);
    IniInfo info20("globals", "DRAW_RECOG_RST", TYPE_INT, "0", "1", "0"); 
    _m_ini_info_vec.push_back(info20);
    IniInfo info21("globals", "DRAW_TIME_LINE_FLAG", TYPE_INT, "0", "1", "0"); 
    _m_ini_info_vec.push_back(info21);
    IniInfo info22("globals", "DRAW_LANG_SCORE_FLAG", TYPE_INT, "0", "1", "0"); 
    _m_ini_info_vec.push_back(info22);
    IniInfo info22_1("globals", "DRAW_RCNN_DECT_RST", TYPE_INT, "0", "1", "0"); 
    _m_ini_info_vec.push_back(info22_1);
    
    // the flag for draw the rotation result of the column image
    IniInfo info22_2("globals", "DRAW_COL_ROTATE_FLAG", TYPE_INT, "0", "1", "0"); 
    _m_ini_info_vec.push_back(info22_2);
    
    IniInfo info23("globals", "DRAW_MUL_ROW_CC_SINGLE_FLAG", TYPE_INT, "0", "1", "0"); 
    _m_ini_info_vec.push_back(info23);
    IniInfo info24("globals", "DRAW_MUL_ROW_CC_MERGE_FLAG", TYPE_INT, "0", "1", "0");
    _m_ini_info_vec.push_back(info24);
    IniInfo info25("globals", "DRAW_MUL_ROW_CC_LINK_FLAG", TYPE_INT, "0", "1", "0");
    _m_ini_info_vec.push_back(info25);
    IniInfo info26("globals", "SAVE_DECODE_BEFORE_RST_FLAG", TYPE_INT, "0", "1", "0");
    _m_ini_info_vec.push_back(info26);

    IniInfo info27("globals", "DRAW_DATA_FOR_RECOG", TYPE_INT, "0", "1", "0");  
    _m_ini_info_vec.push_back(info27);
    IniInfo info28("globals", "DRAW_DECODE_RST", TYPE_INT, "0", "1", "0");  
    _m_ini_info_vec.push_back(info28);
    
    IniInfo info29("globals", "SHOW_RECOG_RST", TYPE_INT, "0", "1", "0");  
    _m_ini_info_vec.push_back(info29);
    // 0: the large model
    // 1: the small fast model
    IniInfo info30("globals", "OCR_MODEL_MODE", TYPE_INT, "0", "1", "1");  
    _m_ini_info_vec.push_back(info30);
    
    // 0: the original decode funtion including language model, space, occupy score
    // 1: the recognition score
    IniInfo info31("globals", "DECODE_LINE_MODE", TYPE_INT, "0", "1", "1");  
    _m_ini_info_vec.push_back(info31);
    
    // 0: gbk
    // 1: utf8
    IniInfo info32("globals", "SAVE_RST_MODE", TYPE_INT, "0", "1", "0");  
    _m_ini_info_vec.push_back(info32);
    
    // 0: load detection result
    // 1: generate the detection result according to the input image
    // 2: generate the column detection result randomly 
    IniInfo info33("globals", "IMG_DECT_MODE", TYPE_INT, "0", "2", "0");  
    _m_ini_info_vec.push_back(info33);
    
    // 0: both row and columns are recognized
    // 1: only row is recognized
    // 2: only column is recognized
    IniInfo info34("globals", "ROW_COL_RECOG_MODE", TYPE_INT, "0", "2", "0");  
    _m_ini_info_vec.push_back(info34);
    
    // 0: Chinese 
    // 1: Japanese
    // 2: Korean
    IniInfo info35("globals", "RECOG_LANG", TYPE_INT, "0", "2", "0");  
    _m_ini_info_vec.push_back(info35);
    
    // 0: do not recognize character if its probability is low
    // 1: recognize character if its probability is low
    IniInfo info36("globals", "LOW_PROB_RECOG_FLAG", TYPE_INT, "0", "1", "0");  
    _m_ini_info_vec.push_back(info36);
    IniInfo info37("globals", "GPU_BATCH_SIZE", TYPE_INT, "1", "100", "4");  
    _m_ini_info_vec.push_back(info37);

    IniInfo info38("globals", "USE_GPU_FLAG", TYPE_INT, "0", "1", "1");  
    _m_ini_info_vec.push_back(info38);

    IniInfo info39("globals", "PREDICT_LOG_FLAG", TYPE_INT, "0", "4", "4");  
    _m_ini_info_vec.push_back(info39);

    return 0;
}
    
std::string IniFile::_s_ini_section_global = "globals";

// Constructor
IniFile::IniFile() {
    _s_section_default = "globals"; 
    _s_key_separator = "@";
    _m_ini_info_vec.clear();
}

int IniFile::initialize(const char *filename) {
    // copy a initial file path to class variable
    snprintf(_m_path, PATH_MAX + 1, "%s", filename);
    int ret_value = 0;
    ret_value = init_ini_file();
    if (ret_value != 0) {
        return ret_value;
    }
    
    int ini_len = _m_ini_info_vec.size(); 
    for (int i = 0; i < ini_len; i++) {
        switch (_m_ini_info_vec[i].type) {
            case TYPE_INT:
    {                   
                // a case of int
                // convert string to int
                int lower_from_infos = 0;
                int upper_from_infos = 0;
                int default_from_infos = 0;
                std::istringstream ss_lower(_m_ini_info_vec[i].lower_value, std::ios_base::in);
                std::istringstream ss_upper(_m_ini_info_vec[i].upper_value, std::ios_base::in);
                std::istringstream ss_default(_m_ini_info_vec[i].default_value, std::ios_base::in);
                ss_lower >> lower_from_infos;
                ss_upper >> upper_from_infos;
                ss_default >> default_from_infos;

                // read the information from the file
                int val = get_value_from_ini_file(_m_ini_info_vec[i].section,\
                    _m_ini_info_vec[i].key, default_from_infos);

                // check default value with lower and upper limit
                if (val < lower_from_infos || val > upper_from_infos) {
                    // replace invalid value to default value
                    val = default_from_infos;
                }

                std::ostringstream os;
                os << _m_ini_info_vec[i].key << _s_key_separator.data() \
                   << _m_ini_info_vec[i].section;
                std::map<std::string, int>::iterator itr = _m_int_table.find(os.str());

                // duplicate key exists in the table
                if (itr != _m_int_table.end()) {
                    ret_value = -1;
                    break;
                }

                _m_int_table.insert(std::pair<std::string, int>(os.str(), val));
                break;
            }
            case TYPE_DOUBLE:
            {    
                // a case of double
                double lower_from_infos = 0;
                double upper_from_infos = 0;
                double default_from_infos = 0;
                std::istringstream ss_lower(_m_ini_info_vec[i].lower_value, std::ios_base::in);
                std::istringstream ss_upper(_m_ini_info_vec[i].upper_value, std::ios_base::in);
                std::istringstream ss_default(_m_ini_info_vec[i].default_value, std::ios_base::in);
                ss_lower >> lower_from_infos;
                ss_upper >> upper_from_infos;
                ss_default >> default_from_infos;
                double val = get_value_from_ini_file(_m_ini_info_vec[i].section,\
                   _m_ini_info_vec[i].key, default_from_infos);

                // check default value with lower and upper limit
                if (val < lower_from_infos || val > upper_from_infos) {
                    // replace invalid value to default value
                    val = default_from_infos;
                }

                std::ostringstream os;
                os << _m_ini_info_vec[i].key << _s_key_separator << _m_ini_info_vec[i].section;
                std::map<std::string, double>::iterator itr = \
                    _m_double_table.find(_m_ini_info_vec[i].key);

                // duplicate key exists in the table
                if (itr != _m_double_table.end()) {
                    ret_value = -1;
                    break;
                }

                _m_double_table.insert(std::pair<std::string, double>(os.str(), val));
                break;
            }
        }
    }

    return ret_value;
}
int IniFile::get_profile_string(const std::string section, const std::string key, char *buf,\
        int ibuf_len, const char *file_path)  {
        int ret = 0;
        
        std::ifstream in_file(file_path);
        if (!in_file.is_open()) {
            return -1;
        }

        std::string str_line;
        int ifind_section = 0;
        int ifind_key = 0;
        // find the section name
        while (getline(in_file, str_line)) {
            int i_pos = str_line.find(section);
            if (i_pos >= 0) {
                ifind_section = 1;
                break;
            }
        }
        if (ifind_section == 1) {
            // find the key string
            while (getline(in_file, str_line)) {
                std::string str_key = "";
                std::string str_value = "";
                int i_pos = str_line.find("=");
                if (i_pos <= 0) {
                    continue ;
                }
                int i_length = str_line.length();
                str_key = str_line.substr(0, i_pos-1);
                str_value = str_line.substr(i_pos+1, i_length-i_pos);
                
                if (str_key == std::string(key)) {
                    snprintf(buf, ibuf_len, "%s", str_value.data());
                    ifind_key = 1;
                    break;
                }
        }
        if (ifind_key == 0) {
            ret = -1;
        }
    }
    else  {
        ret = -1;
    }
    in_file.close();
    
    return ret;
}

int IniFile::get_int_value(const char *key, const std::string section) {

    int ret = 0;
    std::ostringstream os;
    os << key << _s_key_separator.data() << section;
    //std::cout << os.str() << std::endl;

    std::map<std::string, int>::iterator itr = _m_int_table.find(os.str());
    if (itr == _m_int_table.end()) {
        printf("cannot locate the key!\n");
    } else {
        ret = itr->second;
    }

    return ret;
}
    
double IniFile::get_double_value(const char *key, const std::string section) {

    double ret = 0;
    std::ostringstream os;
    os << key << _s_key_separator << section;
    std::map<std::string, double>::iterator itr = _m_double_table.find(os.str());

    if (itr == _m_double_table.end()) {
        printf("cannot locate the key!\n");
    } else {
        ret = itr->second;
    }

    return ret;
}
