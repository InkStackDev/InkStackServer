#include "ArticleAdminListsServlet.hpp"

#include "../../Service/AdminArticleService.hpp"
#include "../../Util/Util.hpp"

ArticleAdminListsServlet::ArticleAdminListsServlet()
    : BlogAdminServlet("ArticleAdminListsServlet")
{
}

int32_t ArticleAdminListsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");
        int32_t state = request->getParamAs<int32_t>("state", 0);
        int64_t category = request->getParamAs<int64_t>("category_id", 0);
        int32_t role = request->getParamAs<int32_t>("role", -1);
        int32_t days = request->getParamAs<int32_t>("days", 0);

        uint64_t offset = (page_num - 1) * page_size;
        std::vector<AdminArticleService::Item> articles;
        int64_t total = 0;
        if (!AdminArticleService::List(offset, state, category, role, days, page_size, articles, total)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total", total);

        Json::Value list;
        for (const auto& atc : articles) {
            Json::Value item;
            item["id"] = atc.id;
            item["user_id"] = atc.user_id;
            if (atc.has_author) {
                item["author"] = atc.author;
                item["author_avatar"] = atc.author_avatar;
                item["email"] = atc.email;
                item["author_role"] = atc.author_role;
            }
            item["title"] = atc.title;
            item["state"] = atc.state;
            item["views"] = atc.views;
            item["praise"] = atc.praise;
            item["favorites"] = atc.favorites;
            item["publish_time"] = atc.publish_time;
            item["summary"] = atc.summary;
            item["comment_count"] = atc.comment_count;
            list.append(item);
        }
        result->set("list", list);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
