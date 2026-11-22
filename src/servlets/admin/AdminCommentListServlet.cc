#include "AdminCommentListServlet.hpp"

#include "../../Manager/UserManager.hpp"
#include "../../Util/Util.hpp"

#include <json/json.h>
#include "../../Repository/ArticleRepository.hpp"
#include "../../Repository/CommentQueryRepository.hpp"
#include "../../Repository/CommentPraiseRepository.hpp"
#include "../../Domain/CommentTypes.hpp"

AdminCommentListServlet::AdminCommentListServlet()
    : BlogLoginedServlet("AdminCommentListServlet")
{
}

int32_t AdminCommentListServlet::handle(
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
        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        int32_t role = current_user->getRole();
        if (role != UserManager::Role::ADMIN) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        DEFINE_AND_CHECK_TYPE(result, int64_t, page_num, "page_num");
        DEFINE_AND_CHECK_TYPE(result, int64_t, page_size, "page_size");

        std::string status_str = request->getParam("status");
        int32_t status = 0;
        if (!status_str.empty()) {
            if (status_str == "approved") {
                status = CommentStatus::APPROVED;
            } else if (status_str == "pending") {
                status = CommentStatus::PENDING;
            } else if (status_str == "rejected") {
                status = CommentStatus::REJECTED;
            } else if (status_str == "spam") {
                status = CommentStatus::SPAM;
            }
        }
        std::string keyword = request->getParam("keyword");
        int64_t article_id = request->getParamAs<int64_t>("article_id", 0);
        int32_t is_reported = request->getParamAs<int32_t>("is_reported", -1);

        std::vector<DB::CommentInfo::ptr> comments;
        int64_t total = CommentQueryRepository::ListByAdmin(
            comments, page_num, page_size, status, keyword, article_id, is_reported, GetDB());

        Json::Value list(Json::arrayValue);
        for (auto& c : comments) {
            Json::Value item;
            item["id"] = std::to_string(c->getId());
            item["content"] = c->getContent();
            item["parent_id"] = c->getParentId() > 0 ? std::to_string(c->getParentId()) : "";
            item["status"] = [](int32_t s) -> const char* {
                switch (s) {
                case CommentStatus::PENDING:
                    return "pending";
                case CommentStatus::APPROVED:
                    return "approved";
                case CommentStatus::REJECTED:
                    return "rejected";
                case CommentStatus::SPAM:
                    return "spam";
                default:
                    return "unknown";
                }
            }(c->getStatus());
            item["created_at"] = c->getCreateTime();
            item["updated_at"] = c->getUpdateTime();
            item["likes"] = CommentPraiseRepository::CountByComment(c->getId(), GetDB());
            item["reply_count"] = CommentQueryRepository::CountReplies(c->getId(), GetDB());
            item["is_reported"] = c->getIsReported() != 0;
            item["report_count"] = c->getReportCount();
            item["ip"] = c->getIp();
            item["user_agent"] = c->getUserAgent();

            // author info
            Json::Value author;
            auto u = UserMgr::GetInstance()->get(c->getUserId());
            if (u) {
                author["id"] = std::to_string(u->getId());
                author["name"] = u->getName();
                author["email"] = u->getEmail();
                author["avatar"] = u->getAvatar();
            }
            item["author"] = author;

            // article info
            Json::Value article;
            auto a = ArticleRepository::Get(c->getArticleId(), GetDB());
            if (a) {
                article["id"] = std::to_string(a->getId());
                article["title"] = a->getTitle();
            }
            item["article"] = article;

            list.append(item);
        }

        result->set("total", total);
        result->set("list", list);
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
