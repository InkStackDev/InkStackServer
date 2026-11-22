#include "ArticleVerifyServlet.hpp"

#include "../../Service/ArticleService.hpp"
#include "../../Util/Util.hpp"

ArticleVerifyServlet::ArticleVerifyServlet()
    : BlogLoginedServlet("ArticleVerifyServlet")
{
}

int32_t ArticleVerifyServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        DEFINE_AND_CHECK_TYPE(result, int64_t, state, "state");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        auto rt = ArticleService::VerifyArticle(uid, id, static_cast<int32_t>(state));
        if (rt == ArticleService::VerifyArticleResult::Ok) {
            result->setErrno(SUCCESS);
        } else if (rt == ArticleService::VerifyArticleResult::ArticleNotFound) {
            result->setErrno(ARTICLE_INVALID_ID);
        } else if (rt == ArticleService::VerifyArticleResult::UserNotFound) {
            result->setErrno(USER_NOT_FOUND);
        } else if (rt == ArticleService::VerifyArticleResult::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
        } else if (rt == ArticleService::VerifyArticleResult::InvalidState) {
            result->setErrno(ARTICLE_INVALID_STATE);
        } else {
            result->setErrno(ARTICLE_UPDATE_FAILED);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
