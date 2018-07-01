/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file ini_file.h
 * @author xieshufu(com@baidu.com)
 * @date 2015/03/18 14:00:57
 * @brief 
 *  
 **/
#ifndef  APP_SEARCH_VIS_OCR_ALG_SEQLEARNINGDECODE_INI_FILE_H
#define  APP_SEARCH_VIS_OCR_ALG_SEQLEARNINGDECODE_INI_FILE_H

#include <map>
#include <string>
#include <sstream>
#include <limits>
#include <stdio.h>
#include <iostream>
#include <vector>

#ifndef PATH_MAX
#define PATH_MAX (512)
#endif

typedef enum {
    TYPE_INT,
    TYPE_DOUBLE
} DefaultIniType;

struct IniInfo {
    std::string section;
    std::string key;
    DefaultIniType type;
    std::string lower_value;
    std::string upper_value;
    std::string default_value;

    IniInfo(std::string _section_, std::string _key_, DefaultIniType _type_, \
       std::string _lower_value_, std::string _upper_value_,\
       std::string _default_value_)
    {
        section = _section_;
        key = _key_;
        type = _type_;
        lower_value = _lower_value_;
        upper_value = _upper_value_;
        default_value = _default_value_;
    }
}; 

//-------------------------------------------------------------------------------------------------
class IniFile {
public:
    IniFile();         // Constructor.

    int initialize(const char *filename);

    virtual ~IniFile() {           // Destructor
    }

    int get_int_value(const char *key, const std::string section); 
    double get_double_value(const char *key, const std::string section); 

    static std::string _s_ini_section_global;
private:
    char _m_path[PATH_MAX + 1];       
    // format: "KEY@SECTION", default value
    std::map<std::string, int> _m_int_table;                    // int value table with map
    std::map<std::string, double> _m_double_table;              // double value table with map

    pthread_mutex_t _m_int_section;
    pthread_mutex_t _m_double_section;

    static const int MAX_INI_VALUE_LEN = 256;
    std::string _s_section_default;
    std::string _s_key_separator;

    std::vector<IniInfo> _m_ini_info_vec;
    int init_ini_file();

    int get_profile_string(const std::string section, const std::string key,\
        char *buf, int ibuf_len, const char *file_path);
    
    template <typename T>
    T get_value_from_ini_file(const std::string section, const std::string key, T default_value) {
        char buf[MAX_INI_VALUE_LEN];

        int iread_key = 0;
        iread_key = get_profile_string(section, key, buf, sizeof(buf)-1, _m_path);
        if (iread_key!=0)  {
            // can not read the key value
            return default_value;
        }
        else  {
            T rt;
            std::istringstream ss(buf, std::ios_base::in);
            ss >> rt;
            
            if (ss.bad() || ss.fail()) {
                return default_value;
            }
            else {
                return rt;
            }
        }
    }
};

#endif
