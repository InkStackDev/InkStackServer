#include "ArticlePublishServlet.hpp"

#include "../../Service/ArticleService.hpp"
#include "../../Util/Util.hpp"

ArticlePublishServlet::ArticlePublishServlet()
    : BlogLoginedServlet("ArticlePublishServlet")
{
}

int32_t ArticlePublishServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        DEFINE_AND_CHECK_TYPE(result, int64_t, publish_time, "publish_time");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        auto rt = ArticleService::PublishArticle(uid, id, publish_time);
        if (rt == ArticleService::PublishArticleResult::Ok) {
            result->setErrno(SUCCESS);
        } else if (rt == ArticleService::PublishArticleResult::ArticleNotFound) {
            result->setErrno(ARTICLE_INVALID_ID);
        } else if (rt == ArticleService::PublishArticleResult::InvalidState) {
            result->setErrno(ARTICLE_INVALID_STATE);
        } else {
            result->setErrno(ARTICLE_UPDATE_FAILED);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
