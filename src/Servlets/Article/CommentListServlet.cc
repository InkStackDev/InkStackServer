#include "CommentListServlet.hpp"

#include "../../Service/CommentQueryService.hpp"
#include "../../Util/Util.hpp"

#include <Chen/Util/Util.hpp>
#include <json/json.h>

static Json::Value BuildCommentJson(const CommentQueryService::Item& item)
{
    Json::Value out;
    out["id"] = item.id;
    out["content"] = item.content;
    out["user_id"] = item.user_id;
    out["time"] = chen::TimeUtil::Time2StrSec(item.create_time);
    out["user"] = item.user;
    out["avatar"] = item.avatar;
    out["likes"] = item.likes;
    out["is_liked"] = item.is_liked;

    Json::Value replies(Json::arrayValue);
    for (const auto& reply : item.replies) {
        replies.append(BuildCommentJson(reply));
    }
    out["replies"] = replies;
    out["reply_count"] = item.reply_count;
    return out;
}

CommentListServlet::CommentListServlet()
    : BlogServlet("CommentListServlet")
{
}

int32_t CommentListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, article_id, "article_id");

        int64_t uid = getUserId(request);

        std::vector<CommentQueryService::Item> comments;
        int64_t total = 0;
        auto rt = CommentQueryService::ListTree(article_id, uid, comments, total);
        if (rt == CommentQueryService::ListResult::ArticleNotFound) {
            result->setErrno(ARTICLE_NOT_FOUND);
            break;
        }
        if (rt != CommentQueryService::ListResult::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value list(Json::arrayValue);
        for (const auto& comment : comments) {
            list.append(BuildCommentJson(comment));
        }

        result->set("total", total);
        result->set("list", list);
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
