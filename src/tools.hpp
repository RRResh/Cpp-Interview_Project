#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

#include <boost/algorithm/string.hpp>

class StringTools
{
    public:
        static void Split(const std::string& input,  const std::string& split_char, std::vector<std::string>* output)
        {
            boost::split(*output, input, boost::is_any_of(split_char), boost::token_compress_off);
        }
};

//[时间 日志等级 文件:行号] 具体的日志信息
class LogTime
{
    public:
        static void GetTimeStamp(std::string* timp_stamp)
        {
            time_t sys_time;
            time(&sys_time);

            struct tm* st = localtime(&sys_time);
            char buf[30] = {0};
            snprintf(buf, sizeof(buf) - 1, "%04d-%02d-%02d %02d-%02d-%02d", st->tm_year + 1900, st->tm_mon + 1, st->tm_mday, st->tm_hour, st->tm_min, st->tm_sec);
            timp_stamp->assign(buf, strlen(buf));
        }
};

//划分日志等级
const char* Level[] = {
    "INFO",
    "WARNING",
    "ERROR",
    "FATAL",
    "DEBUG"
};

enum LogLevel
{
    INFO = 0,
    WARNING,
    ERROR,
    FATAL,
    DEBUG
};

inline std::ostream& Log(LogLevel lev, const char* file, int line, const std::string& log_msg)
{
    //获取日志等级
    std::string log_level = Level[lev];
    //获取时间戳
    std::string time_stamp;
    LogTime::GetTimeStamp(&time_stamp);
    //组织输出内容
    std::cout << "[" << time_stamp << " " << log_level << " " << 
        file << ":" << line << "]" << " " << log_msg;
    return std::cout;
}

#define LOG(lev, msg) Log(lev, __FILE__, __LINE__, msg)



class UrlUtil
{
    //1.分割正文提交的数据
    public:
        //email=111222333%40qq.com&password=222222
        //1&2&3&4&5&6  1 2 3 4 5 6
        //key-value map unordered_map
        //eamil=111&password=
        static void PraseBody(const std::string& body, std::unordered_map<std::string, std::string>* param)
        {
            std::vector<std::string> output;
            StringTools::Split(body, "&", &output);
            for(const auto& token:output)
            {
                std::vector<std::string> kv;
                StringTools::Split(token, "=", &kv);
                if(kv.size() != 2)
                {
                    continue;
                }
                (*param)[kv[0]] = UrlDecode(kv[1]);
            }
        }
    private:
        static unsigned char ToHex(unsigned char x) 
        { 
            return  x > 9 ? x + 55 : x + 48; 
        }

        static unsigned char FromHex(unsigned char x) 
        { 
            unsigned char y;
            if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
            else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
            else if (x >= '0' && x <= '9') y = x - '0';
            else assert(0);
            return y;
        }

       static std::string UrlEncode(const std::string& str)
        {
            std::string strTemp = "";
            size_t length = str.length();
            for (size_t i = 0; i < length; i++)
            {
                if (isalnum((unsigned char)str[i]) || 
                        (str[i] == '-') ||
                        (str[i] == '_') || 
                        (str[i] == '.') || 
                        (str[i] == '~'))
                    strTemp += str[i];
                else if (str[i] == ' ')
                    strTemp += "+";
                else
                {
                    strTemp += '%';
                    strTemp += ToHex((unsigned char)str[i] >> 4);
                    strTemp += ToHex((unsigned char)str[i] % 16);
                }
            }
            return strTemp;
        }

        static std::string UrlDecode(const std::string& str)
        {
            std::string strTemp = "";
            size_t length = str.length();
            for (size_t i = 0; i < length; i++)
            {
                if (str[i] == '+') strTemp += ' ';
                else if (str[i] == '%')
                {
                    assert(i + 2 < length);
                    unsigned char high = FromHex((unsigned char)str[++i]);
                    unsigned char low = FromHex((unsigned char)str[++i]);
                    strTemp += high*16 + low;
                }
                else strTemp += str[i];
            }
            return strTemp;
        }
};

