#include "ArticleQueryServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Repository/ArticleQueryRepository.hpp"
#include "../../Repository/CategoryRepository.hpp"
#include "../../Repository/ArticleRelRepository.hpp"
#include "../../Repository/CommentQueryRepository.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

ArticleQueryServlet::ArticleQueryServlet()
    : BlogServlet("ArticleQueryServlet")
{
}

int32_t ArticleQueryServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t user_id = request->getParamAs<int64_t>("user_id");
        int64_t page_from = request->getParamAs<int64_t>("page_from");
        int64_t page_size = request->getParamAs<int64_t>("page_size", 6);
        int64_t state = request->getParamAs<int64_t>("state", 0);

        int offset = (page_from - 1) * page_size;
        std::vector<DB::ArticleInfo::ptr> infos;
        auto total = ArticleQueryRepository::ListByUserIdPages(infos, user_id, offset, page_size, true, state, GetDB());
        result->set("total", total);
        Json::Value list;
        for (auto& i : infos) {
            Json::Value item;
            auto uinfo = UserRepository::Get(i->getUserId(), GetDB());
            item["id"] = i->getId();
            item["author"] = uinfo->getName();
            item["title"] = i->getTitle();
            item["summary"] = i->getContent().substr(0, 100);
            item["type"] = i->getType();
            item["state"] = i->getState();
            item["views"] = i->getViews();
            item["praise"] = i->getPraise();
            item["favorites"] = i->getFavorites();
            item["publish_time"] = i->getPublishTime();
            item["comment_count"] = CommentQueryRepository::CountByArticle(i->getId(), GetDB());
            // 查询文章分类
            {
                std::vector<DB::ArticleCategoryRelInfo::ptr> rels;
                ArticleCategoryRelRepository::ListByArticle(rels, i->getId(), true, GetDB());
                Json::Value categories(Json::arrayValue);
                for (auto& rel : rels) {
                    auto cat = CategoryRepository::Get(rel->getCategoryId(), GetDB());
                    if (cat && !cat->getIsDeleted()) {
                        Json::Value c;
                        c["id"] = cat->getId();
                        c["name"] = cat->getName();
                        categories.append(c);
                    }
                }
                item["categories"] = categories;
            }
            list.append(item);
        }
        result->set("list", list);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
