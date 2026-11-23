#include "ArticlePraiseServlet.hpp"

#include "../../Event/EventDefine.hpp"
#include "../../Service/ArticleCounterService.hpp"
#include "../../Service/ArticlePraiseRelService.hpp"
#include "../../Service/ArticleQueryService.hpp"
#include "../../Service/UserQueryService.hpp"
#include "../../Util/Util.hpp"

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

        DB::ArticleInfo::ptr article;
        if (!ArticleQueryService::GetArticle(article_id, article) || article->getIsDeleted()) {
            result->setErrno(ARTICLE_NOT_FOUND);
            break;
        }

        bool already_praising = ArticlePraiseRelService::IsPraising(uid, article_id);

        if (already_praising) {
            if (!ArticlePraiseRelService::Unpraise(uid, article_id)) {
                result->setErrno(ARTICLE_UNPRAISE_FAILED);
                break;
            }
            ArticleCounterService::DecPraiseCount(article_id);
            result->set("is_praising", false);
        } else {
            auto info = ArticlePraiseRelService::Praise(uid, article_id);
            if (!info) {
                result->setErrno(ARTICLE_PRAISE_FAILED);
                break;
            }
            ArticleCounterService::IncPraiseCount(article_id);
            result->set("is_praising", true);

            if (article->getUserId() != uid) {
                UserQueryService::Brief liker;
                UserQueryService::GetUserBrief(uid, liker);

                EventArticlePraiseData data = {};
                data.author_id = article->getUserId();
                data.liker_id = uid;
                data.liker_name = liker.name.empty() ? "someone" : liker.name;
                data.article_id = article_id;
                data.article_title = article->getTitle();

                chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ARTICLE_PRAISE, std::move(data));
            }
        }

        result->set("praise_count", (int64_t)article->getPraise());
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
