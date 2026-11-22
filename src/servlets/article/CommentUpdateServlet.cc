#include "CommentUpdateServlet.hpp"

#include "../../Service/CommentService.hpp"
#include "../../Service/UserQueryService.hpp"
#include "../../Util/Util.hpp"

#include <json/json.h>

CommentUpdateServlet::CommentUpdateServlet()
    : BlogLoginedServlet("CommentUpdateServlet")
{
}

int32_t CommentUpdateServlet::handle(
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
        DEFINE_AND_CHECK_STRING(result, content, "content");

        DB::CommentInfo::ptr comment;
        auto rt = CommentService::UpdateComment(uid, id, content, comment);
        if (rt == CommentService::UpdateCommentResult::CommentNotFound) {
            result->setErrno(COMMENT_NOT_FOUND);
            break;
        }
        if (rt == CommentService::UpdateCommentResult::AccessDenied) {
            result->setErrno(COMMENT_PERMISSION_DENIED);
            break;
        }
        if (rt != CommentService::UpdateCommentResult::Ok) {
            result->setErrno(COMMENT_UPDATE_FAILED);
            break;
        }

        // build response (same structure as create)
        Json::Value item;
        item["id"] = comment->getId();
        item["content"] = comment->getContent();
        item["user_id"] = comment->getUserId();
        item["time"] = chen::TimeUtil::Time2StrSec(comment->getCreateTime());

        UserQueryService::Brief user;
        if (UserQueryService::GetUserBrief(uid, user)) {
            item["user"] = user.name;
            item["avatar"] = user.avatar;
        } else {
            item["user"] = "";
            item["avatar"] = "";
        }

        int64_t likes = CommentService::CountPraise(id);
        item["likes"] = likes;
        item["is_liked"] = CommentService::IsPraising(uid, id);
        item["replies"] = Json::Value(Json::arrayValue);
        item["reply_count"] = CommentService::CountReplies(id);

        result->setData(item);
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
