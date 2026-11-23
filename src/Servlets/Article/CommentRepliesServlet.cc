#include "CommentRepliesServlet.hpp"

#include "../../Service/CommentQueryService.hpp"
#include "../../Util/Util.hpp"

#include <Chen/Util/Util.hpp>
#include <json/json.h>

CommentRepliesServlet::CommentRepliesServlet()
    : BlogServlet("CommentRepliesServlet")
{
}

int32_t CommentRepliesServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, comment_id, "comment_id");
        int64_t offset = request->getParamAs<int64_t>("offset", 0);
        int64_t size = request->getParamAs<int64_t>("size", 20);
        int64_t uid = getUserId(request);

        std::vector<CommentQueryService::Item> replies;
        int64_t total = 0;
        auto rt = CommentQueryService::ListReplies(comment_id, uid, offset, size, replies, total);
        if (rt == CommentQueryService::ListResult::CommentNotFound) {
            result->setErrno(COMMENT_NOT_FOUND);
            break;
        }
        if (rt != CommentQueryService::ListResult::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value list(Json::arrayValue);
        for (const auto& reply : replies) {
            Json::Value item;
            item["id"] = reply.id;
            item["content"] = reply.content;
            item["user_id"] = reply.user_id;
            item["time"] = chen::TimeUtil::Time2StrSec(reply.create_time);
            item["user"] = reply.user;
            item["avatar"] = reply.avatar;
            item["likes"] = reply.likes;
            item["is_liked"] = reply.is_liked;
            item["reply_count"] = reply.reply_count;
            list.append(item);
        }

        result->set("total", total);
        result->set("list", list);
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
