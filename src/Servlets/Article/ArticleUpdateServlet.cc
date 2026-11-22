#include "ArticleUpdateServlet.hpp"

#include "../../Service/ArticleService.hpp"
#include "../../Util/Util.hpp"

ArticleUpdateServlet::ArticleUpdateServlet()
    : BlogLoginedServlet("ArticleUpdateServlet")
{
}

int32_t ArticleUpdateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        DEFINE_AND_CHECK_STRING(result, title, "title");
        DEFINE_AND_CHECK_STRING(result, content, "content");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        ArticleService::UpdateArticleInput input;
        input.article_id = id;
        input.user_id = uid;
        input.title = title;
        input.content = content;

        std::string type_str = request->getParam("type");
        if (!type_str.empty()) {
            input.has_type = true;
            input.type = static_cast<int32_t>(chen::TypeUtil::Atoi(type_str));
        }
        std::string channel_str = request->getParam("channel");
        if (!channel_str.empty()) {
            input.has_channel = true;
            input.channel = chen::TypeUtil::Atoi(channel_str);
        }

        std::string category = request->getParam("category");
        if (!category.empty()) {
            input.has_category = true;
            for (auto& s : chen::StringUtil::Split(category, ',')) {
                int64_t cid = chen::TypeUtil::Atoi(s);
                if (cid > 0) {
                    input.category_ids.push_back(cid);
                }
            }
        }
        std::string label = request->getParam("label");
        if (!label.empty()) {
            input.has_label = true;
            for (auto& s : chen::StringUtil::Split(label, ',')) {
                int64_t lid = chen::TypeUtil::Atoi(s);
                if (lid > 0) {
                    input.label_ids.push_back(lid);
                }
            }
        }

        DB::ArticleInfo::ptr info;
        auto rt = ArticleService::UpdateArticle(input, info);
        if (rt == ArticleService::UpdateArticleResult::Ok) {
            result->setErrno(SUCCESS);
        } else if (rt == ArticleService::UpdateArticleResult::NotFound) {
            result->setErrno(ARTICLE_NOT_FOUND);
        } else if (rt == ArticleService::UpdateArticleResult::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
        } else if (rt == ArticleService::UpdateArticleResult::InvalidType) {
            result->setErrno(ARTICLE_INVALID_TYPE);
        } else {
            result->setErrno(ARTICLE_UPDATE_FAILED);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
