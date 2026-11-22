#include "AdminCommentStatsServlet.hpp"

#include "../../Service/AdminCommentService.hpp"
#include "../../Util/Util.hpp"

AdminCommentStatsServlet::AdminCommentStatsServlet()
    : BlogAdminServlet("AdminCommentStatsServlet")
{
}

int32_t AdminCommentStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        AdminCommentService::Stats stats;
        if (!AdminCommentService::GetStats(stats)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total_comments", stats.total);
        result->set("pending_comments", stats.pending);
        result->set("approved_comments", stats.approved);
        result->set("spam_comments", stats.spam);
        result->set("reported_comments", stats.reported);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
