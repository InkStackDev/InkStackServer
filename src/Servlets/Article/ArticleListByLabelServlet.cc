#include "ArticleListByLabelServlet.hpp"

#include "../../Service/ArticleQueryService.hpp"
#include "../../Util/Util.hpp"

ArticleListByLabelServlet::ArticleListByLabelServlet()
    : BlogServlet("ArticleListByLabelServlet")
{
}

int32_t ArticleListByLabelServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, label_id, "label_id");
        int64_t page_from = request->getParamAs<int64_t>("page_from");
        int64_t page_size = request->getParamAs<int64_t>("page_size", 6);

        int64_t offset = (page_from - 1) * page_size;
        std::vector<ArticleQueryService::ListItem> articles;
        int64_t total = 0;
        if (!ArticleQueryService::ListByLabel(label_id, offset, page_size, articles, total)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total", total);
        Json::Value list;
        for (const auto& article : articles) {
            Json::Value item;
            item["id"] = article.id;
            item["author"] = article.author;
            item["author_avatar"] = article.author_avatar;
            item["title"] = article.title;
            item["summary"] = article.summary;
            item["type"] = article.type;
            item["state"] = article.state;
            item["views"] = article.views;
            item["praise"] = article.praise;
            item["favorites"] = article.favorites;
            item["publish_time"] = article.publish_time;
            list.append(item);
        }
        result->set("list", list);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
