/***************************************************************************
 * 
 * Copyright (c) 2016 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file src/ocr_api_imp.cpp
 * @author xieshufu(com@baidu.com)
 * @date 2016/06/01 16:15:55
 * @brief 
 *  
 **/
#include "ocr_api_imp.h"
#include "appocr.h"
#include <iostream>
#include "parse_value.h"
#include "query_data.h"
#include "uconv.h"
#include "time_elapsed.h"
#include "thread.h"
#include "google/malloc_extension.h"
#include "ocr_func.h"
#include "repair_stdout.h"
#include "disk_info.h"
#include "md5.h"
#include "mac.h"
#include "cpuid.h"
#include "time_utility.h"
#include "encrypt.h"
#include "device_info.h"

extern appocr_conf g_conf;

namespace idlapi 
{

static const std::string g_redir_stdout = "general-mocr-redir.log";

#define FATAL(fmt, arg...) \
do { \
    com_writelog(COMLOG_FATAL, fmt, ##arg); \
} while (0)

#define WARNING(fmt, arg...) \
do { \
    com_writelog(COMLOG_WARNING, fmt, ##arg); \
} while (0)

#define NOTICE(fmt, arg...) \
do { \
    com_writelog(COMLOG_NOTICE, fmt, ##arg); \
} while (0)

#define TRACE(fmt, arg...) \
do { \
    com_writelog(COMLOG_TRACE, fmt, ##arg); \
} while (0)

#define DEBUG(fmt, arg...) \
do { \
    com_writelog(COMLOG_DEBUG, fmt, ##arg); \
} while (0)
    
OcrApiImp::OcrApiImp()
{
    TRACE("OcrApiImp::OcrApiImp()");
    int ret = _databuf.init(g_conf);
    if (ret)
    {
        FATAL_LOG("databuf init error!");
    }

    assert(ret == 0);
}

OcrApiImp::~OcrApiImp () 
{
    TRACE("OcrApiImp::~OcrApiImp()");
}

static void* tcmalloc_release(void* thread_arg)
{
    while (!_exit)
    {
        sleep(10);
        char buf[2048];
        ///*
        MallocExtension::instance()->GetStats(buf, sizeof(buf) - 1);
        //DEBUG_LOG("memory before:%s\n", buf);
        MallocExtension::instance()->ReleaseFreeMemory();
        MallocExtension::instance()->GetStats(buf, sizeof(buf) - 1);
        //DEBUG_LOG("memory end:%s\n\n", buf);
        //*/
    }

    return NULL;
}

static std::string convert_result(appocr_thread_data* databuf)
{
    std::string src = std::string(databuf->m_busOcr2fcgi.m_pszTextData);
    if (nsmocr::CQueryData::is_utf8(databuf))
    {
        return src;
    }

    const int dst_len = src.length() * 2;
    char* dst = new char[dst_len];
    dst[0] = 0;

    gbk_to_utf8(src.c_str(), src.length(), dst, dst_len - 1, 0);

    std::string dst_str(dst);
    delete []dst;
    return dst_str;
}

static void set_errmsg(int err_no,
        const appocr_thread_data* databuf,
        const request_t& request,
        response_t& response)
{
    response.err_no = err_no;

    const std::string errmsg = databuf->m_busOcr2fcgi.m_pszTextData;
    response.err_msg = errmsg;
    response.log = "log";
    response.res_data = errmsg;

    FATAL_LOG("[logid:%llu] %s", request.logid, errmsg.c_str());
}

int OcrApiImp::process(const request_t &request, response_t &response) 
{
    nscommon::time_elapsed_t time_used(__FUNCTION__);

    _databuf.reset();
    _databuf.m_busFcgi2ocr.m_uLogId = request.logid;

    if (nsmocr::CParseValue::set_parsed_value(request.req_data, &_databuf))
    {
        set_errmsg(IDLAPI_PARAM_ERR, &_databuf, request, response);
        return IDLAPI_PARAM_ERR;
    }

    int core_ret = 0;
    if (g_conf._repair_stdout)
    {
        int s_fd = 0;
        FILE* fd = NULL;
        nsmocr::CStdout::redir(g_redir_stdout.c_str(), s_fd, fd);
        core_ret = appfeacore(&_databuf);
        nsmocr::CStdout::restore(s_fd, fd);
    }
    else
    {
        core_ret = appfeacore(&_databuf);
    }

    // step 2: work
    if (core_ret != 0)
    {
        set_errmsg(IDLAPI_UNKNOWN, &_databuf, request, response);
        return IDLAPI_UNKNOWN;
    }

    std::string ret = convert_result(&_databuf);

    TRACE_LOG("OcrApiImp::process()[logid:%llu][data:%d] complete",
            request.logid, request.req_data.size());
    response.res_data = ret;

    return IDLAPI_SUCCEED;
}

OcrFactory::OcrFactory () {
    TRACE("OcrFactory::OcrFactory()");
    _exit = 0;
}

OcrFactory::~OcrFactory () {
    TRACE("OcrFactory::~OcrFactory()");
    //_exit = 0;
}

void delete_data()
{
    system("rm -rf ./data.tar.gz");
    system("rm -rf ./data/");
}

int OcrFactory::init(const std::string &conf_path, 
    const std::string &conf_file) {

    TRACE("OcrFactory::init(%s, %s)", 
                conf_path.c_str(), conf_file.c_str());

    int s_fd = 0;
    FILE* fd = NULL;

/*#ifndef INTERNAL_USE
    //jiemi
    if (authentication::ctx_process_file("data.txt", "data.tar.gz"))
    {
        FATAL_LOG("failed to unencrypt data folder!");
        return -1;
    }
    std::string command = "tar -zxvf data.tar.gz";
    if (system(command.c_str()))
    {
        FATAL_LOG("failed to untar model data folder!");
        return -1;
    }
    //
#endif*/

    int ret = general_ocr_init(conf_path.c_str(), "./data/");
    if (ret)
    {
        FATAL_LOG("general_ocr_init error!");
        return -1;
    }

#ifndef BAIDU_MACHINES
    DeviceInfo* device_info = DeviceInfo::get_instance();
    
    std::string disk_info;
    if (authentication::get_disk_uuid(disk_info))
    {
        WARNING("Get device info fail!");
        exit(0);
    }
    std::string disk_info_md5 = authentication::md5(disk_info);
    if (!device_info->is_info_in_list(disk_info_md5))
    {
        WARNING("Compare device info fail!");
        exit(0);
    }

#endif

/*#ifndef INTERNAL_USE    
    //jianquan
    std::string mac_addr;
    if (authentication::get_mac_addr(mac_addr))
    {
        TRACE_LOG("Sign1 authencitaion get fail!");
        exit(0);
    }
    std::string mac_md5 = authentication::md5(mac_addr);
    if ("924a98b5dfc4e7552bc60beeba398594" != mac_md5)
    {
        TRACE_LOG("Sign1 authencitaion compare fail!");
        exit(0);
    }

    authentication::Cpuid cpuid;
    std::string cpuid_info = cpuid.get_cpu_info();
    std::string cpuid_info_md5 = authentication::md5(cpuid_info);

    if ("0ef1bbd8b2628af9e11e3330ae07bdbd" != cpuid_info_md5)
    {
        TRACE_LOG("Sign2 authencitaion compare fail!");
        delete_data();
        exit(0);
    }

    //
    //std::string disk_uuid;
    //if (authentication::get_disk_uuid(disk_uuid))
    //{
    //    TRACE("Sign3 authenciation get fail!");
    //    delete_data();
    //    exit(0);
    //}
    //std::string disk_uuid_md5 = authentication::md5(disk_uuid);
    //if ("56aac3a723a64c6e10c942c685d06335" != disk_uuid_md5)
    //{

    //    TRACE("Sign3 authenciation compare fail!");
    //    delete_data();
    //    exit(0);
    //}
    
    if (authentication::TimeUtility::is_expired("20170601", 2))
    {
        TRACE("Time authenciation fail: expired");
        delete_data();
        exit(0);
    }

    //
#endif*/

    _exit = 0;
    ret = nscommon::CThread::begin_thread((void*)&_thread_arg, tcmalloc_release, _tid);
    if (ret)
    {
        FATAL_LOG("begin thread error!");
        return -1;
    }
    TRACE_LOG("tcmalloc thread begin!");

    return 0;
}

int OcrFactory::destroy_self()
{
    TRACE_LOG("begin destroy_self!");
    nsmocr::ocr_release();

    _exit = 1;
    TRACE_LOG("wait thread exit!");
    pthread_join(_tid, NULL);
    TRACE_LOG("tcmalloc thread exit!");
}

IProcessor * OcrFactory::create() {
    TRACE("OcrFactory::create()");
    OcrApiImp* ocr_imp = new OcrApiImp();
/*#ifndef INTERNAL_USE 
    //jiemi
    system("rm -rf ./data.tar.gz");
    system("rm -rf ./data/");
    //
#endif*/
    return ocr_imp;
}

int OcrFactory::get_info(std::string &info) {
        /*info = "[module:OcrFactory@"VERSION"]"
           "[time:"__DATE__" "__TIME__"]"
           "[gcc:"__VERSION__"]"
           "[usage:general classify]"
           "[provider:gongweibao]";*/
        return idlapi::IDLAPI_SUCCEED;
    }
};//end namespace idlapi

idlapi::ProcessorFactory *create_factory() {
    CDEBUG_LOG("enter create_factory()");
    idlapi::OcrFactory* gcpf = new idlapi::OcrFactory();
    CDEBUG_LOG("after create_factory()");
    return gcpf;
}
/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
