#include "SiteStatusServlet.hpp"

#include "../../Service/SystemSettingsService.hpp"

#include <Chen/Log/Log.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

SiteStatusServlet::SiteStatusServlet()
    : BlogServlet("SiteStatusServlet")
{
}

int32_t SiteStatusServlet::handle(
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
        result->set("maintenance_mode", settings->getMaintenanceMode() ? true : false);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
