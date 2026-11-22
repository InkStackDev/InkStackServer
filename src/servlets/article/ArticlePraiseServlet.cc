#include "ArticlePraiseServlet.hpp"

#include "../../Event/EventDefine.hpp"
#include "../../Manager/ArticleManager.hpp"
#include "../../Manager/ArticlePraiseRelManager.hpp"
#include "../../Manager/UserManager.hpp"
#include "../../Util/Util.hpp"

#include <json/json.h>

ArticlePraiseServlet::ArticlePraiseServlet()
    : BlogLoginedServlet("ArticlePraiseServlet")
{
}

int32_t ArticlePraiseServlet::handle(
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

        // check article exists
        auto article = ArticleMgr::GetInstance()->get(article_id);
        if (!article || article->getIsDeleted()) {
            result->setErrno(ARTICLE_NOT_FOUND);
            break;
        }

        bool already_praising = ArticlePraiseRelMgr::GetInstance()->isPraising(uid, article_id);

        if (already_praising) {
            // unlike
            if (!ArticlePraiseRelMgr::GetInstance()->unpraise(uid, article_id)) {
                result->setErrno(ARTICLE_UNPRAISE_FAILED);
                break;
            }
            ArticleMgr::GetInstance()->decPraiseCount(article_id);
            result->set("is_praising", false);
        } else {
            // like
            auto info = ArticlePraiseRelMgr::GetInstance()->praise(uid, article_id);
            if (!info) {
                result->setErrno(ARTICLE_PRAISE_FAILED);
                break;
            }
            ArticleMgr::GetInstance()->incPraiseCount(article_id);
            result->set("is_praising", true);

            // Notify article author (not self-praise)
            if (article->getUserId() != uid) {
                auto liker_info = UserMgr::GetInstance()->get(uid);
                // 文章点赞事件
                {
                    EventArticlePraiseData data = {};
                    data.author_id = article->getUserId();
                    data.liker_id = uid;
                    data.liker_name = liker_info ? liker_info->getName() : "someone";
                    data.article_id = article_id;
                    data.article_title = article->getTitle();

                    chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ARTICLE_PRAISE, std::move(data));
                }
            }
        }

        result->set("praise_count", (int64_t)article->getPraise());
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
