#include "UserAdminStatsServlet.hpp"

#include "../../Service/AdminUserService.hpp"
#include "../../Util/Util.hpp"

UserAdminStatsServlet::UserAdminStatsServlet()
    : BlogAdminServlet("UserAdminStatsServlet")
{
}

int32_t UserAdminStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        AdminUserService::Stats stats;
        if (!AdminUserService::GetStats(stats)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total_users", stats.total);
        result->set("active_users", stats.active);
        result->set("new_users_30d", stats.new_30d);
        result->set("inactive_users", stats.inactive);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
