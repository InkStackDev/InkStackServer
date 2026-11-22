#include "AdminCommentListServlet.hpp"

#include "../../Service/AdminCommentService.hpp"
#include "../../Util/Util.hpp"

#include "../../Domain/CommentTypes.hpp"

#include <json/json.h>

AdminCommentListServlet::AdminCommentListServlet()
    : BlogAdminServlet("AdminCommentListServlet")
{
}

int32_t AdminCommentListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");

        int32_t status = -1;
        std::string status_str = request->getParam("status");
        if (!status_str.empty()) {
            if (status_str == "pending") {
                status = CommentStatus::PENDING;
            } else if (status_str == "approved") {
                status = CommentStatus::APPROVED;
            } else if (status_str == "rejected") {
                status = CommentStatus::REJECTED;
            } else if (status_str == "spam") {
                status = CommentStatus::SPAM;
            }
        }
        std::string keyword = request->getParam("keyword");
        int64_t article_id = request->getParamAs<int64_t>("article_id", 0);
        int32_t is_reported = request->getParamAs<int32_t>("is_reported", -1);

        std::vector<AdminCommentService::Item> comments;
        int64_t total = 0;
        if (!AdminCommentService::List(
                page_num, page_size, status, keyword, article_id, is_reported, comments, total)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value list(Json::arrayValue);
        for (const auto& c : comments) {
            Json::Value item;
            item["id"] = std::to_string(c.id);
            item["content"] = c.content;
            item["parent_id"] = c.parent_id > 0 ? std::to_string(c.parent_id) : "";
            switch (c.status) {
            case CommentStatus::PENDING:
                item["status"] = "pending";
                break;
            case CommentStatus::APPROVED:
                item["status"] = "approved";
                break;
            case CommentStatus::REJECTED:
                item["status"] = "rejected";
                break;
            case CommentStatus::SPAM:
                item["status"] = "spam";
                break;
            default:
                item["status"] = "unknown";
                break;
            }
            item["created_at"] = c.create_time;
            item["updated_at"] = c.update_time;
            item["likes"] = c.likes;
            item["reply_count"] = c.reply_count;
            item["is_reported"] = c.is_reported;
            item["report_count"] = c.report_count;
            item["ip"] = c.ip;
            item["user_agent"] = c.user_agent;

            Json::Value author;
            if (c.author.valid) {
                author["id"] = std::to_string(c.author.id);
                author["name"] = c.author.name;
                author["email"] = c.author.email;
                author["avatar"] = c.author.avatar;
            }
            item["author"] = author;

            Json::Value article;
            if (!c.article_title.empty()) {
                article["id"] = std::to_string(c.article_id);
                article["title"] = c.article_title;
            }
            item["article"] = article;

            list.append(item);
        }

        result->set("total", total);
        result->set("list", list);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
