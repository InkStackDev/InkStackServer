#include "OrganizationAdminStatsServlet.hpp"

#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"

OrganizationAdminStatsServlet::OrganizationAdminStatsServlet()
    : BlogAdminServlet("OrganizationAdminStatsServlet")
{
}

int32_t OrganizationAdminStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        OrganizationService::OrganizationGlobalStats stats;
        if (!OrganizationService::GetGlobalStats(stats)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total_organizations", stats.total);
        result->set("active_organizations", stats.active);
        result->set("inactive_organizations", stats.inactive);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
