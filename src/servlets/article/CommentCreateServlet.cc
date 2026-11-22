#include "CommentCreateServlet.hpp"

#include "../../Event/EventDefine.hpp"
#include "../../Service/CommentService.hpp"
#include "../../Util/Util.hpp"

#include <Chen/Util/Util.hpp>
#include <json/json.h>
#include "../../Repository/ArticleRepository.hpp"
#include "../../Repository/CommentRepository.hpp"
#include "../../Repository/UserRepository.hpp"

CommentCreateServlet::CommentCreateServlet()
    : BlogLoginedServlet("CommentCreateServlet")
{
}

int32_t CommentCreateServlet::handle(
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

        DEFINE_AND_CHECK_TYPE(result, int64_t, article_id, "article_id");
        DEFINE_AND_CHECK_STRING(result, content, "content");

        int64_t parent_id = 0;
        request->checkGetParamAs("parent_id", parent_id);

        std::string ip = session->getRemoteAddressString();
        std::string user_agent = request->getHeader("User-Agent");

        CommentService::CreateCommentInput input;
        input.article_id = article_id;
        input.user_id = uid;
        input.parent_id = parent_id;
        input.content = content;
        input.ip = ip;
        input.user_agent = user_agent;

        DB::CommentInfo::ptr info;
        auto rt = CommentService::CreateComment(input, info);
        if (rt == CommentService::CreateCommentResult::ArticleNotFound) {
            result->setErrno(ARTICLE_NOT_FOUND);
            break;
        }
        if (rt == CommentService::CreateCommentResult::ParentNotFound) {
            result->setErrno(COMMENT_PARENT_NOT_FOUND);
            break;
        }
        if (rt != CommentService::CreateCommentResult::Ok) {
            result->setErrno(COMMENT_CREATE_FAILED);
            break;
        }

        // build response
        Json::Value item;
        item["id"] = info->getId();
        item["content"] = info->getContent();
        item["user_id"] = info->getUserId();
        item["time"] = chen::TimeUtil::Time2StrSec(info->getCreateTime());

        auto user = UserRepository::Get(uid, GetDB());
        if (user) {
            item["user"] = user->getName();
            item["avatar"] = user->getAvatar();
        } else {
            item["user"] = "";
            item["avatar"] = "";
        }

        item["likes"] = 0;
        item["is_liked"] = false;
        item["replies"] = Json::Value(Json::arrayValue);
        item["reply_count"] = 0;

        result->setData(item);
        result->setErrno(SUCCESS);

        // Notify article author and parent comment author asynchronously
        {
            auto article = ArticleRepository::Get(article_id, GetDB());
            auto commenter_info = UserRepository::Get(uid, GetDB());

            EventCommentCreatedData data = {};
            data.commenter_id = uid;
            data.commenter_name = commenter_info ? commenter_info->getName() : "someone";
            data.article_id = article_id;
            data.article_title = article ? article->getTitle() : "";
            data.author_id = article ? article->getUserId() : 0;
            data.parent_comment_id = parent_id;
            if (parent_id > 0) {
                auto parent = CommentRepository::Get(parent_id, GetDB());
                if (parent) {
                    data.parent_comment_author_id = parent->getUserId();
                }
            }

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_COMMENT_CREATED, std::move(data));
        }
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
