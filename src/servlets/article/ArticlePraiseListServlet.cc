#include "ArticlePraiseListServlet.hpp"


#include <json/json.h>
#include "../../Repository/ArticleRepository.hpp"
#include "../../Repository/ArticlePraiseRelRepository.hpp"
#include "../../Repository/UserRepository.hpp"

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

        std::vector<DB::ArticlePraiseRelInfo::ptr> results;

        if (article_id > 0) {
            // list users who praised this article
            ArticlePraiseRelRepository::ListByArticle(results, article_id, offset, size, GetDB());
            Json::Value arr(Json::arrayValue);
            for (auto& rel : results) {
                Json::Value item;
                item["id"] = rel->getId();
                item["user_id"] = rel->getUserId();
                item["article_id"] = rel->getArticleId();
                item["create_time"] = rel->getCreateTime();
                auto user = UserRepository::Get(rel->getUserId(), GetDB());
                if (user && !user->getIsDeleted()) {
                    item["user_name"] = user->getName();
                    item["user_avatar"] = user->getAvatar();
                }
                arr.append(item);
            }
            result->set("list", arr);
            result->set("total", ArticlePraiseRelRepository::CountByArticle(article_id, GetDB()));
        } else {
            // list articles praised by this user
            int64_t query_user_id = user_id > 0 ? user_id : uid;
            ArticlePraiseRelRepository::ListByUser(results, query_user_id, offset, size, GetDB());
            Json::Value arr(Json::arrayValue);
            for (auto& rel : results) {
                Json::Value item;
                item["id"] = rel->getId();
                item["user_id"] = rel->getUserId();
                item["article_id"] = rel->getArticleId();
                item["create_time"] = rel->getCreateTime();
                auto article = ArticleRepository::Get(rel->getArticleId(), GetDB());
                if (article && !article->getIsDeleted()) {
                    item["title"] = article->getTitle();
                }
                arr.append(item);
            }
            result->set("list", arr);
            result->set("total", ArticlePraiseRelRepository::CountByUser(query_user_id, GetDB()));
        }

        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
