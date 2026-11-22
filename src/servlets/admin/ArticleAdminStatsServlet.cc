#include "ArticleAdminStatsServlet.hpp"

#include "../../Service/AdminArticleService.hpp"
#include "../../Util/Util.hpp"

ArticleAdminStatsServlet::ArticleAdminStatsServlet()
    : BlogAdminServlet("ArticleAdminStatsServlet")
{
}

int32_t ArticleAdminStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int32_t category = request->getParamAs<int32_t>("category_id", 0);
        int32_t role = request->getParamAs<int32_t>("role", -1);
        int32_t days = request->getParamAs<int32_t>("days", 0);
        std::string keyword = request->getParam("keyword");

        AdminArticleService::Stats stats;
        if (!AdminArticleService::GetStats(category, role, days, keyword, stats)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total_articles", stats.total);
        result->set("pending_articles", stats.pending);
        result->set("published_articles", stats.published);
        result->set("rejected_articles", stats.rejected);
        result->set("reported_articles", stats.reported);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
