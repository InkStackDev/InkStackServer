#include "ArticleDetailServlet.hpp"

#include "../../Service/ArticleQueryService.hpp"
#include "../../Util/Util.hpp"

ArticleDetailServlet::ArticleDetailServlet()
    : BlogServlet("ArticleDetailServlet")
{
}

int32_t ArticleDetailServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        auto type = request->getParamAs<uint32_t>("type");

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        ArticleQueryService::Detail detail;
        auto rt = ArticleQueryService::GetDetail(viewer, id, type, detail);
        if (rt == ArticleQueryService::DetailResult::NotFound) {
            result->setErrno(ARTICLE_INVALID_ID);
            break;
        }
        if (rt == ArticleQueryService::DetailResult::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt != ArticleQueryService::DetailResult::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("id", detail.id);
        result->set("author", detail.author);
        result->set("author_avatar", detail.author_avatar);
        result->set("author_article_count", detail.author_article_count);
        result->set("author_praise_count", detail.author_praise_count);
        result->set("author_follower_count", detail.author_follower_count);
        result->set("title", detail.title);
        result->set("content", detail.content);
        result->set("user_id", detail.user_id);
        result->set("type", detail.type);
        result->set("publish_time", detail.publish_time);
        result->set("update_time", detail.update_time);
        result->set("state", detail.state);
        result->set("is_deleted", detail.is_deleted);
        result->set("views", detail.views);
        result->set("praise", detail.praise);
        result->set("favorites", detail.favorites);

        for (const auto& category : detail.categories) {
            Json::Value item;
            item["id"] = category.id;
            item["name"] = category.name;
            item["color"] = category.color;
            result->append("categorys", item);
        }
        for (const auto& label : detail.labels) {
            Json::Value item;
            item["id"] = label.id;
            item["name"] = label.name;
            item["color"] = label.color;
            result->append("labels", item);
        }
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
