#include "dll_main.h"
#include <stdexcept>
#include <stdlib.h>
#include <unistd.h>

// the adjustable parameter from a initial file
IniFile *g_ini_file = NULL;
// the parameter file name
const char* ini_file_name = "seqocr.ini";

int init_para_file() {
    
    char dll_file_path[PATH_MAX + 1];         // the path of own dll directory
    char ini_file_path[PATH_MAX + 1];         // the path of initial file
    getcwd(dll_file_path, PATH_MAX);
    snprintf(ini_file_path, PATH_MAX + 1, "%s/%s", dll_file_path, ini_file_name);
        
    if (g_ini_file) {
        delete g_ini_file;
        g_ini_file = NULL;
    }
    g_ini_file = new IniFile();
    int ret_value = 0;
    ret_value = g_ini_file->initialize(ini_file_path);

    return ret_value;
}
int free_para_file() {
    if (g_ini_file) {
        delete g_ini_file;
        g_ini_file = NULL;
    }
    return 0;
}

