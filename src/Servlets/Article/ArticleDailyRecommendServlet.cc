#include "ArticleDailyRecommendServlet.hpp"

#include "../../Service/DailyRecommendService.hpp"

ArticleDailyRecommendServlet::ArticleDailyRecommendServlet()
    : BlogServlet("ArticleDailyRecommendServlet")
{
}

int32_t ArticleDailyRecommendServlet::handle(chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        // 未登录返回 0，按全局榜返回，不影响公开访问
        const int64_t user_id = getUserId(request);

        std::vector<DailyRecommendService::Item> items;
        if (!DailyRecommendService::ListDaily(items, user_id)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value list(Json::arrayValue);
        for (const auto& item : items) {
            Json::Value node;
            node["article_id"] = item.article_id;
            node["user_id"] = item.user_id;
            node["author"] = item.author;
            node["author_avatar"] = item.author_avatar;
            node["title"] = item.title;
            node["summary"] = item.summary;
            node["publish_time"] = item.publish_time;
            node["views"] = item.views;
            node["praise"] = item.praise;
            node["favorites"] = item.favorites;
            node["rank_no"] = item.rank_no;
            node["reason"] = item.reason;
            list.append(node);
        }

        result->set("date", DailyRecommendService::Today());
        result->set("total", static_cast<int32_t>(items.size()));
        result->set("articles", list);
        result->setErrno(SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
