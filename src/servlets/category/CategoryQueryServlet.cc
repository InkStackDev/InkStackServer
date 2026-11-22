#include "CategoryQueryServlet.hpp"

#include "../../Permission.hpp"

#include <Chen/Log/Log.hpp>
#include "../../Repository/ArticleRepository.hpp"
#include "../../Repository/CategoryRepository.hpp"
#include "../../Repository/ArticleRelRepository.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

CategoryQueryServlet::CategoryQueryServlet()
    : BlogLoginedServlet("CategoryQueryServlet")
{
}

bool CategoryQueryServlet::handlePre(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    if (!BlogLoginedServlet::handlePre(request, response, session, result)) {
        return false;
    }
    if (!isAdminRoute()) {
        return true;
    }
    int64_t uid = getUserId(request);
    DB::UserInfo::ptr user;
    if (uid) {
        user = UserRepository::Get(uid, GetDB());
    }
    int32_t rt = CheckAdminRoute(uid, user);
    if (rt) {
        result->setErrno(rt);
        return false;
    }
    return true;
}

int32_t CategoryQueryServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        int64_t req_uid = request->getParamAs<int64_t>("user_id", 0);

        std::vector<DB::CategoryInfo::ptr> infos;
        if (req_uid) {
            if (!CategoryRepository::ListByUserId(infos, req_uid, true, GetDB())) {
                result->setErrno(DB_OPERATION_FAILED);
                break;
            }
        } else if (isAdminRoute()) {
            CategoryRepository::ListAll(infos, true, GetDB());
        } else if (!CategoryRepository::ListByUserId(infos, uid, true, GetDB())) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total", infos.size());
        Json::Value list;
        for (const auto& cat : infos) {
            Json::Value item;
            item["id"] = cat->getId();
            item["user_id"] = cat->getUserId();
            auto owner = UserRepository::Get(cat->getUserId(), GetDB());
            item["author"] = owner ? owner->getName() : "";
            item["name"] = cat->getName();
            item["url"] = cat->getUrl();
            item["icon"] = cat->getIcon();
            item["color"] = cat->getColor();
            item["description"] = cat->getDescription();
            item["parent_id"] = cat->getParentId();
            item["status"] = cat->getStatus();
            item["create_time"] = cat->getCreateTime();
            item["update_time"] = cat->getUpdateTime();

            int64_t article_count = 0;
            int64_t view_count = 0;
            std::vector<DB::ArticleCategoryRelInfo::ptr> rels;
            ArticleCategoryRelRepository::ListByCategory(rels, cat->getId(), true, GetDB());
            for (const auto& rel : rels) {
                auto article = ArticleRepository::Get(rel->getArticleId(), GetDB());
                if (article) {
                    ++article_count;
                    view_count += article->getViews();
                }
            }
            item["article_count"] = article_count;
            item["view_count"] = view_count;

            list.append(item);
        }
        result->set("list", list);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
