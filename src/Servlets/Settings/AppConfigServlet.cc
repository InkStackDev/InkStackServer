#include "AppConfigServlet.hpp"

#include <json/json.h>

#include "../../I18n/I18n.hpp"
#include "../../Manager/MessageConfigManager.hpp"

AppConfigServlet::AppConfigServlet()
    : BlogServlet("AppConfigServlet")
{
}

int32_t AppConfigServlet::handle(chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    response->setHeader("Vary", "Accept-Language");

    do {
        const std::string lang = I18n::ResolveLanguage(request);

        if (MessageConfigMgr::GetInstance()->getLanguageCount() == 0) {
            result->setErrno(SETTINGS_NOT_LOADED);
            break;
        }

        Json::Value config = MessageConfigMgr::GetInstance()->getConfig(lang);
        if (!config.isObject() || config.empty()) {
            result->setErrno(SETTINGS_NOT_LOADED);
            break;
        }

        result->setData(config);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
