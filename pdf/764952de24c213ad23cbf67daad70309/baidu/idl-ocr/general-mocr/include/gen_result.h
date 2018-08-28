#ifndef NSMOCR_GEN_RESULT_H
#define NSMOCR_GEN_RESULT_H

#include "mocr.pb.h"
#include "locrecg.h"
#include "pb_to_json.h"

#include <string>
#include "appocr_fcgi2ocr_bus.h"
#include "image_process.h"
#include "ocr_error.h"
#include "locrecg.h"
#include "text_func.h"
#include <regex.h>
#include "aho_corasick.h"
#include "pdf_saver.h"
#include "appocr_conf.h"
#include "loadimage.h"

extern std::map<std::string, std::string> g_politics_map;
extern std::map<std::string, std::string> g_adsex_map;
extern appocr_conf g_conf;

namespace nsmocr
{

class GenResult
{
public:
    GenResult()
    {
    }
    virtual~ GenResult()
    {
    }

public:

public:
    
    static std::string get_img_dir_json(int img_dir)
    {
        char tmp[256];
        snprintf(tmp, sizeof(tmp) - 1, 
                "{\"imgdir\":\"%d\"}", img_dir);

        return std::string(tmp);
    }

    static std::string add_img_dir_json(std::string& json_ret, std::string img_dir_json)
    {
        std::string ret = "";
        const char* b = json_ret.c_str();

        char str[] = "\"ret\":[";
        int str_len = strlen(str);

        const char* pos = strstr(b, str);
        if (NULL == pos)
        {
            return ret;
        }

        int sub_len = pos + str_len - b;
        assert(sub_len >= 0);
    
        ret = std::string(b, pos + str_len - b)  + img_dir_json + "," + std::string(pos + str_len);
        return ret;
    }

    static  std::string add_errno(const std::string& src, int err_no)
    {
        std::string json_ret;
        //add errno memeber to compatible last version
        const char* pos = strstr(src.c_str(), "{");
        if (pos == NULL)
        {
            json_ret = "";
            return json_ret;
        }

        char tmp[128];
        snprintf(tmp, sizeof(tmp) - 1, "{\"errno\":\"%d\",", err_no);

        json_ret = std::string(tmp) + std::string(pos + 1);
        return json_ret;
    }

    static int get_err_json(int err_no,
            std::string errmas, std::string* json_ret,
            std::string logid, std::string query_sign)
    {
        mocr::RecgResult obj; 

        //obj.set_err_no(err_no);
        obj.set_errmas(errmas);
        obj.set_querysign(query_sign);
        obj.set_logid(logid);

        Pb2JsonOptions options;
        std::string error;
        std::string json_tmp;
        bool ret = ProtoMessageToJson(obj, &json_tmp, options, &error);
        if (!ret)
        {
            WARNING_LOG("ProtoMessageToJson error:%s", error.c_str());
            return ERR_JSON;
        }

        *json_ret = add_errno(json_tmp, err_no);
        if (json_ret->length() <= 0)
        {
            return ERR_JSON;
        }
        return 0; 
    }

    //st_ocrapi,st_ocrapi_all
    //locaterecg,recognize
    static int get_recg_json(const nsmocr::recg_result_t& recg_result,
            std::string query_sign, std::string* json_ret, std::string* error,
           const dis_info_t& dis_info)
    {
        mocr::RecgResult obj; 
        const int err_no = 0;
        //obj.set_err_no(err_no);
        obj.set_errmas("");
        obj.set_querysign(query_sign);

        if (dis_info.b_img_info)
        {
            mocr::ImageInfo* dst = obj.mutable_image_info();
            dst->set_obj_type(recg_result.img_info.obj_type);
            dst->set_text_type(recg_result.img_info.text_type);
            dst->set_image_dir(recg_result.img_info.img_dir);
            dst->set_lang_cate(recg_result.classify_lang_cate);
            dst->set_image_status(recg_result.img_info.img_status);
            for (int j = 0; j < recg_result.lang_prob_map.size(); j++) {
                mocr::LangProb * lang_prob  = dst->add_lang_prob_map();
                lang_prob->set_lang(recg_result.lang_prob_map[j].lang);
                lang_prob->set_prob(recg_result.lang_prob_map[j].prob);
            }
            std::string none_detected_obj = "-1";
            if (strcmp(recg_result.img_info.detected_obj_type.c_str(), 
                    none_detected_obj.c_str()) != 0 && 
                    recg_result.img_info.detected_obj_type.length() > 0) {
                dst->set_detected_obj_type(recg_result.img_info.detected_obj_type);
            }
            if (recg_result.detected_obj_type_list.size() > 0) {
                for (int j = 0; j < recg_result.detected_obj_type_list.size(); j++) {
                    if (strcmp(recg_result.detected_obj_type_list[j].obj.c_str(),
                        none_detected_obj.c_str()) != 0 && 
                        recg_result.detected_obj_type_list[j].obj.length() > 0) {
                        mocr::Obj_type* objtype  = dst->add_detected_obj_type_list();
                        objtype->set_obj(recg_result.detected_obj_type_list[j].obj);
                        objtype->set_result(
                                    recg_result.detected_obj_type_list[j].result);
                    }
                }
            }

            /*
            if (recg_result.img_info.obj_type >= 0)
            {
                dst->set_obj_type(recg_result.img_info.obj_type);
            }

            if (recg_result.img_info.img_dir >= 0)
            {
                dst->set_image_dir(recg_result.img_info.img_dir);
            }

            if (recg_result.img_info.text_type >= 0)
            {
                dst->set_text_type(recg_result.img_info.text_type);
            }
            */
        }

        /*
        if (dis_info.base64 == 1)
        {
            obj.set_base64(1);
        }
        else if (dis_info.base64 == 0)
        {
            obj.set_base64(0);
        }

        if (dis_info.coding == CODING_UTF8)
        {
            obj.set_coding("utf8");
        }
        else if (dis_info.coding == CODING_GBK)
        {
            obj.set_coding("gbk");
        }
        */

        const std::vector<nsmocr::recg_region_t>& regions = 
            recg_result.regions;

        for (int i = 0; i < (int)regions.size(); i++)
        {
            nsmocr::recg_region_t src = regions[i];
            mocr::RecgRegion* dst = obj.add_ret();
            mocr::Rect* rect = dst->mutable_rect();

            if (dis_info.b_img_rect){
                rect->set_left(CTextFunc::to_string(src.left));
                rect->set_top(CTextFunc::to_string(src.top));
                rect->set_width(CTextFunc::to_string(src.right - src.left + 1));
                rect->set_height(CTextFunc::to_string(src.bottom - src.top + 1));
            }

            // if poly_location is set
            if (dis_info.b_disp_line_poly && 
                src.poly_location.size() >= 8 && src.poly_location.size() % 2 == 0)
            {
                mocr::IntPolyLoc* dst_poly = dst->mutable_poly_location();
                for (int i = 0; i < src.poly_location.size() / 2; i++)
                {
                    mocr::IntPoint* dst_pt = dst_poly->add_points();
                    dst_pt->set_x(src.poly_location[i * 2]);
                    dst_pt->set_y(src.poly_location[i * 2 + 1]);
                }
            }

            //lx added for the line prob
            if (dis_info.b_disp_line_prob &&
                (src.line_prob.avg_prob >= 0.0 || src.line_prob.min_prob >= 0.0 ||
                 src.line_prob.prob_var >= 0.0)) {
                mocr::LineProb* line_prob = dst->mutable_line_probability();
                line_prob->set_average(CTextFunc::to_string(src.line_prob.avg_prob));
                line_prob->set_min(CTextFunc::to_string(src.line_prob.min_prob));
                line_prob->set_variance(CTextFunc::to_string(src.line_prob.prob_var));
            }

            std::string word_name_json;
            std::string word_json;

            if (dis_info.base64 == 1)
            {
                nscommon::CImageProcess::base64_encode(src.name, word_name_json);
                nscommon::CImageProcess::base64_encode(src.ocrresult, word_json);
                if (dis_info.b_word_name
                        || src.name.length() > 0)
                {
                    dst->set_word_name(word_name_json);
                }
                dst->set_word(word_json);
            }
            else
            {
                if (dis_info.b_word_name
                        || src.name.length() > 0)
                {
                    dst->set_word_name(src.name);
                }
                dst->set_word(src.ocrresult);
            }

            std::vector<nsmocr::recg_wordinfo_t> wordinfos = 
                src.wordinfo;
            if (wordinfos.size() > 0 && dis_info.all_words_pos)
            {
                for (int j = 0; j < (int)wordinfos.size(); j++)
                {
                    nsmocr::recg_wordinfo_t wordinfo = wordinfos[j];
                    mocr::WordInfo* charset = dst->add_charset();
                    mocr::Rect* rect = charset->mutable_rect();

                    if (dis_info.b_img_rect){
                    rect->set_left(CTextFunc::to_string(wordinfo.rect.left));
                    rect->set_top(CTextFunc::to_string(wordinfo.rect.top));
                    rect->set_width(
                            CTextFunc::to_string(wordinfo.rect.right - wordinfo.rect.left + 1));
                    rect->set_height(
                            CTextFunc::to_string(wordinfo.rect.bottom - wordinfo.rect.top + 1));
                    }

                    std::string word_json;
                    if (dis_info.base64 == 1)
                    {
                        nscommon::CImageProcess::base64_encode(wordinfo.word, word_json);
                        charset->set_word(word_json);
                    }
                    else
                    {
                        charset->set_word(wordinfo.word);
                    }
                }
            }
        }

        Pb2JsonOptions options;
        std::string json_tmp;
        bool ret = ProtoMessageToJson(obj, &json_tmp, options, error);
        if (!ret)
        {
            return ERR_JSON;
        }

        *json_ret = add_errno(json_tmp, err_no);
        if (json_ret->length() <= 0)
        {
            return ERR_JSON;
        }

        /*if (dis_info.b_img_dir &&
            dis_info.b_old_style_img_dir
                && recg_result.img_info.img_dir >= 0
                && regions.size() > 0)
        {
            std::string img_dir_json = get_img_dir_json(recg_result.img_info.img_dir);
            *json_ret = add_img_dir_json(*json_ret, img_dir_json);
            if (json_ret->length() <= 0)
            {
                return ERR_JSON;
            }
        }*/

        return 0;
    }

    // output the candidate and probability
    static int get_recg_candidate_json(const nsmocr::recg_result_t& recg_result,
            std::string query_sign, std::string* json_ret, std::string* error,
           const dis_info_t& dis_info)
    {
        mocr::RecgResult obj; 
        const int err_no = 0;
        obj.set_errmas("");
        obj.set_querysign(query_sign);

        if (dis_info.b_img_info)
        {
            mocr::ImageInfo* dst = obj.mutable_image_info();
            dst->set_obj_type(recg_result.img_info.obj_type);
            dst->set_text_type(recg_result.img_info.text_type);
            dst->set_image_dir(recg_result.img_info.img_dir);
            dst->set_lang_cate(recg_result.classify_lang_cate);
            for (int j = 0; j < recg_result.lang_prob_map.size(); j++) {
                mocr::LangProb * lang_prob  = dst->add_lang_prob_map();
                lang_prob->set_lang(recg_result.lang_prob_map[j].lang);
                lang_prob->set_prob(recg_result.lang_prob_map[j].prob);
            }
        }

        const std::vector<nsmocr::recg_region_t>& regions = 
            recg_result.regions;

        for (int i = 0; i < (int)regions.size(); i++)
        {
            nsmocr::recg_region_t src = regions[i];
            mocr::RecgRegion* dst = obj.add_ret();
            mocr::Rect* rect = dst->mutable_rect();

            if (dis_info.b_img_rect){
                rect->set_left(CTextFunc::to_string(src.left));
                rect->set_top(CTextFunc::to_string(src.top));
                rect->set_width(CTextFunc::to_string(src.right - src.left + 1));
                rect->set_height(CTextFunc::to_string(src.bottom - src.top + 1));
            }

            std::string word_name_json;
            std::string word_json;

            if (dis_info.base64 == 1)
            {
                nscommon::CImageProcess::base64_encode(src.name, word_name_json);
                nscommon::CImageProcess::base64_encode(src.ocrresult, word_json);
                if (dis_info.b_word_name
                        || src.name.length() > 0)
                {
                    dst->set_word_name(word_name_json);
                }
                dst->set_word(word_json);
            }
            else
            {
                if (dis_info.b_word_name
                        || src.name.length() > 0)
                {
                    dst->set_word_name(src.name);
                }
                dst->set_word(src.ocrresult);
            }

            std::vector<nsmocr::recg_wordinfo_t> wordinfos = 
                src.wordinfo;
            if (wordinfos.size() > 0 && dis_info.all_words_pos)
            {
                for (int j = 0; j < (int)wordinfos.size(); j++)
                {
                    nsmocr::recg_wordinfo_t wordinfo = wordinfos[j];
                    mocr::WordInfo* charset = dst->add_charset();
                    mocr::Rect* rect = charset->mutable_rect();

                    if (dis_info.b_img_rect){
                    rect->set_left(CTextFunc::to_string(wordinfo.rect.left));
                    rect->set_top(CTextFunc::to_string(wordinfo.rect.top));
                    rect->set_width(
                            CTextFunc::to_string(wordinfo.rect.right - wordinfo.rect.left + 1));
                    rect->set_height(
                            CTextFunc::to_string(wordinfo.rect.bottom - wordinfo.rect.top + 1));
                    }

                    std::string word_json;
                    if (dis_info.base64 == 1)
                    {
                        nscommon::CImageProcess::base64_encode(wordinfo.word, word_json);
                        charset->set_word(word_json);
                    }
                    else
                    {
                        charset->set_word(wordinfo.word);
                    }
                    // add the recognition probabilty output
                    charset->set_recg_prob(wordinfo.probability);
                    for (int cand_index = 0; cand_index < wordinfo.candidate.size(); cand_index++) {
                        mocr::SingleCharInfo* candidate_char = charset->add_candidate(); 
                        candidate_char->set_word(wordinfo.candidate[cand_index].word);
                        candidate_char->set_prob(\
                            CTextFunc::to_string(wordinfo.candidate[cand_index].probability));
                    }
                }
            }
        }

        Pb2JsonOptions options;
        std::string json_tmp;
        bool ret = ProtoMessageToJson(obj, &json_tmp, options, error);
        if (!ret)
        {
            return ERR_JSON;
        }

        *json_ret = add_errno(json_tmp, err_no);
        if (json_ret->length() <= 0)
        {
            return ERR_JSON;
        }

        /*if (dis_info.b_img_dir &&
            dis_info.b_old_style_img_dir
                && recg_result.img_info.img_dir >= 0
                && regions.size() > 0)
        {
            std::string img_dir_json = get_img_dir_json(recg_result.img_info.img_dir);
            *json_ret = add_img_dir_json(*json_ret, img_dir_json);
            if (json_ret->length() <= 0)
            {
                return ERR_JSON;
            }
        }*/

        return 0;
    }
    //single 
    static int get_single_char_recg_json(const nsmocr::recg_result_t& recg_result,
            std::string query_sign, std::string* json_ret, std::string* error)
    {
        nscommon::time_elapsed_t time_used(__FUNCTION__);

        mocr::SingleCharRecgResult obj; 
        const int err_no = 0;
        //obj.set_err_no(err_no);
        obj.set_errmas("");
        obj.set_querysign(query_sign);

        const std::vector<nsmocr::recg_region_t>& regions = 
            recg_result.regions;

        for (int i = 0; i < (int)regions.size(); i++)
        {
            std::vector<recg_wordinfo_t> wordinfos = regions[i].wordinfo;
            for (int j = 0; j < (int)wordinfos.size(); j++)
            {
                mocr::SingleCharInfo* obj_ret = obj.add_ret();
                nsmocr::recg_wordinfo_t wordinfo = wordinfos[j];

                obj_ret->set_word(wordinfo.word);
                obj_ret->set_prob(CTextFunc::to_string(wordinfo.probability));
            }
        }

        Pb2JsonOptions options;
        std::string json_tmp;
        bool ret = ProtoMessageToJson(obj, &json_tmp, options, error);
        if (!ret)
        {
            return ERR_JSON;
        }

        *json_ret = add_errno(json_tmp, err_no);
        if (json_ret->length() <= 0)
        {
            return ERR_JSON;
        }

        return 0;
    }

    static int get_locate_json(const nsmocr::recg_result_t& recg_result,
            std::string query_sign, std::string* json_ret, std::string* error,
            const dis_info_t& dis_info)
    {
        mocr::LocateResult obj; 
        const int err_no = 0;
        //obj.set_err_no(err_no);
        obj.set_errmas("");
        obj.set_querysign(query_sign);

        if (dis_info.b_img_info)
        {
            mocr::ImageInfo* dst = obj.mutable_image_info();
            dst->set_obj_type(recg_result.img_info.obj_type);
            dst->set_text_type(recg_result.img_info.text_type);
            dst->set_image_dir(recg_result.img_info.img_dir);
            for (int j = 0; j < recg_result.lang_prob_map.size(); j++) {
                mocr::LangProb * lang_prob  = dst->add_lang_prob_map();
                lang_prob->set_lang(recg_result.lang_prob_map[j].lang);
                lang_prob->set_prob(recg_result.lang_prob_map[j].prob);
            }


            /*
            if (recg_result.img_info.obj_type >= 0)
            {
                dst->set_obj_type(recg_result.img_info.obj_type);
            }

            if (recg_result.img_info.img_dir >= 0)
            {
                dst->set_image_dir(recg_result.img_info.img_dir);
            }

            if (recg_result.img_info.text_type >= 0)
            {
                dst->set_text_type(recg_result.img_info.text_type);
            }
            */
        }

        const std::vector<nsmocr::recg_region_t>& regions = 
            recg_result.regions;

        ///*
        //mocr::LocateRegion* dst = obj.mutable_ret();
        for (int i = 0; i < (int)regions.size(); i++)
        {
            nsmocr::recg_region_t src = regions[i];
            //mocr::Rect* rect = obj.add_ret();

            mocr::LocateRegion* dst = obj.add_ret();
            mocr::Rect* rect = dst->mutable_rect();

            // if poly_location is set
            if (dis_info.b_disp_line_poly && 
                src.poly_location.size() >= 8 && src.poly_location.size() % 2 == 0)
            {
                mocr::IntPolyLoc* dst_poly = dst->mutable_poly_location();
                for (int i = 0; i < src.poly_location.size() / 2; i++)
                {
                    mocr::IntPoint* dst_pt = dst_poly->add_points();
                    dst_pt->set_x(src.poly_location[i * 2]);
                    dst_pt->set_y(src.poly_location[i * 2 + 1]);
                }
            }

            rect->set_left(CTextFunc::to_string(src.left));
            rect->set_top(CTextFunc::to_string(src.top));
            rect->set_width(CTextFunc::to_string(src.right - src.left + 1));
            rect->set_height(CTextFunc::to_string(src.bottom - src.top + 1));
        }
        //*/
        
        /*
        mocr::LocateRegion* dst = obj.mutable_ret();
        for (int i = 0; i < (int)regions.size(); i++)
        {
            nsmocr::recg_region_t src = regions[i];
            mocr::Rect* rect = dst->add_rect();

            rect->set_left(CTextFunc::to_string(src.left));
            rect->set_top(CTextFunc::to_string(src.top));
            rect->set_width(CTextFunc::to_string(src.right - src.left + 1));
            rect->set_height(CTextFunc::to_string(src.bottom - src.top + 1));
        }
        */

        Pb2JsonOptions options;
        std::string json_tmp;
        bool ret = ProtoMessageToJson(obj, &json_tmp, options, error);
        if (!ret)
        {
            return ERR_JSON;
        }

        *json_ret = add_errno(json_tmp, err_no);
        if (json_ret->length() <= 0)
        {
            return ERR_JSON;
        }

        /*if (dis_info.b_img_dir 
                && recg_result.img_info.old_img_dir >= 0
                && regions.size() > 0)
        {
            std::string img_dir_json = get_img_dir_json(recg_result.img_info.old_img_dir);
            *json_ret = add_img_dir_json(*json_ret, img_dir_json);
            if (json_ret->length() <= 0)
            {
                return ERR_JSON;
            }
        }*/
        return 0; 
    }

    static std::string get_recg_json_img_info_v1_pdf(char * query_sign,
            const nsmocr::recg_result_t& recg_ret)
    {   
        nsmocr::dis_info_t dis_info;
        dis_info.b_img_info = true; 
        dis_info.b_img_dir = true;

        dis_info.b_img_rect = true;
        dis_info.b_disp_line_poly = false;
        dis_info.b_disp_line_prob = false;

        std::string error;
        std::string json_ret;

        nsmocr::GenResult::get_recg_json(recg_ret,
                query_sign,
                &json_ret, &error, dis_info);

        return json_ret;
    }

    static int get_pdf_gen_json(const appocr_fcgi2ocr_bus * bus, 
            const nsmocr::recg_result_t& recg_result, std::string* json_ret, std::string* error)
    {
        std::string query_sign = std::string(bus->m_pszQuerySign);
        //int ret = get_stocr_json(recg_result, query_sign, json_ret, error);

        *json_ret = get_recg_json_img_info_v1_pdf(bus->m_pszQuerySign, recg_result);
        std::cout << "*json_ret:" << *json_ret << std::endl;
        //add query_sign
        const char* pos = strstr((*json_ret).c_str(), "{");
        if (pos == NULL)
        {
            WARNING_LOG("cannot find { in json_ret");
            return nsmocr::ERR_JSON;
        }

        //*json_ret = std::string("{\"querysign\":\"") + query_sign + "\"" + std::string(pos + 1);
        *json_ret = std::string("{\"file\":\"") + 
            std::string(g_conf._file_output_path) +"/pdf/" + query_sign + ".pdf\"," + std::string(pos + 1);

        if (g_conf._file_output_path[0] != 0)
        {
            cv::Mat mat = loadcvmatfrommem(bus->m_pImageData, bus->m_uImageDataLen, 1);
            if (mat.empty()) {
                WARNING_LOG("logid:%llu load image_mat error!", bus->m_uLogId);
                return nsmocr::ERR_LOAD_IMG;
            }

            if (mat.cols > mat.rows) {
                double scale = ((double)1500) / mat.cols;
                double dst_height = scale * mat.rows;
                cv::Size mat_size(1500, dst_height);
                cv::resize(mat, mat, mat_size, mat.cols / 2, mat.rows / 2, cv::INTER_LINEAR);
            }else {
                double scale = ((double)1500) / mat.rows;
                double dst_width = scale * mat.cols;
                cv::Size mat_size(dst_width, 1500);
                cv::resize(mat, mat, mat_size, mat.cols / 2, mat.rows / 2, cv::INTER_LINEAR);
            }  
            std::cout << "mat size:" << mat.rows << " " << mat.cols << std::endl;
            IplImage img = mat;
            for (int i = 0; i < recg_result.regions.size(); i++)
            { 
                std::cout << "recg_result:" << recg_result.regions[i].ocrresult << std::endl;
            }
            std::cout << "g_conf._file_output_path:" << std::string(g_conf._file_output_path) << std::endl;
            std::cout << "query_sign:" << query_sign << std::endl;
            CPDFSaver pdf_saver(recg_result, std::string(g_conf._file_output_path), query_sign, 
                    &img);
            int init_result = pdf_saver.init();
            if (init_result != 0)
            {
                WARNING_LOG("init pdf fail");
                return nsmocr::ERR_PDF;
            }
            int pdf_result = pdf_saver.save();
            if (pdf_result != 0)
            {
                WARNING_LOG("save pdf fail");
                return nsmocr::ERR_PDF; 
            }
            return 0;
        } 
        else
        {
            return 0;
        }
    }
    static int get_stocr_json(const nsmocr::recg_result_t& recg_result,
            std::string query_sign, std::string* json_ret, std::string* error)
    {
        nscommon::time_elapsed_t time_used(__FUNCTION__);

        *json_ret = "";
        mocr::STOCRResult obj; 
        //const int err_no = 0;
        //obj.set_err_no(err_no);
        //obj.set_errmas("");
        //obj.set_querysign(query_sign);

        const std::vector<nsmocr::recg_region_t>& regions = 
            recg_result.regions;

        //mocr::STOCRRegin* obj_result = obj.mutable_result();
        for (int i = 0; i < (int)regions.size(); i++)
        {
            nsmocr::recg_region_t src = regions[i];
            mocr::STOCRRegion* obj_result = obj.add_result();

            char tmp[256];
            snprintf(tmp, sizeof(tmp) - 1, "%d %d %d %d",
                    src.left, src.top,
                    src.right - src.left + 1, 
                    src.bottom - src.top + 1);

            obj_result->set_rect(tmp);
            obj_result->set_word(src.ocrresult);
        }

        Pb2JsonOptions options;
        //std::string json_tmp;
        bool ret = ProtoMessageToJson(obj, json_ret, options, error);
        if (!ret)
        {
            return ERR_JSON;
        }

        /*
        *json_ret = add_errno(json_tmp, err_no);
        if (json_ret->length() <= 0)
        {
            return ERR_JSON;
        }
        */

        return 0;
    }

    static int get_anti_spam_json(const nsmocr::recg_result_t& recg_result, std::string query_sign, 
            std::string* json_ret, std::string* error, const nsmocr::recg_result_t* result)
    {
        nscommon::time_elapsed_t time_used(__FUNCTION__);

        *json_ret = "";
        mocr::AntiSpam obj;
        obj.set_errmas("");
        obj.set_querysign(query_sign);
        
        get_anti_spam_words_json(recg_result, query_sign, json_ret, obj);
        get_anti_spam_locations_json(result, query_sign, json_ret, obj);
        get_anti_spam_account_json(recg_result, query_sign, json_ret, obj);
        get_anti_spam_vocab_json(recg_result, query_sign, json_ret, obj);
        
        Pb2JsonOptions options;
        std::string json_tmp;
        bool ret = ProtoMessageToJson(obj, &json_tmp, options, error);
        if (!ret)
        {
            return ERR_JSON;
        }

        const int err_no = 0;
        *json_ret = add_errno(json_tmp, err_no);
        if (json_ret->length() <= 0)
        {
            return ERR_JSON;
        }

        return 0;
    }

    static int get_multi_single_char_recg_json(const nsmocr::recg_result_t& recg_result,
            std::string query_sign, std::string* json_ret, std::string* error)
    {
        nscommon::time_elapsed_t time_used(__FUNCTION__);
        mocr::MultiSingleCharRecgResult obj; 
        const int err_no = 0;
        //obj.set_err_no(err_no);
        obj.set_errmas("");
        obj.set_querysign(query_sign);

        const std::vector<nsmocr::recg_region_t>& regions = 
            recg_result.regions;

        for (int i = 0; i < (int)regions.size(); i++)
        {
            std::vector<nsmocr::recg_wordinfo_t> wordinfos = regions[i].wordinfo;
            mocr::MultiSingleCharLine* obj_ret = obj.add_ret();

            for (int j = 0;j < (int)wordinfos.size(); j++)//sinlge char line
            {
                nsmocr::recg_wordinfo_t wordinfo = wordinfos[j];

                /*
                obj_ret->set_words(j, wordinfo.word);
                obj_ret->set_prob(j, CTextFunc::to_string(wordinfo.probability));
                */

                obj_ret->add_words(wordinfo.word);
                obj_ret->add_prob(CTextFunc::to_string(wordinfo.probability));
            }
        }

        Pb2JsonOptions options;
        std::string json_tmp;
        bool ret = ProtoMessageToJson(obj, &json_tmp, options, error);
        if (!ret)
        {
            return ERR_JSON;
        }

        *json_ret = add_errno(json_tmp, err_no);
        if (json_ret->length() <= 0)
        {
            return ERR_JSON;
        }

        return 0;
    }
private:
    static void get_anti_spam_words_json(const nsmocr::recg_result_t& recg_result,
            std::string query_sign, std::string* json_ret, mocr::AntiSpam& obj)
    {
        const std::vector<nsmocr::recg_region_t>& regions = recg_result.regions;
        for (int i = 0; i < (int)regions.size(); i++)
        {
            nsmocr::recg_region_t src = regions[i];
            obj.add_words(src.ocrresult);
        }
    }

    static void get_anti_spam_locations_json(const nsmocr::recg_result_t* result,
            std::string query_sign, std::string* json_ret, mocr::AntiSpam& obj)
    {
        const std::vector<nsmocr::recg_region_t>& regions = result->regions;
        
        //index from middle to 2 sides
        int j = (int)regions.size() / 2;
        for (int i = 0; i < (int)regions.size(); i++)
        {
            if (i % 2 == 0)
            {
                j += i;
            }else
            {
                j -= i;
            }
            nsmocr::recg_region_t src = regions[j];

            // 添加位置扰动 
            int height = src.bottom - src.top + 1;
            int width = src.right - src.left + 1;
            int height_real = height > width ? width : height;

            float normalization = ((float)((height + width) % 10)) / 10;
            int disturb = (int)(0.3 * normalization * height_real);
            if (src.poly_location.size() >= 8 && src.poly_location.size() % 2 == 0)
            {
                mocr::IntPolyLoc* dst_poly = obj.add_poly_location();
                for (int k = 0; k < src.poly_location.size() / 2; k++)
                {
                    mocr::IntPoint* dst_pt = dst_poly->add_points();
                    int point_x = src.poly_location[k * 2];
                    int point_y = src.poly_location[k * 2 + 1];
                    if (point_x > disturb)
                    {
                        point_x -= disturb;
                    }
                    if (point_y > disturb)
                    {
                        point_y -= disturb;
                    }
                    dst_pt->set_x(point_x);
                    dst_pt->set_y(point_y);
                }
            }
            else
            {
                WARNING_LOG("This region is not a quadrangle! \
                        left:%d right:%d top:%d bottom:%d loc_idx:%d ocr_result:%s polysize:%d", 
                        src.left, src.right, src.top, src.bottom, src.loc_idx, 
                        src.ocrresult.c_str(), src.poly_location.size());
            }
        }
    }

    static bool is_url(const std::string result, std::string* p_match_result)
    {
        const char * buf = result.c_str();
        const char* pattern = "(http|https)://((\\w|-)+\\.)+(\\w|-)+(/(\\w| |\\.|/|\\?|%|&|=|-)*)?";
        
        regex_t reg;
        int cflags = REG_EXTENDED;
        regcomp(&reg, pattern, cflags);

        regmatch_t pmatch[1];
        int status = regexec(&reg, buf, 1, pmatch, 0);
        regfree(&reg);

        if (status == 0)
        {
            p_match_result->insert(0, buf + pmatch[0].rm_so, pmatch[0].rm_eo - pmatch[0].rm_so);
            return true;
        }
        else
        {
            return false;
        }
    }

    static bool is_telephone(const std::string result, std::string* p_match_result)
    {
        const char * buf = result.c_str();
        const char * pattern = "1[34578][0-9]{9}";
        
        regex_t reg;
        int cflags = REG_EXTENDED;
        regcomp(&reg, pattern, cflags);

        regmatch_t pmatch[1];
        int status = regexec(&reg, buf, 1, pmatch, 0);
        regfree(&reg);

        if (status == 0)
        {
            p_match_result->insert(0, buf + pmatch[0].rm_so, pmatch[0].rm_eo - pmatch[0].rm_so);
            return true;
        }
        else
        {
            return false;
        }
    }

    static bool is_email(const std::string result, std::string* p_match_result)
    {
        const char * buf = result.c_str();
        const char * pattern = "(\\w|_|-)+@(\\w|_|-)+(\\.(\\w|_|-)+)+";
        
        regex_t reg;
        int cflags = REG_EXTENDED;
        regcomp(&reg, pattern, cflags);

        regmatch_t pmatch[1];
        int status = regexec(&reg, buf, 1, pmatch, 0);
        regfree(&reg);

        if (status == 0)
        {
            p_match_result->insert(0, buf + pmatch[0].rm_so, pmatch[0].rm_eo - pmatch[0].rm_so);
            return true;
        }
        else
        {
            return false;
        }
    }

    static void get_anti_spam_account_json(const nsmocr::recg_result_t& recg_result,
            std::string query_sign, std::string* json_ret, mocr::AntiSpam& obj)
    {
        mocr::AntiSpamSpecificFormat* spec_form = obj.mutable_specific_format();
        const std::vector<nsmocr::recg_region_t>& regions = recg_result.regions;
        for (int i = 0; i < (int)regions.size(); i++)
        {
            nsmocr::recg_region_t src = regions[i];
            std::string match_result;
            if (is_url(src.ocrresult, &match_result))
            {
                spec_form->add_url(match_result);
            }
            else if (is_email(src.ocrresult, &match_result))
            {
                spec_form->add_email(match_result);
            }
            else if (is_telephone(src.ocrresult, &match_result))
            {
                spec_form->add_telephone(match_result);
            }

        }
    }

    static void get_anti_spam_vocab_json(const nsmocr::recg_result_t& recg_result,
            std::string query_sign, std::string* json_ret, mocr::AntiSpam& obj)
    {
        const std::vector<nsmocr::recg_region_t>& regions = recg_result.regions;        
        mocr::AntiSpamVocabMatch* vocab_match = obj.mutable_vocab_match();
            
        std::string total_words = "";
        for (int i = 0; i < (int)regions.size(); i++)
        {
            nsmocr::recg_region_t src = regions[i];
            total_words += src.ocrresult;
        }

        AhoCorasick::category_keywords_map result;
        AhoCorasick::get_instance()->search(total_words, result);
        AhoCorasick::category_keywords_map::iterator it = result.find("adsex");
        if (it != result.end())
        {
            std::vector<std::string> sex_vector = it->second;
            for (int i = 0; i < sex_vector.size(); i++)
            {
                vocab_match->add_adsex(sex_vector[i]);
            }
        }

        it = result.find("ad");
        if (it != result.end())
        {
            std::vector<std::string> ad_vector = it->second;
            for (int i = 0; i < ad_vector.size(); i++)
            {
                vocab_match->add_ad(ad_vector[i]);
            }
        }

        it = result.find("politics");
        if (it != result.end())
        {
            std::vector<std::string> politics_vector = it->second;
            for (int i = 0; i < politics_vector.size(); i++)
            {
                vocab_match->add_politics(politics_vector[i]);
            }
        }

        std::set<std::string> politics_split_set;
        it = result.find("politics_split");
        if (it != result.end())
        {
            std::vector<std::string> tmp_vector = it->second;
            for (int i = 0; i < tmp_vector.size(); i++)
            {
                politics_split_set.insert(tmp_vector[i]);
            }
        }

        for (std::set<std::string>::iterator it = politics_split_set.begin(); 
                it != politics_split_set.end(); it++)
        {
            std::string first = *it;
            std::map<std::string, std::string>::iterator it_map = g_politics_map.find(first);
            if (it_map == g_politics_map.end())
            {
                continue;
            }
            std::string second = it_map->second;
            size_t pos = second.find("&");
            while (pos != std::string::npos)
            {
                std::string second_split_part = second.substr(0, pos);
                std::set<std::string>::iterator it_tmp = politics_split_set.find(second_split_part);
                if (it_tmp != politics_split_set.end())
                {
                    std::string vocab_combine = "";
                    vocab_combine.append(*it).append("&").append(*it_tmp);
                    vocab_match->add_politics(vocab_combine);
                }

                second.erase(0, pos + 1);
                pos = second.find("&");
            }
            std::set<std::string>::iterator it_tmp = politics_split_set.find(second);
            if (it_tmp != politics_split_set.end())
            {
                std::string vocab_combine = "";
                vocab_combine.append(*it).append("&").append(*it_tmp);
                vocab_match->add_politics(vocab_combine);
            }
        }

        std::set<std::string> adsex_split_set;
        it = result.find("adsex_split");
        if (it != result.end())
        {
            std::vector<std::string> ad_vector = it->second;
            for (int i = 0; i < ad_vector.size(); i++)
            {
                adsex_split_set.insert(ad_vector[i]);
            }
        }

        for (std::set<std::string>::iterator it = adsex_split_set.begin(); 
                it != adsex_split_set.end(); it++)
        {
            std::string first = *it;
            std::map<std::string, std::string>::iterator it_map = g_adsex_map.find(first);
            if (it_map == g_adsex_map.end())
            {
                continue;
            }
            std::string second = it_map->second;
            size_t pos = second.find("&");
            while (pos != std::string::npos)
            {
                std::string second_split_part = second.substr(0, pos);
                std::set<std::string>::iterator it_tmp = adsex_split_set.find(second_split_part);
                if (it_tmp != adsex_split_set.end())
                {
                    std::string vocab_combine = "";
                    vocab_combine.append(*it).append("&").append(*it_tmp);
                    vocab_match->add_adsex(vocab_combine);
                }

                second.erase(0, pos + 1);
                pos = second.find("&");
            }
            std::set<std::string>::iterator it_tmp = adsex_split_set.find(second);
            if (it_tmp != adsex_split_set.end())
            {
                std::string vocab_combine = "";
                vocab_combine.append(*it).append("&").append(*it_tmp);
                vocab_match->add_adsex(vocab_combine);
            }
        }
    }
};
};
#endif
