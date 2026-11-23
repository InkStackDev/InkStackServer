#include "CommentPraiseServlet.hpp"

#include "../../Event/EventDefine.hpp"
#include "../../Service/CommentService.hpp"
#include "../../Service/UserQueryService.hpp"
#include "../../Util/Util.hpp"

#include <json/json.h>

CommentPraiseServlet::CommentPraiseServlet()
    : BlogLoginedServlet("CommentPraiseServlet")
{
}

int32_t CommentPraiseServlet::handle(
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

        DEFINE_AND_CHECK_TYPE(result, int64_t, comment_id, "comment_id");

        bool was_praising = CommentService::IsPraising(uid, comment_id);
        bool is_praising = false;
        DB::CommentInfo::ptr comment;
        auto rt = CommentService::TogglePraise(uid, comment_id, is_praising, comment);
        if (rt == CommentService::PraiseResult::CommentNotFound) {
            result->setErrno(COMMENT_NOT_FOUND);
            break;
        }
        if (rt != CommentService::PraiseResult::Ok) {
            result->setErrno(was_praising ? COMMENT_UNPRAISE_FAILED : COMMENT_PRAISE_FAILED);
            break;
        }

        result->set("is_praising", is_praising);
        result->set("praise_count", CommentService::CountPraise(comment_id));
        result->setErrno(SUCCESS);

        // Notify comment author (not self-praise)
        if (is_praising && comment && comment->getUserId() != uid) {
            UserQueryService::Brief liker;
            UserQueryService::GetUserBrief(uid, liker);

            EventCommentPraiseData data = {};
            data.comment_author_id = comment->getUserId();
            data.liker_id = uid;
            data.liker_name = liker.name.empty() ? "someone" : liker.name;
            data.article_id = comment->getArticleId();
            data.comment_id = comment_id;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_COMMENT_PRAISE, std::move(data));
        }
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
