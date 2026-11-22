#include "UserStatsServlet.hpp"

#include "../../Service/UserStatsService.hpp"
#include "../../Util/Util.hpp"

UserStatsServlet::UserStatsServlet()
    : BlogLoginedServlet("UserStatsServlet")
{
}

int32_t UserStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = request->getParamAs<int64_t>("user_id", 0);
        if (!uid) {
            uid = getUserId(request);
        }
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        UserStatsService::Stats stats;
        if (!UserStatsService::GetStats(uid, stats)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->setErrno(SUCCESS);
        result->set("total_articles", stats.total_articles);
        result->set("published_articles", stats.published_articles);
        result->set("private_articles", stats.private_articles);
        result->set("total_views", stats.total_views);
        result->set("total_likes", stats.total_likes);
        result->set("total_comments", stats.total_comments);
        result->set("total_tags", stats.total_tags);
        result->set("total_organizations", stats.total_organizations);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
