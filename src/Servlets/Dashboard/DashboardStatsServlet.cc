#include "DashboardStatsServlet.hpp"

#include "../../Service/DashboardService.hpp"
#include "../../Util/Util.hpp"

DashboardStatsServlet::DashboardStatsServlet()
    : BlogLoginedServlet("DashboardStatsServlet")
{
}

int32_t DashboardStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        if (!viewer.is_platform_admin) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        DashboardService::Stats stats;
        if (!DashboardService::GetStats(stats)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total_users", stats.total_users);
        result->set("total_users_change", stats.total_users_change);
        result->set("total_articles", stats.total_articles);
        result->set("total_articles_change", stats.total_articles_change);
        result->set("total_comments", stats.total_comments);
        result->set("total_comments_change", stats.total_comments_change);
        result->set("today_visits", stats.today_visits);
        result->set("today_visits_change", stats.today_visits_change);
        result->set("new_users_today", stats.new_users_today);
        result->set("new_users_today_change", stats.new_users_today_change);
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
