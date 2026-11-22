#include "ArticleIsPraisingServlet.hpp"

#include "../../Manager/ArticlePraiseRelManager.hpp"
#include "../../Util/Util.hpp"

ArticleIsPraisingServlet::ArticleIsPraisingServlet()
    : BlogLoginedServlet("ArticleIsPraisingServlet")
{
}

int32_t ArticleIsPraisingServlet::handle(
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

        bool praising = ArticlePraiseRelMgr::GetInstance()->isPraising(uid, article_id);

        result->setErrno(SUCCESS);
        result->set("is_praising", praising);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
