#include "ArticleViewServlet.hpp"

#include "../../Util/Util.hpp"
#include "../../Repository/ArticleRepository.hpp"
#include "../../Service/ArticleCounterService.hpp"

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

        auto article = ArticleRepository::Get(article_id, GetDB());
        if (!article || article->getIsDeleted()) {
            result->setErrno(ARTICLE_NOT_FOUND);
            break;
        }

        std::string cookie_id = std::to_string(uid);
        ArticleCounterService::IncViews(article_id, cookie_id, uid);

        result->set("views", article->getViews());
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
