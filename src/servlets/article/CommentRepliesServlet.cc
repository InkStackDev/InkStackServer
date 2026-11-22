#include "CommentRepliesServlet.hpp"

#include "../../Manager/UserManager.hpp"
#include "../../Util/Util.hpp"

#include <Chen/Util/Util.hpp>
#include <json/json.h>
#include "../../Repository/CommentRepository.hpp"
#include "../../Repository/CommentQueryRepository.hpp"
#include "../../Repository/CommentPraiseRepository.hpp"

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

        // check parent comment exists
        auto parent = CommentRepository::Get(comment_id, GetDB());
        if (!parent || parent->getIsDeleted()) {
            result->setErrno(COMMENT_NOT_FOUND);
            break;
        }

        int64_t uid = getUserId(request);

        std::vector<DB::CommentInfo::ptr> replies;
        CommentQueryRepository::ListReplies(replies, comment_id, offset, size, GetDB());
        int64_t total = CommentQueryRepository::CountReplies(comment_id, GetDB());

        Json::Value list(Json::arrayValue);
        for (auto& r : replies) {
            Json::Value item;
            item["id"] = r->getId();
            item["content"] = r->getContent();
            item["user_id"] = r->getUserId();
            item["time"] = chen::TimeUtil::Time2StrSec(r->getCreateTime());

            auto user = UserMgr::GetInstance()->get(r->getUserId());
            if (user) {
                item["user"] = user->getName();
                item["avatar"] = user->getAvatar();
            } else {
                item["user"] = "";
                item["avatar"] = "";
            }

            int64_t likes = CommentPraiseRepository::CountByComment(r->getId(), GetDB());
            item["likes"] = likes;
            if (uid > 0) {
                item["is_liked"] = CommentPraiseRepository::IsPraising(uid, r->getId(), GetDB());
            } else {
                item["is_liked"] = false;
            }
            item["reply_count"] = CommentQueryRepository::CountReplies(r->getId(), GetDB());

            list.append(item);
        }

        result->set("total", total);
        result->set("list", list);
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
