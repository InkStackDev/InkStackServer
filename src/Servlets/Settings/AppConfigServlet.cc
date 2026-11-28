#include "AppConfigServlet.hpp"

#include <json/json.h>

#include "../../I18n/I18n.hpp"
#include "../../I18n/MessageCatalog.hpp"

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

        if (MessageCatalogRegistry::GetInstance()->getLanguageCount() == 0) {
            result->setErrno(SETTINGS_NOT_LOADED);
            break;
        }

        Json::Value config = MessageCatalogRegistry::GetInstance()->getConfig(lang);
        if (!config.isObject() || config.empty()) {
            result->setErrno(SETTINGS_NOT_LOADED);
            break;
        }

        result->setData(config);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
