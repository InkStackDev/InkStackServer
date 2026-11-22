#include "ArticleDeleteServlet.hpp"

#include <set>
#include <vector>

#include "../../Service/ArticleService.hpp"
#include "../../Util/Util.hpp"

ArticleDeleteServlet::ArticleDeleteServlet()
    : BlogLoginedServlet("ArticleDeleteServlet")
{
}

int32_t ArticleDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, ids, "ids");
        std::set<int64_t> art_ids;
        auto tmp = chen::StringUtil::Split(ids, ',');
        for (auto& i : tmp) {
            art_ids.insert(chen::TypeUtil::Atoi(i));
        }
        std::vector<int64_t> id_list(art_ids.begin(), art_ids.end());

        int64_t uid = getUserId(request);
        std::vector<int64_t> deleted;
        auto rt = ArticleService::DeleteArticles(uid, id_list, deleted);
        if (rt != ArticleService::DeleteArticlesResult::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }
        if (!deleted.empty()) {
            Json::Value json_ids;
            for (auto id : deleted) {
                json_ids.append(id);
            }
            result->set("ids", json_ids);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
