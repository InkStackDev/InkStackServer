#include "Events.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/Ds/EventBus.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/FSUtil.hpp>
#include <Chen/Util/JsonUtil.hpp>
#include <json/json.h>

#include "../Client/MailClient.hpp"
#include "../Repository/RecordRepository.hpp"
#include "../Util/Util.hpp"
#include "EventDefine.hpp"
#include "../Domain/OrganizationTypes.hpp"
#include "../Repository/OrganizationUserRelRepository.hpp"
#include "../Domain/ArticleTypes.hpp"
#include "../Repository/CommentRepository.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Service/UserQueryService.hpp"
#include "../Domain/UserTypes.hpp"
#include "../Service/NotificationService.hpp"
#include "../Message/MessageRouter.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

static Json::Value ParseNotificationParams(const std::string& text)
{
    Json::Value params(Json::objectValue);
    if (!text.empty()) {
        chen::JsonUtil::FromString(params, text);
    }
    return params;
}

static void AddNotificationMessageFields(Json::Value& msg, const DB::NotificationInfo::ptr& notif)
{
    msg["title_id"] = notif->getTitleId();
    msg["content_id"] = notif->getContentId();
    msg["params"] = ParseNotificationParams(notif->getParams());

    int64_t sender_id = notif->getSenderId();
    msg["sender_id"] = sender_id;
    UserQueryService::Brief sender;
    UserQueryService::GetUserBrief(sender_id, sender);
    msg["sender_name"] = sender.name;
    msg["sender_avatar"] = sender.avatar;
}

static void EventUserSendCode(const std::any& d)
{
    auto data = std::any_cast<const EventUserSendCodeData&>(d);

    if (!data.email) {
        ERROR(logger) << "email is null";
        return;
    }
    if (data.smtp_host.empty() || data.port == 0) {
        ERROR(logger) << "smtp server not configured";
        return;
    }

    auto error = MailClient::Send(data.smtp_host, data.port, data.use_ssl, data.email);
    if (error != ClientError::Ok) {
        ERROR(logger) << "send email failed, error=" << (int)error << " host=" << data.smtp_host << ":" << data.port;
    }
}

static void EventComment(const std::any& d)
{
    auto data = std::any_cast<const EventCommentData&>(d);

    auto comment = CommentRepository::Get(data.comment_id, GetDB());
    if (!comment) {
        ERROR(logger) << "comment not found, id=" << data.comment_id;
        return;
    }

    auto& type = data.type;
    std::string title_id, content_id;
    if (type == "comment_approved") {
        title_id = "notification.comment.approved.title";
        content_id = "notification.comment.approved.content";
    } else if (type == "comment_admin_deleted") {
        title_id = "notification.comment.admin_deleted.title";
        content_id = "notification.comment.admin_deleted.content";
    } else if (type == "comment_rejected") {
        title_id = "notification.comment.rejected.title";
        content_id = "notification.comment.rejected.content";
    } else if (type == "comment_spam") {
        title_id = "notification.comment.spam.title";
        content_id = "notification.comment.spam.content";
    } else {
        return;
    }

    NotificationData notif_data{};
    notif_data.user_id = data.author_id;
    notif_data.title_id = title_id;
    notif_data.content_id = content_id;
    notif_data.params["comment"] = comment->getContent();
    notif_data.type = type;
    notif_data.sender_id = 0;
    notif_data.article_id = comment->getArticleId();
    notif_data.comment_id = data.comment_id;

    auto notif = NotificationService::AddNotification(notif_data);
    if (notif) {
        Json::Value wsMsg;
        wsMsg["id"] = notif->getId();
        wsMsg["title"] = notif->getTitle();
        wsMsg["content"] = notif->getContent();
        AddNotificationMessageFields(wsMsg, notif);
        wsMsg["type"] = type;
        wsMsg["article_id"] = comment->getArticleId();
        wsMsg["comment_id"] = data.comment_id;
        wsMsg["is_read"] = false;
        wsMsg["create_time"] = notif->getCreateTime();
        SendNotificationToUser(data.author_id, chen::JsonUtil::ToString(wsMsg));
    }
}

static void EventArticleReview(const std::any& d)
{
    auto data = std::any_cast<const EventArticleReviewData&>(d);

    if (data.type == "request") {
        // Notify admins and checkers about new article pending review
        const std::string title_id = "notification.article_review.request.title";
        const std::string content_id = "notification.article_review.request.content";

        std::vector<int64_t> userIds;
        UserRepository::GetAllIds(userIds, true, GetDB());
        for (auto targetId : userIds) {
            auto u = UserRepository::Get(targetId, GetDB());
            if (u && (u->getRole() == UserRole::ADMIN || u->getRole() == UserRole::CHECKER)) {
                NotificationData notif_data{};
                notif_data.user_id = targetId;
                notif_data.title_id = title_id;
                notif_data.content_id = content_id;
                notif_data.params["author"] = data.author_name;
                notif_data.params["title"] = data.article_title;
                notif_data.type = "article_review_request";
                notif_data.sender_id = data.author_id;
                notif_data.article_id = data.article_id;

                auto notif_info = NotificationService::AddNotification(notif_data);
                if (notif_info) {
                    Json::Value wsMsg;
                    wsMsg["id"] = notif_info->getId();
                    wsMsg["title"] = notif_info->getTitle();
                    wsMsg["content"] = notif_info->getContent();
                    AddNotificationMessageFields(wsMsg, notif_info);
                    wsMsg["type"] = "article_review_request";
                    wsMsg["article_id"] = data.article_id;
                    wsMsg["is_read"] = false;
                    wsMsg["create_time"] = notif_info->getCreateTime();
                    SendNotificationToUser(targetId, chen::JsonUtil::ToString(wsMsg));
                }
            }
        }
    } else {
        bool approved = (data.type == "approved");
        const char* notif_type = approved ? "article_review_approved" : "article_review_rejected";
        const std::string title_id =
            approved ? "notification.article_review.approved.title" : "notification.article_review.rejected.title";
        const std::string content_id =
            approved ? "notification.article_review.approved.content" : "notification.article_review.rejected.content";

        NotificationData notif_data{};
        notif_data.user_id = data.author_id;
        notif_data.title_id = title_id;
        notif_data.content_id = content_id;
        notif_data.params["title"] = data.article_title;
        notif_data.type = notif_type;
        notif_data.sender_id = data.reviewer_id;
        notif_data.article_id = data.article_id;

        auto notif_info = NotificationService::AddNotification(notif_data);
        if (notif_info) {
            Json::Value wsMsg;
            wsMsg["id"] = notif_info->getId();
            wsMsg["title"] = notif_info->getTitle();
            wsMsg["content"] = notif_info->getContent();
            AddNotificationMessageFields(wsMsg, notif_info);
            wsMsg["type"] = notif_type;
            wsMsg["article_id"] = data.article_id;
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif_info->getCreateTime();
            SendNotificationToUser(data.author_id, chen::JsonUtil::ToString(wsMsg));
        }

        // Mark other admins/checkers' article_review_request as read
        std::vector<int64_t> userIds;
        UserRepository::GetAllIds(userIds, true, GetDB());
        for (auto targetId : userIds) {
            auto u = UserRepository::Get(targetId, GetDB());
            if (u && (u->getRole() == UserRole::ADMIN || u->getRole() == UserRole::CHECKER)) {
                NotificationService::MarkReadByType(targetId, "article_review_request");
            }
        }
    }
}

static void EventArticleStateChanged(const std::any& d)
{
    auto data = std::any_cast<const EventArticleStateChangedData&>(d);

    const std::string title_id = "notification.article_state_changed.title";
    const std::string content_id = data.new_state == ArticleStatus::PUBLISHED
        ? "notification.article_state_changed.published.content"
        : "notification.article_state_changed.private.content";

    NotificationData notif_data{};
    notif_data.user_id = data.author_id;
    notif_data.title_id = title_id;
    notif_data.content_id = content_id;
    notif_data.params["title"] = data.article_title;
    notif_data.type = "article_state_changed";
    notif_data.sender_id = 0;
    notif_data.article_id = data.article_id;

    auto notif = NotificationService::AddNotification(notif_data);
    if (notif) {
        Json::Value wsMsg;
        wsMsg["id"] = notif->getId();
        wsMsg["title"] = notif->getTitle();
        wsMsg["content"] = notif->getContent();
        AddNotificationMessageFields(wsMsg, notif);
        wsMsg["type"] = "article_state_changed";
        wsMsg["article_id"] = data.article_id;
        wsMsg["is_read"] = false;
        wsMsg["create_time"] = notif->getCreateTime();
        SendNotificationToUser(data.author_id, chen::JsonUtil::ToString(wsMsg));
    }
}

static void EventArticlePraise(const std::any& d)
{
    auto data = std::any_cast<const EventArticlePraiseData&>(d);

    NotificationData notif_data{};
    notif_data.user_id = data.author_id;
    notif_data.title_id = "notification.article_praise.title";
    notif_data.content_id = "notification.article_praise.content";
    notif_data.params["liker"] = data.liker_name;
    notif_data.params["title"] = data.article_title;
    notif_data.type = "praise";
    notif_data.sender_id = data.liker_id;
    notif_data.article_id = data.article_id;

    auto notif_info = NotificationService::AddNotification(notif_data);
    if (notif_info) {
        Json::Value wsMsg;
        wsMsg["id"] = notif_info->getId();
        wsMsg["title"] = notif_info->getTitle();
        wsMsg["content"] = notif_info->getContent();
        AddNotificationMessageFields(wsMsg, notif_info);
        wsMsg["type"] = "praise";
        wsMsg["article_id"] = data.article_id;
        wsMsg["is_read"] = false;
        wsMsg["create_time"] = notif_info->getCreateTime();
        SendNotificationToUser(data.author_id, chen::JsonUtil::ToString(wsMsg));
    }
}

static void EventCommentCreated(const std::any& d)
{
    auto data = std::any_cast<const EventCommentCreatedData&>(d);

    auto send_notify = [&](int64_t target_uid, const std::string& title_id, const std::string& content_id,
                           const std::map<std::string, std::string>& params) {
        if (target_uid == data.commenter_id) {
            return;
        }

        NotificationData notif_data{};
        notif_data.user_id = target_uid;
        notif_data.title_id = title_id;
        notif_data.content_id = content_id;
        notif_data.params = params;
        notif_data.type = "comment";
        notif_data.sender_id = data.commenter_id;
        notif_data.article_id = data.article_id;

        auto notif_info = NotificationService::AddNotification(notif_data);
        if (notif_info) {
            Json::Value wsMsg;
            wsMsg["id"] = notif_info->getId();
            wsMsg["title"] = notif_info->getTitle();
            wsMsg["content"] = notif_info->getContent();
            AddNotificationMessageFields(wsMsg, notif_info);
            wsMsg["type"] = "comment";
            wsMsg["article_id"] = data.article_id;
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif_info->getCreateTime();
            SendNotificationToUser(target_uid, chen::JsonUtil::ToString(wsMsg));
        }
    };

    // notify article author
    send_notify(data.author_id, "notification.article_comment.title", "notification.article_comment.content",
        {{"commenter", data.commenter_name}, {"title", data.article_title}});

    // notify parent comment author on reply
    if (data.parent_comment_id > 0 && data.parent_comment_author_id > 0 && data.parent_comment_author_id != data.author_id) {
        send_notify(data.parent_comment_author_id, "notification.comment_reply.title",
            "notification.comment_reply.content", {{"commenter", data.commenter_name}});
    }
}

static void EventCommentPraise(const std::any& d)
{
    auto data = std::any_cast<const EventCommentPraiseData&>(d);

    NotificationData notif_data{};
    notif_data.user_id = data.comment_author_id;
    notif_data.title_id = "notification.comment_praise.title";
    notif_data.content_id = "notification.comment_praise.content";
    notif_data.params["liker"] = data.liker_name;
    notif_data.type = "comment_praise";
    notif_data.sender_id = data.liker_id;
    notif_data.article_id = data.article_id;
    notif_data.comment_id = data.comment_id;

    auto notif_info = NotificationService::AddNotification(notif_data);
    if (notif_info) {
        Json::Value wsMsg;
        wsMsg["id"] = notif_info->getId();
        wsMsg["title"] = notif_info->getTitle();
        wsMsg["content"] = notif_info->getContent();
        AddNotificationMessageFields(wsMsg, notif_info);
        wsMsg["type"] = "comment_praise";
        wsMsg["article_id"] = data.article_id;
        wsMsg["comment_id"] = data.comment_id;
        wsMsg["is_read"] = false;
        wsMsg["create_time"] = notif_info->getCreateTime();
        SendNotificationToUser(data.comment_author_id, chen::JsonUtil::ToString(wsMsg));
    }
}

static void EventUserFollow(const std::any& d)
{
    auto data = std::any_cast<const EventUserFollowData&>(d);

    NotificationData notif_data{};
    notif_data.user_id = data.following_id;
    notif_data.title_id = "notification.user_follow.title";
    notif_data.content_id = "notification.user_follow.content";
    notif_data.params["follower"] = data.follower_name;
    notif_data.type = "follow";
    notif_data.sender_id = data.follower_id;

    auto notif_info = NotificationService::AddNotification(notif_data);
    if (notif_info) {
        Json::Value wsMsg;
        wsMsg["id"] = notif_info->getId();
        wsMsg["title"] = notif_info->getTitle();
        wsMsg["content"] = notif_info->getContent();
        AddNotificationMessageFields(wsMsg, notif_info);
        wsMsg["type"] = "follow";
        wsMsg["is_read"] = false;
        wsMsg["create_time"] = notif_info->getCreateTime();
        SendNotificationToUser(data.following_id, chen::JsonUtil::ToString(wsMsg));
    }
}

static void EventUserAdminAction(const std::any& d)
{
    auto data = std::any_cast<const EventUserAdminData&>(d);

    std::string title_id, content_id;
    if (data.type == "password_reset") {
        title_id = "notification.password_reset.title";
        content_id = "notification.password_reset.content";
    } else if (data.type == "account_recovered") {
        title_id = "notification.account_recovered.title";
        content_id = "notification.account_recovered.content";
    } else if (data.type == "account_deleted") {
        title_id = "notification.account_deleted.title";
        content_id = "notification.account_deleted.content";
    } else {
        return;
    }

    NotificationData notif_data{};
    notif_data.user_id = data.user_id;
    notif_data.title_id = title_id;
    notif_data.content_id = content_id;
    notif_data.type = data.type;
    notif_data.sender_id = 0;

    auto notif = NotificationService::AddNotification(notif_data);
    if (notif) {
        Json::Value wsMsg;
        wsMsg["id"] = notif->getId();
        wsMsg["title"] = notif->getTitle();
        wsMsg["content"] = notif->getContent();
        AddNotificationMessageFields(wsMsg, notif);
        wsMsg["type"] = data.type;
        wsMsg["is_read"] = false;
        wsMsg["create_time"] = notif->getCreateTime();
        SendNotificationToUser(data.user_id, chen::JsonUtil::ToString(wsMsg));
    }
}

static void EventOrgDeleted(const std::any& d)
{
    auto data = std::any_cast<const EventOrgDeletedData&>(d);

    NotificationData notif_data{};
    notif_data.user_id = data.member_id;
    notif_data.title_id = "notification.org_deleted.title";
    notif_data.content_id = "notification.org_deleted.content";
    notif_data.params["org"] = data.org_name;
    notif_data.type = "org_deleted";
    notif_data.sender_id = 0;

    auto notif = NotificationService::AddNotification(notif_data);
    if (notif) {
        Json::Value wsMsg;
        wsMsg["id"] = notif->getId();
        wsMsg["title"] = notif->getTitle();
        wsMsg["content"] = notif->getContent();
        AddNotificationMessageFields(wsMsg, notif);
        wsMsg["type"] = "org_deleted";
        wsMsg["is_read"] = false;
        wsMsg["create_time"] = notif->getCreateTime();
        SendNotificationToUser(data.member_id, chen::JsonUtil::ToString(wsMsg));
    }
}

static void EventOrgMember(const std::any& d)
{
    auto data = std::any_cast<const EventOrgMemberData&>(d);

    if (data.type == "role_change") {
        const std::string content_id = "notification.org_role_change." + data.new_role_name + ".content";

        NotificationData notif_data{};
        notif_data.user_id = data.target_user_id;
        notif_data.title_id = "notification.org_role_change.title";
        notif_data.content_id = content_id;
        notif_data.params["org"] = data.org_name;
        notif_data.type = "org_role_change";
        notif_data.sender_id = data.operator_id;

        auto notif_info = NotificationService::AddNotification(notif_data);
        if (notif_info) {
            Json::Value wsMsg;
            wsMsg["id"] = notif_info->getId();
            wsMsg["title"] = notif_info->getTitle();
            wsMsg["content"] = notif_info->getContent();
            AddNotificationMessageFields(wsMsg, notif_info);
            wsMsg["type"] = "org_role_change";
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif_info->getCreateTime();
            SendNotificationToUser(data.target_user_id, chen::JsonUtil::ToString(wsMsg));
        }
    } else if (data.type == "kicked") {
        // Notify all org admins
        std::vector<DB::OrganizationUserRelInfo::ptr> members;
        OrganizationUserRelRepository::ListByOrgPages(members, data.org_id, 0, 10000, -1, true, GetDB());
        for (auto& m : members) {
            if (m->getRole() == OrgRole::ORG_ADMIN) {
                NotificationData notif_data{};
                notif_data.user_id = m->getUserId();
                notif_data.title_id = "notification.org_member_kicked.title";
                notif_data.content_id = "notification.org_member_kicked.content";
                notif_data.params["operator"] = data.operator_name;
                notif_data.params["member"] = data.applicant_name;
                notif_data.params["org"] = data.org_name;
                notif_data.type = "org_member_kicked";
                notif_data.sender_id = data.operator_id;

                auto notif_info = NotificationService::AddNotification(notif_data);
                if (notif_info) {
                    Json::Value wsMsg;
                    wsMsg["id"] = notif_info->getId();
                    wsMsg["title"] = notif_info->getTitle();
                    wsMsg["content"] = notif_info->getContent();
                    AddNotificationMessageFields(wsMsg, notif_info);
                    wsMsg["type"] = "org_member_kicked";
                    wsMsg["is_read"] = false;
                    wsMsg["create_time"] = notif_info->getCreateTime();
                    SendNotificationToUser(m->getUserId(), chen::JsonUtil::ToString(wsMsg));
                }
            }
        }
        // Notify kicked user
        {
            NotificationData notif_data{};
            notif_data.user_id = data.kicked_user_id;
            notif_data.title_id = "notification.org_member_kicked_self.title";
            notif_data.content_id = "notification.org_member_kicked_self.content";
            notif_data.params["org"] = data.org_name;
            notif_data.type = "org_member_kicked";
            notif_data.sender_id = data.operator_id;

            auto notif_info = NotificationService::AddNotification(notif_data);
            if (notif_info) {
                Json::Value wsMsg;
                wsMsg["id"] = notif_info->getId();
                wsMsg["title"] = notif_info->getTitle();
                wsMsg["content"] = notif_info->getContent();
                AddNotificationMessageFields(wsMsg, notif_info);
                wsMsg["type"] = "org_member_kicked";
                wsMsg["is_read"] = false;
                wsMsg["create_time"] = notif_info->getCreateTime();
                SendNotificationToUser(data.kicked_user_id, chen::JsonUtil::ToString(wsMsg));
            }
        }
    } else if (data.type == "join_request") {
        auto applicant = UserRepository::Get(data.applicant_id, GetDB());
        std::string applicant_name = applicant ? applicant->getName() : std::to_string(data.applicant_id);
        std::vector<DB::OrganizationUserRelInfo::ptr> members;
        OrganizationUserRelRepository::ListByOrgPages(members, data.org_id, 0, 10000, -1, true, GetDB());
        for (auto& m : members) {
            if (m->getRole() == OrgRole::ORG_ADMIN) {
                NotificationData notif_data{};
                notif_data.user_id = m->getUserId();
                notif_data.title_id = "notification.org_join_request.title";
                notif_data.content_id = "notification.org_join_request.content";
                notif_data.params["user"] = applicant_name;
                notif_data.params["org"] = data.org_name;
                notif_data.type = "org_join_request";
                notif_data.sender_id = data.applicant_id;

                auto notif_info = NotificationService::AddNotification(notif_data);
                if (notif_info) {
                    Json::Value wsMsg;
                    wsMsg["id"] = notif_info->getId();
                    wsMsg["title"] = notif_info->getTitle();
                    wsMsg["content"] = notif_info->getContent();
                    AddNotificationMessageFields(wsMsg, notif_info);
                    wsMsg["type"] = "org_join_request";
                    wsMsg["is_read"] = false;
                    wsMsg["create_time"] = notif_info->getCreateTime();
                    SendNotificationToUser(m->getUserId(), chen::JsonUtil::ToString(wsMsg));
                }
            }
        }
    } else if (data.type == "join_approved" || data.type == "join_rejected") {
        bool approved = (data.type == "join_approved");
        const char* notif_type = approved ? "org_join_approved" : "org_join_rejected";
        const std::string title_id =
            approved ? "notification.org_join_approved.title" : "notification.org_join_rejected.title";
        const std::string content_id =
            approved ? "notification.org_join_approved.content" : "notification.org_join_rejected.content";

        NotificationData notif_data{};
        notif_data.user_id = data.applicant_id;
        notif_data.title_id = title_id;
        notif_data.content_id = content_id;
        notif_data.params["org"] = data.org_name;
        notif_data.type = notif_type;
        notif_data.sender_id = data.operator_id;

        auto notif_info = NotificationService::AddNotification(notif_data);
        if (notif_info) {
            Json::Value wsMsg;
            wsMsg["id"] = notif_info->getId();
            wsMsg["title"] = notif_info->getTitle();
            wsMsg["content"] = notif_info->getContent();
            AddNotificationMessageFields(wsMsg, notif_info);
            wsMsg["type"] = notif_type;
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif_info->getCreateTime();
            SendNotificationToUser(data.applicant_id, chen::JsonUtil::ToString(wsMsg));
        }
        // Mark other admins' org_join_request as read
        std::vector<DB::OrganizationUserRelInfo::ptr> members;
        OrganizationUserRelRepository::ListByOrgPages(members, data.org_id, 0, 10000, -1, true, GetDB());
        for (auto& m : members) {
            if (m->getRole() == OrgRole::ORG_ADMIN) {
                NotificationService::MarkReadByType(m->getUserId(), "org_join_request");
            }
        }
    }
}

static void EventOrgApply(const std::any& d)
{
    auto data = std::any_cast<const EventOrgApplyData&>(d);

    if (data.type == "created") {
        std::vector<DB::UserInfo::ptr> admins;
        UserRepository::ListByPages(admins, 0, 10000, UserRole::ADMIN, -1, -1, true, GetDB());
        for (auto& admin : admins) {
            NotificationData notif_data{};
            notif_data.user_id = admin->getId();
            notif_data.title_id = "notification.org_apply_request.title";
            notif_data.content_id = "notification.org_apply_request.content";
            notif_data.params["user"] = data.applicant_name;
            notif_data.params["org"] = data.org_name;
            notif_data.type = "org_apply_request";
            notif_data.sender_id = 0;
            notif_data.article_id = data.apply_id;

            auto notif = NotificationService::AddNotification(notif_data);
            if (notif) {
                Json::Value wsMsg;
                wsMsg["id"] = notif->getId();
                wsMsg["title"] = notif->getTitle();
                wsMsg["content"] = notif->getContent();
                AddNotificationMessageFields(wsMsg, notif);
                wsMsg["type"] = "org_apply_request";
                wsMsg["apply_id"] = data.apply_id;
                wsMsg["is_read"] = false;
                wsMsg["create_time"] = notif->getCreateTime();
                SendNotificationToUser(admin->getId(), chen::JsonUtil::ToString(wsMsg));
            }
        }
    } else if (data.type == "approved") {
        NotificationData notif_data{};
        notif_data.user_id = data.applicant_id;
        notif_data.title_id = "notification.org_apply_approved.title";
        notif_data.content_id = "notification.org_apply_approved.content";
        notif_data.params["org"] = data.org_name;
        notif_data.type = "org_apply_approved";
        notif_data.sender_id = 0;
        notif_data.article_id = data.org_id;

        auto notif = NotificationService::AddNotification(notif_data);
        if (notif) {
            Json::Value wsMsg;
            wsMsg["id"] = notif->getId();
            wsMsg["title"] = notif->getTitle();
            wsMsg["content"] = notif->getContent();
            AddNotificationMessageFields(wsMsg, notif);
            wsMsg["type"] = "org_apply_approved";
            wsMsg["org_id"] = data.org_id;
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif->getCreateTime();
            SendNotificationToUser(data.applicant_id, chen::JsonUtil::ToString(wsMsg));
        }
    } else if (data.type == "rejected") {
        NotificationData notif_data{};
        notif_data.user_id = data.applicant_id;
        notif_data.title_id = "notification.org_apply_rejected.title";
        notif_data.content_id = data.reason.empty() ? "notification.org_apply_rejected.content"
                                                    : "notification.org_apply_rejected.content_with_reason";
        notif_data.params["org"] = data.org_name;
        notif_data.params["reason"] = data.reason;
        notif_data.type = "org_apply_rejected";
        notif_data.sender_id = 0;

        auto notif = NotificationService::AddNotification(notif_data);
        if (notif) {
            Json::Value wsMsg;
            wsMsg["id"] = notif->getId();
            wsMsg["title"] = notif->getTitle();
            wsMsg["content"] = notif->getContent();
            AddNotificationMessageFields(wsMsg, notif);
            wsMsg["type"] = "org_apply_rejected";
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif->getCreateTime();
            SendNotificationToUser(data.applicant_id, chen::JsonUtil::ToString(wsMsg));
        }
    }
}

static void EventAssignment(const std::any& d)
{
    auto data = std::any_cast<const EventAssignmentData&>(d);

    if (data.type == "deleted") {
        NotificationData notif_data{};
        notif_data.user_id = data.submitter_id;
        notif_data.title_id = "notification.assignment_deleted.title";
        notif_data.content_id = "notification.assignment_deleted.content";
        notif_data.params["assignment"] = data.assignment_name;
        notif_data.type = "assignment_deleted";
        notif_data.sender_id = 0;

        auto notif = NotificationService::AddNotification(notif_data);
        if (notif) {
            Json::Value wsMsg;
            wsMsg["id"] = notif->getId();
            wsMsg["title"] = notif->getTitle();
            wsMsg["content"] = notif->getContent();
            AddNotificationMessageFields(wsMsg, notif);
            wsMsg["type"] = "assignment_deleted";
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif->getCreateTime();
            SendNotificationToUser(data.submitter_id, chen::JsonUtil::ToString(wsMsg));
        }
    } else if (data.type == "created") {
        for (auto uid : data.member_user_ids) {
            NotificationData notif_data{};
            notif_data.user_id = uid;
            notif_data.title_id = "notification.assignment_created.title";
            notif_data.content_id = "notification.assignment_created.content";
            notif_data.params["assignment"] = data.assignment_name;
            notif_data.params["subject"] = data.subject_name;
            notif_data.type = "assignment_created";
            notif_data.sender_id = 0;
            notif_data.article_id = data.assignment_id;

            auto notif = NotificationService::AddNotification(notif_data);
            if (notif) {
                Json::Value wsMsg;
                wsMsg["id"] = notif->getId();
                wsMsg["title"] = notif->getTitle();
                wsMsg["content"] = notif->getContent();
                AddNotificationMessageFields(wsMsg, notif);
                wsMsg["type"] = "assignment_created";
                wsMsg["assignment_id"] = data.assignment_id;
                wsMsg["is_read"] = false;
                wsMsg["create_time"] = notif->getCreateTime();
                SendNotificationToUser(uid, chen::JsonUtil::ToString(wsMsg));
            }
        }
    }
}

static void EventAssignmentSubmitted(const std::any& d)
{
    auto data = std::any_cast<const EventAssignmentSubmittedData&>(d);

    for (auto admin_id : data.admin_user_ids) {
        NotificationData notif_data{};
        notif_data.user_id = admin_id;
        notif_data.title_id = "notification.assignment_submitted.title";
        notif_data.content_id = "notification.assignment_submitted.content";
        notif_data.params["user"] = data.submitter_name;
        notif_data.params["assignment"] = data.assignment_name;
        notif_data.type = "assignment_submitted";
        notif_data.sender_id = 0;
        notif_data.article_id = data.assignment_id;

        auto notif = NotificationService::AddNotification(notif_data);
        if (notif) {
            Json::Value wsMsg;
            wsMsg["id"] = notif->getId();
            wsMsg["title"] = notif->getTitle();
            wsMsg["content"] = notif->getContent();
            AddNotificationMessageFields(wsMsg, notif);
            wsMsg["type"] = "assignment_submitted";
            wsMsg["assignment_id"] = data.assignment_id;
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif->getCreateTime();
            SendNotificationToUser(admin_id, chen::JsonUtil::ToString(wsMsg));
        }
    }
}

static void EventRdAssign(const std::any& d)
{
    auto data = std::any_cast<const EventRdAssignData&>(d);

    std::string notif_type, title_id, content_id;
    if (data.type == "task") {
        notif_type = "task_assigned";
        title_id = "notification.rd_assign.task.title";
        content_id = "notification.rd_assign.task.content";
    } else if (data.type == "requirement") {
        notif_type = "requirement_assigned";
        title_id = "notification.rd_assign.requirement.title";
        content_id = "notification.rd_assign.requirement.content";
    } else if (data.type == "bug") {
        notif_type = "bug_assigned";
        title_id = "notification.rd_assign.bug.title";
        content_id = "notification.rd_assign.bug.content";
    } else {
        return;
    }

    NotificationData notif_data{};
    notif_data.user_id = data.assignee_id;
    notif_data.title_id = title_id;
    notif_data.content_id = content_id;
    notif_data.params["title"] = data.item_title;
    notif_data.type = notif_type;
    notif_data.sender_id = data.creator_id;
    notif_data.article_id = data.item_id;

    auto notif_info = NotificationService::AddNotification(notif_data);
    if (notif_info) {
        Json::Value wsMsg;
        wsMsg["id"] = notif_info->getId();
        wsMsg["title"] = notif_info->getTitle();
        wsMsg["content"] = notif_info->getContent();
        AddNotificationMessageFields(wsMsg, notif_info);
        wsMsg["type"] = notif_type;
        if (data.type == "task") {
            wsMsg["task_id"] = data.item_id;
        } else if (data.type == "requirement") {
            wsMsg["requirement_id"] = data.item_id;
        } else if (data.type == "bug") {
            wsMsg["bug_id"] = data.item_id;
        }
        wsMsg["is_read"] = false;
        wsMsg["create_time"] = notif_info->getCreateTime();
        SendNotificationToUser(data.assignee_id, chen::JsonUtil::ToString(wsMsg));
    }
}

static void EventDatabaseBackup(const std::any& d)
{
    auto data = std::any_cast<const EventDatabaseBackupData&>(d);

    if (data.mysql_dbs.empty()) {
        ERROR(logger) << "mysql config not found for backup " << data.backup_id;
        auto info = BackupRecordRepository::Get(data.backup_id, GetDB());
        if (info) {
            info->setStatus("failed");
            info->setDescription("mysql config not found");
            auto db = GetDB();
            if (db) {
                BackupRecordRepository::Update(info, db);
            }
        }
        return;
    }
    auto& dbcfg = data.mysql_dbs.begin()->second;
    for (const char* key : {"host", "port", "user", "passwd", "dbname"}) {
        if (dbcfg.find(key) == dbcfg.end()) {
            ERROR(logger) << "mysql config missing key=" << key << " for backup " << data.backup_id;
            auto info = BackupRecordRepository::Get(data.backup_id, GetDB());
            if (info) {
                info->setStatus("failed");
                info->setDescription(std::string("mysql config missing: ") + key);
                auto db = GetDB();
                if (db) {
                    BackupRecordRepository::Update(info, db);
                }
            }
            return;
        }
    }
    std::string host = dbcfg.at("host");
    std::string port = dbcfg.at("port");
    std::string user = dbcfg.at("user");
    std::string passwd = dbcfg.at("passwd");
    std::string dbname = dbcfg.at("dbname");

    std::string backup_dir = data.work_path + "/backups";
    chen::FSUtil::Mkdir(backup_dir.c_str());

    std::string filename = data.name + ".sql.gz";
    std::string filepath = backup_dir + "/" + filename;

    int64_t t0 = time(0);
    std::string cmd = "mysqldump -h " + host + " -P " + port + " -u " + user + " -p'" + passwd + "' " + dbname + " 2>/dev/null | gzip > " + filepath;

    INFO(logger) << "Backup " << data.backup_id << " starting, cmd=" << cmd;
    int rc = std::system(cmd.c_str());

    auto info = BackupRecordRepository::Get(data.backup_id, GetDB());
    if (!info) {
        ERROR(logger) << "Backup record " << data.backup_id << " not found after execution";
        return;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Backup " << data.backup_id << " failed: db is null";
        return;
    }

    if (rc != 0) {
        ERROR(logger) << "Backup " << data.backup_id << " failed, rc=" << rc;
        info->setStatus("failed");
        info->setDescription("mysqldump failed with exit code " + std::to_string(rc));
        BackupRecordRepository::Update(info, db);
        return;
    }

    int64_t fileSize = chen::FSUtil::FileSize(filepath);
    int64_t elapsed = time(0) - t0;

    info->setStatus("completed");
    info->setSize(fileSize);
    info->setFileCount(1);
    info->setFilePath("backups/" + filename);
    info->setCompletedAt(time(0));
    info->setDescription("completed in " + std::to_string(elapsed) + "s, size " + std::to_string(fileSize) + " bytes");
    BackupRecordRepository::Update(info, db);

    INFO(logger) << "Backup " << data.backup_id << " completed: " << filepath << " (" << fileSize << " bytes, " << elapsed << "s)";
}

bool EventMsgsInit()
{
    auto bus = chen::EventBusMgr::GetInstance();

    // Register event handlers
    bus->on(EVENT_TYPE_USER_SEND_CODE, EventUserSendCode);
    bus->on(EVENT_TYPE_COMMENT, EventComment);
    bus->on(EVENT_TYPE_ARTICLE_REVIEW, EventArticleReview);
    bus->on(EVENT_TYPE_ARTICLE_STATE_CHANGED, EventArticleStateChanged);
    bus->on(EVENT_TYPE_ARTICLE_PRAISE, EventArticlePraise);
    bus->on(EVENT_TYPE_COMMENT_CREATED, EventCommentCreated);
    bus->on(EVENT_TYPE_COMMENT_PRAISE, EventCommentPraise);
    bus->on(EVENT_TYPE_USER_FOLLOW, EventUserFollow);
    bus->on(EVENT_TYPE_USER_ADMIN, EventUserAdminAction);
    bus->on(EVENT_TYPE_ORG_DELETED, EventOrgDeleted);
    bus->on(EVENT_TYPE_ORG_MEMBER, EventOrgMember);
    bus->on(EVENT_TYPE_ORG_APPLY, EventOrgApply);
    bus->on(EVENT_TYPE_ASSIGNMENT, EventAssignment);
    bus->on(EVENT_TYPE_ASSIGNMENT_SUBMITTED, EventAssignmentSubmitted);
    bus->on(EVENT_TYPE_RD_ASSIGN, EventRdAssign);
    bus->on(EVENT_TYPE_DATABASE_BACKUP, EventDatabaseBackup);

    return true;
};
