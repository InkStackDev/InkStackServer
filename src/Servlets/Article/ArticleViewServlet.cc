#include "ArticleViewServlet.hpp"

#include "../../Service/ArticleCounterService.hpp"
#include "../../Service/ArticleQueryService.hpp"
#include "../../Util/Util.hpp"

ArticleViewServlet::ArticleViewServlet()
    : BlogLoginedServlet("ArticleViewServlet")
{
}

int32_t ArticleViewServlet::handle(
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

        ArticleCounterService::IncViews(article_id, std::to_string(uid), uid);
        result->set("views", article->getViews());
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
