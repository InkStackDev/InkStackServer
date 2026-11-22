#include "RdStatsServlet.hpp"

#include "../../Service/RdAnalyticsService.hpp"
#include "../../Util/Util.hpp"

RdStatsServlet::RdStatsServlet()
    : BlogLoginedServlet("RdStatsServlet")
{
}

int32_t RdStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t org_id = request->getParamAs<int64_t>("org_id", 0);

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        RdAnalyticsService::Overview overview;
        RdAnalyticsService::Result stats_result = RdAnalyticsService::GetOverview(viewer, org_id, overview);
        if (stats_result == RdAnalyticsService::Result::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (stats_result == RdAnalyticsService::Result::NoAccessibleOrg) {
            result->setErrno(ACCESS_DENIED, "no accessible organization");
            break;
        }
        if (stats_result != RdAnalyticsService::Result::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total_requirements", overview.total_requirements);
        result->set("open_requirements", overview.open_requirements);
        result->set("total_bugs", overview.total_bugs);
        result->set("unresolved_bugs", overview.unresolved_bugs);
        result->set("total_tasks", overview.total_tasks);
        result->set("overdue_tasks", overview.overdue_tasks);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
