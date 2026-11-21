#pragma once

#include <memory>
#include <string>

#include <chen/http/servlet.h>
#include <chen/log/log.h>
#include <json/json.h>

#include "ErrorCodes.hpp"

inline static chen::Logger::ptr g_session_logger = LOG_ROOT();

struct CookieKey {
    static const std::string SESSION_KEY;
};

struct SessionDataKey {
    static const std::string USER_ID;
    static const std::string ANONYMOUS;
};

struct Result {
    typedef std::shared_ptr<Result> ptr;

    Result(int32_t code = SUCCESS, const std::string& msg = "ok");

    template <class T>
    void set(const std::string& key, const T& v);

    template <class T>
    void append(const std::string& key, const T& v);

    void setErrno(int32_t code);

    void setErrno(int32_t code, const std::string& customMsg);
    int32_t getErrno() const { return code_; }

    const std::string& getMessage() const { return message_; }

    void setErrnoFmt(int32_t code, const char* fmt, ...);

    void setData(Json::Value& json_data);

    void setLanguage(const std::string& lang);

    std::string toJsonString() const;

private:
    int32_t code_;
    int64_t used_;
    std::string message_;
    std::string lang_;
    Json::Value jsondata_;
};

template <class T>
void Result::set(const std::string& key, const T& v)
{
    jsondata_[key] = v;
}

template <class T>
void Result::append(const std::string& key, const T& v)
{
    jsondata_[key].append(v);
}
