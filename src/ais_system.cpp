#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <fstream>

#include <json/json.h>

#include "tools.hpp"
#include "database.hpp"
#include "session.hpp"
#include "httplib.h"

#define REGUSER "insert into reg_userinfo (name, password, email) values(\"%s\", \"%s\", \"%s\")"
#define USERINFO "insert into stu_info (stu_name, stu_school, stu_major, stu_grade, stu_mobile) values(\"%s\", \"%s\", \"%s\", \"%s\", \"%s\")"
#define UPDATE_INTERVIEW_TIME "update stu_info set stu_choice_score=%d, stu_program_score=%d, stu_total_score=%d, stu_interview_time=\"%s\" where stu_id=%d"

#define START_TRANSACTION "start transaction"
#define COMMIT "commit"

class AisSvr
{
    public:
        AisSvr()
        {
            svr_ip_.clear();
            svr_port_ = -1;

            db_ = NULL;
            db_ip_.clear();
            db_user_.clear();
            db_passwd_.clear();
            db_name_.clear();
            db_port_ = -1;
            all_session_ = NULL;
        }

        int OnInit(const std::string& config_filename)
        {
            //原因是：构造函数没有返回值
            //1.加载配置文件
            if(!Load(config_filename))
            {
                LOG(ERROR, "open config file failed") << std::endl;
                return -1;
            }
            LOG(INFO, "open config file success") << std::endl;

            //2.初始化数据库模块
            db_ = new DataBaseSvr(db_ip_, db_user_, db_passwd_, db_name_, db_port_);
            if(!db_)
            {
                LOG(ERROR, "create database failed") << std::endl;
                return -2;
            }

            if(!db_->Connect2Mysql())
            {
                LOG(ERROR, "connect database failed") << std::endl;
                return -3;
            }
            LOG(INFO, "connect database success") << std::endl;

            all_session_ = new AllSessionInfo();
            if(!all_session_)
            {
                LOG(ERROR, "create all session info failed") << std::endl;
                return -4;
            }
            return 0;
        }

        void Start()
        {
            //注册请求， 用户名字，密码， 邮箱
            http_svr_.Post("/register", [this](const httplib::Request& res, httplib::Response& resp){
                    std::unordered_map<std::string, std::string> parm;
                    UrlUtil::PraseBody(res.body, &parm);
                    //1.插入数据库当中
                    //1.1 针对注册的信息， 先插入注册信息表
                    std::string name = parm["name"];
                    std::string password = parm["password"];
                    std::string email = parm["email"];

                    std::string school = parm["school"];
                    std::string major = parm["major"];
                    std::string class_no = parm["class_no"];
                    std::string phone_num = parm["phone_num"];
                    //1.2 组织插入语句
                    Json::Value response_json;
                    Json::FastWriter writer;
                    //开启事务
                    db_->QuerySql(START_TRANSACTION);

                    //创建保存点
                    db_->QuerySql("savepoint aa");
                    char buf[1024] = {0};
                    snprintf(buf, sizeof(buf) - 1, REGUSER, name.c_str(),
                            password.c_str(), email.c_str());
                    //printf("buf:%s\n", buf);
                    bool ret = db_->QuerySql(buf);
                    if(!ret)
                    {
                        //1.第一个就插入失败了
                        //  1.1 结束事务
                        db_->QuerySql(COMMIT);
                        //  1.2 返回应答
                        response_json["is_insert"] = false;
                        resp.body = writer.write(response_json);
                        resp.set_header("Content-Type", "application/json");
                        //  1.3 结束这个函数
                        return;
                    }

                    memset(buf, '\0', sizeof(buf));
                    snprintf(buf, sizeof(buf) - 1, USERINFO, name.c_str(), school.c_str(), major.c_str(), class_no.c_str(), phone_num.c_str());
                    //printf("buf2 : %s\n", buf);
                    ret = db_->QuerySql(buf);
                    if(!ret)
                    {
                        //1.回滚
                        db_->QuerySql("rollback to aa");
                        //2.提交事务
                        db_->QuerySql(COMMIT);
                        //3.组织应答
                        response_json["is_insert"] = false;
                        resp.body = writer.write(response_json);
                        resp.set_header("Content-Type", "application/json");
                        //4.return
                        return;
                    }
                    //提交事务
                    db_->QuerySql(COMMIT);

                    //2.给浏览器响应一个应答， 需要是json格式
                    response_json["is_insert"] = true;
                    resp.body = writer.write(response_json);
                    resp.set_header("Content-Type", "application/json");
            });

            http_svr_.Post("/login", [this](const httplib::Request& res, httplib::Response& resp){
                    //1.解析提交的内容
                    std::unordered_map<std::string, std::string> parm;
                    UrlUtil::PraseBody(res.body, &parm);
                    Json::Value request_json;
                    request_json["email"] = parm["email"];
                    request_json["password"] = parm["password"];

                    //2.校验用户的邮箱和密码
                    //  2.1 如果校验失败， 则给浏览器返回false
                    //  2.2 如果校验成功，执行下面的第3步
                    //具体的操作步骤，需要在注册表当中进行查询，用提交上来的邮箱作为查询的依据
                    //   如果邮箱不存在，则登录失败
                    //   如果邮箱存在
                    //      密码正确，则登录成功
                    //      密码正确，则登录失败
                    Json::Value response_json;
                    bool ret = db_->QueryUserExist(request_json, &response_json);
                    if(!ret)
                    {
                    response_json["login_status"] = false;
                    }
                    else
                    {
                        response_json["login_status"] = true;
                    }

                    //3.前提是在登录正常的情况下
                    //返回sessionid, 用户标识当前用户
                    //3.1 获取指定用户的信息
                    Json::Value user_info;
                    db_->QueryOneStuInfo(response_json["stu_id"].asString(), &user_info);
                    //3.2 生成sessionid
                    Session sess(user_info);
                    std::string sessionid = sess.GetSessinId();
                    all_session_->SetSessionValue(sessionid, sess);

                    std::string tmp = "JSESSIONID=";
                    tmp += sessionid;

                    Json::FastWriter writer;
                    resp.body = writer.write(response_json);

                    //将session放到cookie当中返回给浏览器
                    resp.set_header("Set-Cookie", tmp.c_str());
                    resp.set_header("Content-Type", "application/json");
            });

            http_svr_.Get("/interview", [this](const httplib::Request& res, httplib::Response& resp){
                    //1.要根据请求头部当中的sessinid, 从all_session_，查询到对于的用户信息
                    std::string session_id;
                    GetSessinId(res, &session_id);

                    Session sess;
                    bool ret = all_session_->GetSessionValue(session_id, &sess);
                    if(!ret)
                    {
                    //302 //防止的是直接来访问interview.html页面的情况
                    resp.set_redirect("/index.html");
                    return;
                    }

                    //2.在去查询数据库，获取用户的信息
                    Json::Value response_json;
                    ret = db_->QueryOneStuInfo(sess.user_info_["stu_id"].asString(), &response_json);
                    if(!ret)
                    {
                    return;
                    }

                    //3.组织应答
                    Json::FastWriter writer;
                    resp.body = writer.write(response_json);
                    resp.set_header("Content-Type", "application/json");
            });

            http_svr_.Post("/post_interview", [this](const httplib::Request& res, httplib::Response& resp){
                    //1.从header获取sessionid
                    std::string session_id;
                    GetSessinId(res, &session_id);

                    Session sess;
                    all_session_->GetSessionValue(session_id, &sess);
                    //2.获取正文当中那个提交的信息, 进行切割，获取到value
                    std::unordered_map<std::string, std::string> parm;
                    parm.clear();
                    UrlUtil::PraseBody(res.body, &parm);
                    std::string choice_score = parm["choice_score"];
                    std::string program_score = parm["program_score"];
                    std::string total_score = parm["total_score"];
                    std::string interview_time = parm["interview_time"];
                    //3.组织更新的sql语句
                    char sql[10240] = {0};
                    snprintf(sql, sizeof(sql) - 1, UPDATE_INTERVIEW_TIME, 
                            atoi(choice_score.c_str()), atoi(program_score.c_str()), atoi(total_score.c_str()), interview_time.c_str(), sess.user_info_["stu_id"].asInt());
                    //4.调用执行sql语句的函数
                    Json::Value response_json;
                    bool ret = db_->QuerySql(sql);
                    if(!ret)
                    {
                        response_json["is_modify"] = false;
                    }
                    else
                    {
                        response_json["is_modify"] = true;
                    }
                    //5.组织应答
                    Json::FastWriter writer;
                    resp.body = writer.write(response_json);
                    resp.set_header("Content-Type", "application/json");
            });

            http_svr_.set_mount_point("/", "./www");
            LOG(INFO, "start server... listen ip:") << svr_ip_ << " listen port:" <<
                svr_port_ << std::endl;
            http_svr_.listen(svr_ip_.c_str(), svr_port_);
        }

        void GetSessinId(httplib::Request res, std::string* session_id)
        {
            std::string session = res.get_header_value("Cookie");
            //JSESSIONID=d3060d761b861d9867534637eaa827cb
            std::unordered_map<std::string, std::string> parm;
            UrlUtil::PraseBody(session, &parm);
            *session_id = parm["JSESSIONID"];
        }

        bool Load(const std::string& config_filename)
        {
            std::ifstream file(config_filename.c_str());
            if(!file.is_open())
            {
                LOG(ERROR, "open file failed") << std::endl;
                return false;
            }

            //正常打开文件了
            std::string line;
            std::vector<std::string> output;
            while(std::getline(file, line))
            {
                output.clear();
                StringTools::Split(line, "=", &output);

                //解析内容
                if(strcmp(output[0].c_str(), "svr_ip") == 0)
                {
                    if(output[1].empty())
                    {
                        LOG(ERROR, "ip is empty") << std::endl;
                        return false;
                    }
                    svr_ip_ = output[1];
                }
                else if(strcmp(output[0].c_str(), "svr_port") == 0)
                {
                    if(output[1].empty())
                    {
                        LOG(ERROR, "port is empty") << std::endl;
                        return false;
                    }
                    svr_port_ = atoi(output[1].c_str());
                }
                else if(strcmp(output[0].c_str(), "db_ip") == 0)
                {
                    if(output[1].empty())
                    {
                        LOG(ERROR, "db_ip is empty") << std::endl;
                        return false;
                    }
                    db_ip_ = output[1];
                }
                else if(strcmp(output[0].c_str(), "db_user") == 0)
                {
                    if(output[1].empty())
                    {
                        LOG(ERROR, "db_user is empty") << std::endl;
                        return false;
                    }
                    db_user_ = output[1];
                }
                else if(strcmp(output[0].c_str(), "db_passwd") == 0)
                {
                    if(output[1].empty())
                    {
                        LOG(ERROR, "db_passwd is empty") << std::endl;
                        return false;
                    }
                    db_passwd_ = output[1];
                }
                else if(strcmp(output[0].c_str(), "db_name") == 0)
                {
                    if(output[1].empty())
                    {
                        LOG(ERROR, "db_name is empty") << std::endl;
                        return false;
                    }
                    db_name_ = output[1];
                }
                else if(strcmp(output[0].c_str(), "db_port") == 0)
                {
                    if(output[1].empty())
                    {
                        LOG(ERROR, "db_user is empty") << std::endl;
                        return false;
                    }
                    db_port_ = atoi(output[1].c_str());
                }
            }
            return true;
        }
    private:
        std::string svr_ip_; //服务端侦听的ip地址
        uint16_t svr_port_; //服务端侦听的端口

        DataBaseSvr* db_;
        std::string db_ip_;
        std::string db_user_;
        std::string db_passwd_;
        std::string db_name_;
        uint16_t db_port_;

        httplib::Server http_svr_;

        //所有登录用户对应的会话信息
        AllSessionInfo* all_session_;
};

int main()
{
    AisSvr as;
    int ret = as.OnInit("./config_ais.cfg");
    if(ret < 0)
    {
        LOG(ERROR, "Init server failed") << std::endl;
        return -1;
    }

    as.Start();

    return 0;
}
