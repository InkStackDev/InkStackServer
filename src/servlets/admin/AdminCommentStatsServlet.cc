#include "AdminCommentStatsServlet.hpp"

#include "../../Manager/UserManager.hpp"
#include "../../Repository/CommentQueryRepository.hpp"

AdminCommentStatsServlet::AdminCommentStatsServlet()
    : BlogLoginedServlet("AdminCommentStatsServlet")
{
}

int32_t AdminCommentStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        int32_t role = current_user->getRole();
        if (role != UserManager::Role::ADMIN) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        auto stats = CommentQueryRepository::GetStats();

        result->set("total_comments", stats.total);
        result->set("pending_comments", stats.pending);
        result->set("approved_comments", stats.approved);
        result->set("spam_comments", stats.spam);
        result->set("reported_comments", stats.reported);
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
