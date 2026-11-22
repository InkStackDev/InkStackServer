#include "ArticleQueryServlet.hpp"

#include "../../Service/ArticleQueryService.hpp"
#include "../../Util/Util.hpp"

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

        int64_t offset = (page_from - 1) * page_size;
        std::vector<ArticleQueryService::ListItem> articles;
        int64_t total = 0;
        if (!ArticleQueryService::ListByUser(user_id, offset, page_size, (int32_t)state, articles, total)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total", total);
        Json::Value list;
        for (const auto& article : articles) {
            Json::Value item;
            item["id"] = article.id;
            item["author"] = article.author;
            item["title"] = article.title;
            item["summary"] = article.summary;
            item["type"] = article.type;
            item["state"] = article.state;
            item["views"] = article.views;
            item["praise"] = article.praise;
            item["favorites"] = article.favorites;
            item["publish_time"] = article.publish_time;
            item["comment_count"] = article.comment_count;
            Json::Value categories(Json::arrayValue);
            for (const auto& category : article.categories) {
                Json::Value c;
                c["id"] = category.id;
                c["name"] = category.name;
                categories.append(c);
            }
            item["categories"] = categories;
            list.append(item);
        }
        result->set("list", list);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
