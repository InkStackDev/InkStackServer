#include "SessionData.hpp"

#include <cstdarg>

#include "Auth/AuthService.hpp"
#include "I18n/I18n.hpp"
#include "Manager/ErrorCodeManager.hpp"
#include "Metrics/MetricsService.hpp"
#include "Util/Util.hpp"

const std::string CookieKey::kSessionKey = "SESSIONID";
const std::string SessionDataKey::kUserId = "user_id";
const std::string SessionDataKey::kAnonymous = "anonymous";

Result::Result(int32_t code, const std::string& m)
    : code_(code)
    , used_(chen::TimeUtil::GetCurrentUs())
    , message_(m)
    , lang_(I18n::DefaultLanguage())
{
}

void Result::setErrno(int32_t code)
{
    code_ = code;
    message_ = ErrorCodeMgr::GetInstance()->getMessage(code, lang_);
}

void Result::setErrno(int32_t code, const std::string& custom_msg)
{
    code_ = code;
    message_ = I18n::Translate(custom_msg, lang_);
}

void Result::setErrnoFmt(int32_t code, const char* fmt, ...)
{
    code_ = code;
    if (!fmt) {
        message_.clear();
        return;
    }

    const std::string localized = I18n::Translate(fmt, lang_);
    va_list ap;
    va_start(ap, fmt);
    message_ = chen::StringUtil::Formatv(localized.c_str(), ap);
    va_end(ap);
}

void Result::setData(Json::Value& json_data)
{
    jsondata_ = json_data;
}

void Result::setLanguage(const std::string& lang)
{
    this->lang_ = lang;
}

std::string Result::toJsonString() const
{
    Json::Value v;
    v["errno"] = code_;
    v["msg"] = message_;
    v["used"] = ((chen::TimeUtil::GetCurrentUs() - used_) / 1000.0);
    if (!jsondata_.isNull()) {
        v["data"] = jsondata_;
    }
    return chen::JsonUtil::ToString(v);
}
