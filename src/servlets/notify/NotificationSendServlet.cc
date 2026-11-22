#include "NotificationSendServlet.hpp"

#include "../../Util/Util.hpp"

#include <Chen/IOManager/IOManager.hpp>
#include <Chen/Log/Log.hpp>
#include <json/json.h>

#include <vector>
#include "../../Repository/UserRepository.hpp"
#include "../../Domain/UserTypes.hpp"
#include "../../Service/NotificationService.hpp"
#include "../../Message/MessageRouter.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

NotificationSendServlet::NotificationSendServlet()
    : BlogLoginedServlet("NotificationSendServlet")
{
}

int32_t NotificationSendServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        // 权限检查: 仅管理员可发送通知
        int64_t uid = getUserId(request);
        DB::UserInfo::ptr uinfo = UserRepository::Get(uid, GetDB());
        if (!uinfo || uinfo->getRole() != UserRole::ADMIN) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        DEFINE_AND_CHECK_STRING(result, title, "title");
        DEFINE_AND_CHECK_STRING(result, content, "content");
        DEFINE_AND_CHECK_STRING(result, type, "type");
        DEFINE_AND_CHECK_STRING(result, target, "target");

        if (title.size() > 100) {
            result->setErrno(NOTIFICATION_TITLE_TOO_LONG);
            break;
        }
        if (content.size() > 2000) {
            result->setErrno(NOTIFICATION_CONTENT_TOO_LONG);
            break;
        }

        int64_t article_id = 0;
        request->checkGetParamAs("article_id", article_id);
        int64_t comment_id = 0;
        request->checkGetParamAs("comment_id", comment_id);

        // 广播参数
        int32_t is_broadcast = request->getParamAs<int32_t>("is_broadcast", 0);
        std::string level = request->getParam("level");
        int64_t start_time = request->getParamAs<int64_t>("start_time", 0);
        int64_t end_time = request->getParamAs<int64_t>("end_time", 0);

        // 构建通知 JSON
        Json::Value notif;
        notif["type"] = "notification";
        notif["notification_type"] = type;
        notif["title"] = title;
        notif["content"] = content;
        notif["article_id"] = article_id;
        notif["comment_id"] = comment_id;
        notif["create_time"] = (int64_t)time(0);
        if (is_broadcast) {
            notif["is_broadcast"] = 1;
            notif["level"] = level;
            notif["start_time"] = start_time;
            notif["end_time"] = end_time;
        }
        std::string msg = chen::JsonUtil::ToString(notif);

        if (target == "all") {
            std::vector<int64_t> user_ids;
            UserRepository::GetAllIds(user_ids, true, GetDB());

            NotificationData ntf{};
            ntf.title = title;
            ntf.content = content;
            ntf.type = type;
            ntf.sender_id = uid;
            ntf.article_id = article_id;
            ntf.comment_id = comment_id;
            ntf.is_broadcast = is_broadcast;
            ntf.level = level;
            ntf.start_time = start_time;
            ntf.end_time = end_time;

            if (is_broadcast) {
                // 广播只落一条记录（user_id=0），前端 GET /broadcast/list 只显示这一条
                NotificationService::AddNotification(ntf);
            }

            // 给每个用户创建普通通知（is_broadcast=0）并 WS 推送
            chen::IOManager::GetThis()->schedule([ntf, msg, user_ids]() mutable {
                for (auto target_uid : user_ids) {
                    ntf.user_id = target_uid;
                    NotificationService::AddNotification(ntf);
                }
                BroadcastNotification(msg);
            });

            INFO(logger) << "[NOTIFY] broadcast scheduled: type=" << type << " title=" << title << " count=" << user_ids.size();
        } else if (target == "users") {
            std::string user_ids_str = request->getParam("user_ids");
            if (user_ids_str.empty()) {
                result->setErrno(NOTIFICATION_USERS_REQUIRED);
                break;
            }

            std::vector<int64_t> ids;
            for (auto& s : chen::StringUtil::Split(user_ids_str, ',')) {
                auto trimmed = chen::StringUtil::Trim(s);
                if (!trimmed.empty()) {
                    ids.push_back(chen::TypeUtil::Atoi(trimmed));
                }
            }

            int32_t sent = 0;
            for (auto id : ids) {
                NotificationData ntf{};
                ntf.user_id = id;
                ntf.title = title;
                ntf.content = content;
                ntf.type = type;
                ntf.sender_id = uid;
                ntf.article_id = article_id;
                ntf.comment_id = comment_id;

                NotificationService::AddNotification(ntf);
                if (SendNotificationToUser(id, msg) == 0) {
                    sent++;
                }
            }
            INFO(logger) << "[NOTIFY] send to users: count=" << ids.size() << " sent=" << sent << " type=" << type << " title=" << title;
        } else {
            result->setErrno(NOTIFICATION_INVALID_TARGET);
            break;
        }

        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
