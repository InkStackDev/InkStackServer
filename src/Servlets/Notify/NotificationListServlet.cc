#include "NotificationListServlet.hpp"

#include "../../Service/NotificationService.hpp"
#include "../../Service/UserQueryService.hpp"
#include "../../Util/Util.hpp"

#include <json/json.h>

#include <Chen/Util/JsonUtil.hpp>

NotificationListServlet::NotificationListServlet()
    : BlogLoginedServlet("NotificationListServlet")
{
}

int32_t NotificationListServlet::handle(
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

        int32_t offset = request->getParamAs<int32_t>("offset", 0);
        int32_t size = request->getParamAs<int32_t>("size", 20);
        std::string type = request->getParam("type");

        std::vector<DB::NotificationInfo::ptr> notifications;
        if (!NotificationService::ListByUser(notifications, uid, offset, size, type)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value arr(Json::arrayValue);
        for (const auto& n : notifications) {
            Json::Value item;
            item["id"] = n->getId();
            item["title"] = n->getTitle();
            item["content"] = n->getContent();
            item["title_id"] = n->getTitleId();
            item["content_id"] = n->getContentId();
            Json::Value params(Json::objectValue);
            if (!n->getParams().empty()) {
                chen::JsonUtil::FromString(params, n->getParams());
            }
            item["params"] = params;
            item["type"] = n->getType();

            int64_t sender_id = n->getSenderId();
            item["sender_id"] = sender_id;

            UserQueryService::Brief sender;
            UserQueryService::GetUserBrief(sender_id, sender);
            item["sender_name"] = sender.name;
            item["sender_avatar"] = sender.avatar;

            const std::string& item_type = n->getType();
            int64_t target_id = n->getArticleId();
            if (item_type == "bug_assigned") {
                item["bug_id"] = target_id;
            } else if (item_type == "task_assigned") {
                item["task_id"] = target_id;
            } else if (item_type == "requirement_assigned") {
                item["requirement_id"] = target_id;
            } else if (item_type == "assignment_submitted" || item_type == "assignment_created") {
                item["assignment_id"] = target_id;
            } else if (item_type == "org_apply_approved") {
                item["org_id"] = target_id;
            } else if (item_type == "org_apply_request") {
                item["apply_id"] = target_id;
            } else {
                item["article_id"] = target_id;
                item["comment_id"] = n->getCommentId();
            }

            item["is_read"] = n->getIsRead();
            item["create_time"] = n->getCreateTime();
            arr.append(item);
        }

        result->setErrno(SUCCESS);
        result->set("list", arr);
        result->set("total", NotificationService::CountByUser(uid, type));
        result->set("offset", offset);
        result->set("size", (int32_t)notifications.size());
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
