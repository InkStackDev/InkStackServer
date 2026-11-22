#include "ArticleAdminListsServlet.hpp"

#include "../../Util/Util.hpp"
#include "../../Repository/ArticleQueryRepository.hpp"
#include "../../Repository/CommentQueryRepository.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Domain/UserTypes.hpp"

ArticleAdminListsServlet::ArticleAdminListsServlet()
    : BlogLoginedServlet("ArticleAdminListsServlet")
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
        int32_t rrole = request->getParamAs<int32_t>("role", -1);
        int32_t days = request->getParamAs<int32_t>("days", 0);

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        auto current_user = UserRepository::Get(uid, GetDB());
        if (!current_user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        int32_t role = current_user->getRole();
        if (role != UserRole::ADMIN) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        uint64_t offset = (page_num - 1) * page_size;
        std::vector<DB::ArticleInfo::ptr> articles;
        uint64_t total = ArticleQueryRepository::ListByPages(
            articles, offset, state, category, rrole, days, page_size, true, GetDB());

        result->set("total", total);

        Json::Value list;
        for (const auto& atc : articles) {
            const auto& user = UserRepository::Get(atc->getUserId(), GetDB());
            Json::Value item;
            item["id"] = atc->getId();
            item["user_id"] = atc->getUserId();
            if (user) {
                item["author"] = user->getName();
                item["email"] = user->getEmail();
                item["author_role"] = user->getRole();
            }
            item["title"] = atc->getTitle();
            item["state"] = atc->getState();
            item["views"] = atc->getViews();
            item["praise"] = atc->getPraise();
            item["favorites"] = atc->getFavorites();
            item["publish_time"] = atc->getPublishTime();
            item["summary"] = atc->getContent().substr(0, 100);
            item["comment_count"] = CommentQueryRepository::CountByArticle(atc->getId(), GetDB());
            list.append(item);
        }
        result->set("list", list);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
