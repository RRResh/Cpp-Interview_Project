#pragma once
//1.计算sessionid
//2.保存登录用户sessionid和其对应的用户信息
#include <json/json.h>
#include <openssl/md5.h>
#include <pthread.h>
#include <unordered_map>
#include <string>

#include "tools.hpp"

class Session
{
    public:
        Session(Json::Value& user_info)
        {
            origin_str_.clear();
            user_info_ = user_info;

            //计算下原始串 stu_id, stu_name, stu_interview_time
            origin_str_ += std::to_string(user_info_["stu_id"].asInt());
            origin_str_ += user_info_["stu_name"].asString();
            origin_str_ += user_info_["stu_interview_time"].asString();
        }

        Session()
        {}

        ~Session()
        {}

        bool SumMd5()
        {
            //生成32位的md5值，当做sessionid
            //1.定义MD5操作句柄&进行初始化
            MD5_CTX ctx;
            MD5_Init(&ctx);
            //2.计算MD5值
            LOG(INFO, origin_str_) << std::endl;
            int ret = MD5_Update(&ctx, origin_str_.c_str(), origin_str_.size());
            if(ret != 1)
            {
                LOG(ERROR, "MD5_Update failed") << std::endl;
                return false;
            }
            //3.获取计算完成的MD5值
            unsigned char md5[16] = {0};
            ret = MD5_Final(md5, &ctx);
            if(ret != 1)
            {
                LOG(ERROR, "MD5_Final failed") << std::endl;
                return false;
            }
            LOG(INFO, "md5 : ")  << md5 << std::endl;
            
            //32位的字符串就是计算出来的sessionid
            char tmp[2] = {0};
            char buf[32] = {0};
            for(int i = 0; i < 16; i++)
            {
                sprintf(tmp, "%02x", md5[i]);
                strncat(buf, tmp, 2);
            }

            LOG(INFO, buf) << std::endl;
            session_id_ = buf;
            return true;
        }

        std::string& GetSessinId()
        {
            SumMd5();
            return session_id_;
        }
    //private:
        //保存session_id
        std::string session_id_;
        //原始的串，用来生成session_id的
        std::string origin_str_;
        //原始串的内容， stu_id, stu_name, stu_interview_time
        Json::Value user_info_;
};

class AllSessionInfo
{
    public:
        AllSessionInfo()
        {
            session_map_.clear();
            pthread_mutex_init(&map_lock_, NULL);
        }

        ~AllSessionInfo()
        {
            session_map_.clear();
            pthread_mutex_destroy(&map_lock_);
        }

        //Set Session
        bool SetSessionValue(std::string& session_id, Session& session_info)
        {
            pthread_mutex_lock(&map_lock_);
            session_map_.insert(std::make_pair(session_id, session_info));
            pthread_mutex_unlock(&map_lock_);
            return true;
        }
        //Get Session
        bool GetSessionValue(std::string& session_id, Session* session_info)
        {
            pthread_mutex_lock(&map_lock_);
            auto iter = session_map_.find(session_id);
            if(iter == session_map_.end())
            {
                pthread_mutex_unlock(&map_lock_);
                return false;
            }
            *session_info = iter->second;
            pthread_mutex_unlock(&map_lock_);
            return true;
        }
    private:
        //key ：session_id_
        //value : Session
        std::unordered_map<std::string, Session> session_map_;
        pthread_mutex_t map_lock_;
};
