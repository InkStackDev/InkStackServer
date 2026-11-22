#include "ArticlePraiseListServlet.hpp"

#include "../../Service/ArticleQueryService.hpp"
#include "../../Util/Util.hpp"

ArticlePraiseListServlet::ArticlePraiseListServlet()
    : BlogLoginedServlet("ArticlePraiseListServlet")
{
}

int32_t ArticlePraiseListServlet::handle(
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

        int64_t article_id = 0;
        request->checkGetParamAs("article_id", article_id);
        int64_t user_id = 0;
        request->checkGetParamAs("user_id", user_id);
        uint64_t offset = 0;
        request->checkGetParamAs("offset", offset);
        uint64_t size = 20;
        request->checkGetParamAs("size", size);

        if (article_id > 0) {
            std::vector<ArticleQueryService::PraiseUser> users;
            int64_t total = 0;
            if (!ArticleQueryService::ListPraiseUsers(article_id, offset, size, users, total)) {
                result->setErrno(DB_OPERATION_FAILED);
                break;
            }
            Json::Value arr(Json::arrayValue);
            for (const auto& user : users) {
                Json::Value item;
                item["id"] = user.id;
                item["user_id"] = user.user_id;
                item["article_id"] = user.article_id;
                item["create_time"] = user.create_time;
                if (!user.user_name.empty()) {
                    item["user_name"] = user.user_name;
                    item["user_avatar"] = user.user_avatar;
                }
                arr.append(item);
            }
            result->set("list", arr);
            result->set("total", total);
        } else {
            int64_t query_user_id = user_id > 0 ? user_id : uid;
            std::vector<ArticleQueryService::PraiseArticle> articles;
            int64_t total = 0;
            if (!ArticleQueryService::ListPraisedArticles(query_user_id, offset, size, articles, total)) {
                result->setErrno(DB_OPERATION_FAILED);
                break;
            }
            Json::Value arr(Json::arrayValue);
            for (const auto& article : articles) {
                Json::Value item;
                item["id"] = article.id;
                item["user_id"] = article.user_id;
                item["article_id"] = article.article_id;
                item["create_time"] = article.create_time;
                if (!article.title.empty()) {
                    item["title"] = article.title;
                }
                arr.append(item);
            }
            result->set("list", arr);
            result->set("total", total);
        }

        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
