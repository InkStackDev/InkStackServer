#include "CommentDeleteServlet.hpp"

#include "../../Manager/CommentManager.hpp"
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

        auto comment = CommentMgr::GetInstance()->get(id);
        if (!comment || comment->getIsDeleted()) {
            result->setErrno(COMMENT_NOT_FOUND);
            break;
        }

        // only the comment author can delete
        if (comment->getUserId() != uid) {
            result->setErrno(COMMENT_PERMISSION_DENIED);
            break;
        }

        if (!CommentMgr::GetInstance()->del(id)) {
            result->setErrno(COMMENT_DELETE_FAILED);
            break;
        }

        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
