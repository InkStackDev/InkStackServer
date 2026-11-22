#include "DatabaseStatsServlet.hpp"

#include "../../Service/DatabaseAdminService.hpp"
#include "../../Util/Util.hpp"

DatabaseStatsServlet::DatabaseStatsServlet()
    : BlogAdminServlet("DatabaseStatsServlet")
{
}

int32_t DatabaseStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DatabaseAdminService::Stats stats;
        if (!DatabaseAdminService::GetStats(stats)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        auto du = GetDiskUsage();
        result->set("total_size", du.total_size);
        result->set("used_size", du.used_size);
        result->set("available_size", du.available_size);
        result->set("total_records", stats.articles + stats.users + stats.comments + stats.categories + stats.labels);
        result->set("articles", stats.articles);
        result->set("users", stats.users);
        result->set("comments", stats.comments);
        result->set("categories", stats.categories);
        result->set("tags", stats.labels);
        result->set("backups", stats.backups);
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
