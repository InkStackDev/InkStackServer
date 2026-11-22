#include "ArticleUpdateCategoryServlet.hpp"

#include "../../Search/SearchSync.hpp"
#include "../../Service/ArticleCategoryService.hpp"
#include "../../Service/ArticleQueryService.hpp"
#include "../../Util/Util.hpp"

ArticleUpdateCategoryServlet::ArticleUpdateCategoryServlet()
    : BlogLoginedServlet("ArticleUpdateCategoryServlet")
{
}

int32_t ArticleUpdateCategoryServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        std::string adds = request->getParam("add_category_ids");
        std::string dels = request->getParam("del_category_ids");

        if (adds.empty() && dels.empty()) {
            result->setErrno(ARTICLE_CATEGORY_EMPTY);
            break;
        }

        DB::ArticleInfo::ptr ainfo;
        if (!ArticleQueryService::GetArticle(id, ainfo)) {
            result->setErrno(ARTICLE_INVALID_ID);
            break;
        }

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        if (ainfo->getUserId() != uid) {
            result->setErrno(ARTICLE_INVALID_ID);
            break;
        }

        std::vector<int64_t> add_ids;
        if (!adds.empty()) {
            for (auto& i : chen::StringUtil::Split(adds, ',')) {
                if (!i.empty()) {
                    add_ids.push_back(chen::TypeUtil::Atoi(i));
                }
            }
        }
        std::vector<int64_t> del_ids;
        if (!dels.empty()) {
            for (auto& i : chen::StringUtil::Split(dels, ',')) {
                if (!i.empty()) {
                    del_ids.push_back(chen::TypeUtil::Atoi(i));
                }
            }
        }

        std::vector<int64_t> added;
        std::vector<int64_t> removed;
        if (!ArticleCategoryService::UpdateForArticle(id, uid, add_ids, del_ids, added, removed)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        SearchSyncMgr::GetInstance()->markDirty(ainfo->getId(), SearchSync::OP_UPSERT);

        if (!added.empty()) {
            Json::Value add_category_ids(Json::arrayValue);
            for (auto cid : added) {
                add_category_ids.append(cid);
            }
            result->set("add_category_ids", add_category_ids);
        }
        if (!removed.empty()) {
            Json::Value del_category_ids(Json::arrayValue);
            for (auto cid : removed) {
                del_category_ids.append(cid);
            }
            result->set("del_category_ids", del_category_ids);
        }
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
