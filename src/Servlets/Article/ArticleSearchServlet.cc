#include "ArticleSearchServlet.hpp"

#include "../../Service/ArticleSearchService.hpp"
#include "../../Util/Util.hpp"

#include <Chen/Log/Log.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

ArticleSearchServlet::ArticleSearchServlet()
    : BlogServlet("ArticleSearchServlet")
{
}

int32_t ArticleSearchServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        std::string keyword = request->getParam("keyword");

        int64_t category_id = request->getParamAs<int64_t>("category_id");
        int64_t label_id = request->getParamAs<int64_t>("label_id");
        int64_t channel = request->getParamAs<int64_t>("channel");
        int64_t user_id = request->getParamAs<int64_t>("user_id");
        std::string year_month = request->getParam("year_month");

        int64_t page = request->getParamAs<int64_t>("page", 1);
        int64_t page_size = request->getParamAs<int64_t>("page_size", 20);
        if (page < 1) {
            page = 1;
        }
        if (page_size < 1) {
            page_size = 20;
        }
        if (page_size > 100) {
            page_size = 100;
        }

        tagSearchQuery query;
        query.Keyword = keyword;
        if (category_id > 0) {
            query.CategoryIds.push_back(static_cast<uint64_t>(category_id));
        }
        if (label_id > 0) {
            query.LabelIds.push_back(static_cast<uint64_t>(label_id));
        }
        query.Channel = channel;
        query.UserId = user_id;
        query.YearMonth = year_month;
        query.Page = static_cast<int32_t>(page);
        query.PageSize = static_cast<int32_t>(page_size);

        ArticleSearchService::Result data;
        if (!ArticleSearchService::Search(query, data)) {
            WARN(logger) << "article search unavailable, keyword=" << keyword;
            result->setErrno(ARTICLE_SEARCH_UNAVAILABLE);
            result->set("total", 0);
            result->set("list", Json::Value(Json::arrayValue));
            break;
        }

        result->set("total", data.total);

        Json::Value list(Json::arrayValue);
        for (const auto& item : data.items) {
            Json::Value row;
            row["id"] = item.id;
            row["author"] = item.author;
            row["title"] = item.title;
            row["summary"] = item.summary;
            row["type"] = item.type;
            row["state"] = item.state;
            row["views"] = item.views;
            row["praise"] = item.praise;
            row["favorites"] = item.favorites;
            row["publish_time"] = item.publish_time;
            list.append(row);
        }
        result->set("list", list);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
