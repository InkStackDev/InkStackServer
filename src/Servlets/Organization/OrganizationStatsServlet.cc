#include "OrganizationStatsServlet.hpp"

#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"

OrganizationStatsServlet::OrganizationStatsServlet()
    : BlogLoginedServlet("OrganizationStatsServlet")
{
}

int32_t OrganizationStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        OrganizationService::OrganizationStatsView stats;
        auto rt = OrganizationService::GetStats(id, stats);
        if (rt == OrganizationService::Result::NotFound) {
            result->setErrno(ORG_NOT_FOUND);
            break;
        }
        if (rt != OrganizationService::Result::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total_members", stats.total_members);
        result->set("active_members", stats.active_members);
        result->set("org_admin_count", stats.org_admin_count);
        result->set("regular_count", stats.regular_count);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
