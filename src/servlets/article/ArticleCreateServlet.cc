#include "ArticleCreateServlet.hpp"

#include "../../Service/ArticleService.hpp"
#include "../../Util/Util.hpp"

ArticleCreateServlet::ArticleCreateServlet()
    : BlogLoginedServlet("ArticleCreateServlet")
{
}

int32_t ArticleCreateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, title, "title");
        DEFINE_AND_CHECK_STRING(result, content, "content");
        DEFINE_AND_CHECK_TYPE(result, int32_t, type, "type");
        std::string category = request->getParam("category");
        std::string label = request->getParam("label");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        ArticleService::CreateArticleInput input;
        input.user_id = uid;
        input.title = title;
        input.content = content;
        input.type = type;
        for (auto& s : chen::StringUtil::Split(category, ',')) {
            int64_t id = chen::TypeUtil::Atoi(s);
            if (id > 0) {
                input.category_ids.push_back(id);
            }
        }
        for (auto& s : chen::StringUtil::Split(label, ',')) {
            int64_t id = chen::TypeUtil::Atoi(s);
            if (id > 0) {
                input.label_ids.push_back(id);
            }
        }

        DB::ArticleInfo::ptr info;
        auto rt = ArticleService::CreateArticle(input, info);
        if (rt == ArticleService::CreateArticleResult::Ok) {
            result->set("id", info->getId());
        } else if (rt == ArticleService::CreateArticleResult::InvalidType) {
            result->setErrno(ARTICLE_INVALID_TYPE);
        } else {
            result->setErrno(ARTICLE_INSERT_FAILED);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
