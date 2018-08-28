#ifndef NSCOMMON_GET_JSON_H
#define NSCOMMON_GET_JSON_H

#include "query_data.h"

namespace nsmocr
{
class CGetJson
{
public:
    CGetJson()
    {
    }

    virtual~ CGetJson()
    {
    }
public:
    static std::string get_json_ret(appocr_thread_data* databuf,
        const nsmocr::recg_result_t& recg_ret, nsmocr::recg_result_t* result)
    {
        nscommon::time_elapsed_t time_used(__FUNCTION__);
        std::string json_ret = "";
        std::cout<< "enter the get_json_ret function:"<<std::endl; 
        //gen pdf
        //std::cout<<"nsmocr::CQueryData::is_pdf_gen(databuf):"<<nsmocr::CQueryData::is_pdf_gen(databuf)<<std::endl;
        if (nsmocr::CQueryData::is_pdf_gen(databuf))
        {
            return nsmocr::CGetJson::get_pdf_gen_json_v1(databuf, recg_ret);
        }
        /*
        //anti_spam
        if (nsmocr::CQueryData::is_anti_spam(databuf))
        {
            return nsmocr::CGetJson::get_anti_spam_json_v1(databuf, recg_ret, result);
        }

        //st_ocr
        if (nsmocr::CQueryData::is_st_ocr(databuf))
        {
            return nsmocr::CGetJson::get_stocr_json_v1(databuf, recg_ret);
        }

        //if (databuf->m_busFcgi2ocr.m_iSingleCharNum >= 0) 
        if (nsmocr::CQueryData::is_recg_multi_char(databuf))
        {
            return nsmocr::CGetJson::get_multi_single_char_recg_json_v1(databuf, recg_ret);
        }

        //if (strcmp(databuf->m_busFcgi2ocr.m_pszDetecttype, "SingleCharRecognize") == 0)
        if (nsmocr::CQueryData::is_recg_single_char(databuf))
        {
            return nsmocr::CGetJson::get_single_char_recg_json_v1(databuf, recg_ret);
        }
*/
        //st_ocrapi || st_ocrapi_all
        if (strcmp(databuf->m_busFcgi2ocr.m_pszDetecttype, "LocateRecognize") == 0 
                || strcmp(databuf->m_busFcgi2ocr.m_pszDetecttype, "Recognize") == 0 
                || strcmp(databuf->m_busFcgi2ocr.m_pszDetecttype, "") == 0)
        {
            if (strcmp(databuf->m_busFcgi2ocr.m_pszServiceType, "st_ocrapi") == 0) 
            {
                if (nsmocr::CQueryData::is_detect_img_dir(databuf))
                {
                    json_ret = nsmocr::CGetJson::get_recg_json_img_info_v1(databuf, recg_ret);
                }
                else
                {
                    json_ret = nsmocr::CGetJson::get_recg_json_v1(databuf, recg_ret);
                }
                return json_ret;
            } 
            else if (strcmp(databuf->m_busFcgi2ocr.m_pszServiceType, "st_ocrapi_all") == 0) 
            {
                if (nsmocr::CQueryData::is_bdtrans_menu_chn(databuf)) {
                    // designed for baidu translation menu
                    json_ret = nsmocr::CGetJson::get_recg_wordpos_candidate_json(databuf, recg_ret);
                }
                else {
                    json_ret = nsmocr::CGetJson::get_recg_wordpos_json_v1(databuf, recg_ret);
                }
                return json_ret;
            }
        } 
        else if (strcmp(databuf->m_busFcgi2ocr.m_pszDetecttype, "Locate") == 0) 
        {
            json_ret = nsmocr::CGetJson::get_locate_json_img_info_v1(databuf, recg_ret);
            return json_ret;
        }
        return json_ret;
    }

public:
    static std::string get_pdf_gen_json_v1(const appocr_thread_data* databuf,
            const nsmocr::recg_result_t& recg_ret)
    {
        nscommon::time_elapsed_t time_used(__FUNCTION__);
        std::string error;
        std::string json_ret;
        std::cout<<"enter get_pdf_gen_json_v1 function"<<std::endl;
        if (nsmocr::GenResult::get_pdf_gen_json(&(databuf->m_busFcgi2ocr), recg_ret,
                    &json_ret, &error))
        {
            WARNING_LOG("get_pdf_gen_json_v1 error errmsg:%s", error.c_str());
            return "";
        }
        std::cout<<"get_pdf_gen_json_v1:"<<json_ret<<std::endl;
        return json_ret;
    }

    static std::string get_stocr_json_v1(const appocr_thread_data* databuf,
            const nsmocr::recg_result_t& recg_ret)
    {
        nscommon::time_elapsed_t time_used(__FUNCTION__);
        std::string error;
        std::string json_ret;

        if (nsmocr::GenResult::get_stocr_json(recg_ret,
                databuf->m_busFcgi2ocr.m_pszQuerySign,
                &json_ret, &error))
        {
            WARNING_LOG("get_stocr_json_v1 error errmsg:%s", error.c_str());
            return "";
        }

        return json_ret;
    }

    static std::string get_anti_spam_json_v1(const appocr_thread_data* databuf,
            const nsmocr::recg_result_t& recg_ret, const nsmocr::recg_result_t* result)
    {
        nscommon::time_elapsed_t time_used(__FUNCTION__);
        std::string error;
        std::string json_ret;
     
        if (nsmocr::GenResult::get_anti_spam_json(recg_ret,
                    databuf->m_busFcgi2ocr.m_pszQuerySign, &json_ret, &error, result))
        {
            WARNING_LOG("get_anti_spam_json_v1 error errmsg:%s", error.c_str());
            return ""; 
        }  
        
        return json_ret;
    }

    static std::string get_recg_json_v1(const appocr_thread_data* databuf,
            const nsmocr::recg_result_t& recg_ret)
    {
        nsmocr::dis_info_t dis_info;

        if (CQueryData::is_disp_line_poly(databuf)) {
            dis_info.b_disp_line_poly = true;
        }
        if (CQueryData::is_disp_line_prob(databuf)) {
            dis_info.b_disp_line_prob = true;
        }
        /*
        if (CQueryData::is_utf8(databuf))
        {
            dis_info.coding = coding_utf8;
        }
        else
        {
            dis_info.coding = coding_gbk;
        }
        */
        if (CQueryData::is_get_lang_prob(databuf)) {
            dis_info.b_img_info = true;
        }
        // added by lx to output detected_obj_type
        if (CQueryData::is_detected_lottery(databuf)) {
            dis_info.b_img_info = true;
        }

        //added by zhouxiangyu
        if (!CQueryData::is_disp_rect(databuf))
        {
        dis_info.b_img_rect = false;
        }

        std::string error;
        std::string json_ret;

        nsmocr::GenResult::get_recg_json(recg_ret,
                databuf->m_busFcgi2ocr.m_pszQuerySign,
                &json_ret, &error, dis_info);

        return json_ret;
    }

    static std::string get_recg_json_img_info_v1(const appocr_thread_data* databuf,
            const nsmocr::recg_result_t& recg_ret)
    {
        nsmocr::dis_info_t dis_info;
        dis_info.b_img_info = true; 
        dis_info.b_img_dir = true;

        //added by zhouxiangyu
        if (!CQueryData::is_disp_rect(databuf))
        {
            dis_info.b_img_rect = false;
        }

        /*
        if (CQueryData::is_utf8(databuf))
        {
            dis_info.coding = coding_utf8;
        }
        else
        {
            dis_info.coding = coding_gbk;
        }
        */
        if (CQueryData::is_get_lang_prob(databuf)) {
            dis_info.b_img_info = true;
        }
        if (CQueryData::is_detected_lottery(databuf)) {
            dis_info.b_img_info = true;
        }

        if (CQueryData::is_disp_line_poly(databuf)) {
            dis_info.b_disp_line_poly = true;
        }

        if (CQueryData::is_disp_line_prob(databuf)) {
            dis_info.b_disp_line_prob = true;
        }
        std::string error;
        std::string json_ret;

        nsmocr::GenResult::get_recg_json(recg_ret,
                databuf->m_busFcgi2ocr.m_pszQuerySign,
                &json_ret, &error, dis_info);

        return json_ret;
    }

    static std::string get_recg_wordpos_json_v1(const appocr_thread_data* databuf,
            const nsmocr::recg_result_t& recg_ret)
    {
        nsmocr::dis_info_t dis_info;
        dis_info.all_words_pos = true;
        if (CQueryData::is_get_lang_prob(databuf)) {
            dis_info.b_img_info = true;
        }
        if (CQueryData::is_detected_lottery(databuf)) {
            dis_info.b_img_info = true;
        }
        //added by zhouxiangyu
        dis_info.b_img_info = true; 
        dis_info.b_img_dir = true;

        if (!CQueryData::is_disp_rect(databuf))
        {
            dis_info.b_img_rect = false;
        }
        if (!CQueryData::is_disp_old_style_img_dir(databuf))
        {
            dis_info.b_old_style_img_dir = false;
        }

        /*
        if (CQueryData::is_utf8(databuf))
        {
            dis_info.coding = coding_utf8;
        }
        else
        {
            dis_info.coding = coding_gbk;
        }
        */
        if (CQueryData::is_get_lang_prob(databuf)) {
            dis_info.b_img_info = true;
        }

        if (CQueryData::is_disp_line_poly(databuf)) {
            dis_info.b_disp_line_poly = true;
        }

        if (CQueryData::is_disp_line_prob(databuf)) {
            dis_info.b_disp_line_prob = true;
        }

        std::string error;
        std::string json_ret;
        nsmocr::GenResult::get_recg_json(recg_ret,
                databuf->m_busFcgi2ocr.m_pszQuerySign,
                &json_ret, &error, dis_info);

        return json_ret;
    }

    // get candidate for the words
    static std::string get_recg_wordpos_candidate_json(const appocr_thread_data* databuf,
            const nsmocr::recg_result_t& recg_ret)
    {
        nsmocr::dis_info_t dis_info;
        dis_info.all_words_pos = true;
        if (CQueryData::is_get_lang_prob(databuf)) {
            dis_info.b_img_info = true;
        }
        if (CQueryData::is_detected_lottery(databuf)) {
            dis_info.b_img_info = true;
        }
        dis_info.b_img_info = true; 
        dis_info.b_img_dir = true;

        if (CQueryData::is_disp_line_poly(databuf)) {
            dis_info.b_disp_line_poly = true;
        }

        if (CQueryData::is_disp_line_prob(databuf)) {
            dis_info.b_disp_line_prob = true;
        }
        if (!CQueryData::is_disp_rect(databuf))
        {
            dis_info.b_img_rect = false;
        }

        if (!CQueryData::is_disp_old_style_img_dir(databuf))
        {
            dis_info.b_old_style_img_dir = false;
        }

        if (CQueryData::is_get_lang_prob(databuf)) {
            dis_info.b_img_info = true;
        }

        std::string error;
        std::string json_ret;
        nsmocr::GenResult::get_recg_candidate_json(recg_ret,
                databuf->m_busFcgi2ocr.m_pszQuerySign,
                &json_ret, &error, dis_info);

        return json_ret;
    }

    static std::string get_single_char_recg_json_v1(const appocr_thread_data* databuf,
            const nsmocr::recg_result_t& recg_ret)
    {
        nscommon::time_elapsed_t time_used(__FUNCTION__);
        std::string error;
        std::string json_ret;

        nsmocr::GenResult::get_single_char_recg_json(recg_ret,
                databuf->m_busFcgi2ocr.m_pszQuerySign,
                &json_ret, &error);

        return json_ret;
    }

    static std::string get_multi_single_char_recg_json_v1(const appocr_thread_data* databuf,
            const nsmocr::recg_result_t& recg_ret)
    {
        std::string error;
        std::string json_ret;

        nsmocr::GenResult::get_multi_single_char_recg_json(recg_ret,
            databuf->m_busFcgi2ocr.m_pszQuerySign,
            &json_ret, &error);

        return json_ret;
    }

    
    //static std::string get_locate_json_v1(const appocr_thread_data* databuf,
    //        const nsmocr::recg_result_t& recg_ret)
    //{
    //    nsmocr::dis_info_t dis_info;
    //    //dis_info.b_img_info = true;
    //    //dis_info.b_img_dir = true;
    //    if (CQueryData::is_get_lang_prob(databuf)) {
    //        dis_info.b_img_info = true;
    //    }
    //    if (CQueryData::is_detected_lottery(databuf)) {
    //        dis_info.b_img_info = true;
    //    }

    //    std::string error;
    //    std::string json_ret;

    //    nsmocr::GenResult::get_locate_json(recg_ret,
    //            databuf->m_busFcgi2ocr.m_pszQuerySign,
    //            &json_ret, &error, dis_info);

    //    return json_ret;
    //}

    static std::string get_locate_json_img_info_v1(const appocr_thread_data* databuf,
            const nsmocr::recg_result_t& recg_ret)
    {
        nsmocr::dis_info_t dis_info;
        dis_info.b_img_info = true;
        dis_info.b_img_dir = true;
        dis_info.b_disp_line_poly = CQueryData::is_disp_line_poly(databuf);

        if (!CQueryData::is_disp_rect(databuf))
        {
            dis_info.b_img_rect = false;
        }

        std::string error;
        std::string json_ret;

        nsmocr::GenResult::get_locate_json(recg_ret,
                databuf->m_busFcgi2ocr.m_pszQuerySign,
                &json_ret, &error, dis_info);

        return json_ret;
    }

    /*
    static std::string get_locate_img_info_json_v1(const appocr_thread_data* databuf,
            const nsmocr::recg_result_t& recg_ret)
    {
        nsmocr::dis_info_t dis_info;
        dis_info.b_img_info = true;

        std::string error;
        std::string json_ret;

        nsmocr::GenResult::get_locate_json(recg_ret,
                databuf->m_busFcgi2ocr.m_pszQuerySign,
                &json_ret, &error, dis_info);

        return json_ret;
    }
    */
};

};
#endif

