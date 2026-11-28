#include "SiteSettingsServlet.hpp"

#include "../../Service/SystemSettingsService.hpp"

#include <Chen/Log/Log.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

SiteSettingsServlet::SiteSettingsServlet()
    : BlogServlet("SiteSettingsServlet")
{
}

int32_t SiteSettingsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        auto settings = SystemSettingsServiceRegistry::GetInstance()->get();
        if (!settings) {
            result->setErrno(SETTINGS_NOT_LOADED);
            break;
        }
        result->set("site_name", settings->getSiteName());
        result->set("site_description", settings->getSiteDescription());
        result->set("site_keywords", settings->getSiteKeywords());
        result->set("site_icon", settings->getSiteIcon());
        result->set("site_logo", settings->getSiteLogo());
        result->set("favicon", settings->getFavicon());
        result->set("timezone", settings->getTimezone());
        result->set("language", settings->getLanguage());
        result->set("enable_registration", settings->getEnableRegistration() ? true : false);
        result->set("maintenance_mode", settings->getMaintenanceMode() ? true : false);
        result->set("allow_comments", settings->getAllowComments() ? true : false);
        result->set("moderate_comments", settings->getModerateComments() ? true : false);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
