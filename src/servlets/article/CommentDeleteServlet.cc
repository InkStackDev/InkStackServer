#include "CommentDeleteServlet.hpp"

#include "../../Service/CommentService.hpp"
#include "../../Util/Util.hpp"

CommentDeleteServlet::CommentDeleteServlet()
    : BlogLoginedServlet("CommentDeleteServlet")
{
}

int32_t CommentDeleteServlet::handle(
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

        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");

        auto rt = CommentService::DeleteComment(uid, id);
        if (rt == CommentService::DeleteCommentResult::Ok) {
            result->setErrno(SUCCESS);
        } else if (rt == CommentService::DeleteCommentResult::CommentNotFound) {
            result->setErrno(COMMENT_NOT_FOUND);
        } else if (rt == CommentService::DeleteCommentResult::AccessDenied) {
            result->setErrno(COMMENT_PERMISSION_DENIED);
        } else {
            result->setErrno(COMMENT_DELETE_FAILED);
        }
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
