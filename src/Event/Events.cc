#include "Events.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/Ds/EventBus.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/FSUtil.hpp>
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
#include "../Domain/UserTypes.hpp"
#include "../Service/NotificationService.hpp"
#include "../Message/MessageRouter.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

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
    std::string title, content;
    if (type == "comment_approved") {
        title = "Comment approved";
        content = "Your comment \"" + comment->getContent() + "\" has been approved.";
    } else if (type == "comment_admin_deleted") {
        title = "Comment deleted";
        content = "Your comment \"" + comment->getContent() + "\" has been deleted by an administrator.";
    } else if (type == "comment_rejected") {
        title = "Comment rejected";
        content = "Your comment \"" + comment->getContent() + "\" was not approved.";
    } else if (type == "comment_spam") {
        title = "Comment marked as spam";
        content = "Your comment \"" + comment->getContent() + "\" has been marked as spam.";
    } else {
        return;
    }

    NotificationData notif_data{};
    notif_data.user_id = data.author_id;
    notif_data.title = title;
    notif_data.content = content;
    notif_data.type = type;
    notif_data.sender_id = 0;
    notif_data.article_id = comment->getArticleId();
    notif_data.comment_id = data.comment_id;

    auto notif = NotificationService::AddNotification(notif_data);
    if (notif) {
        Json::Value wsMsg;
        wsMsg["id"] = notif->getId();
        wsMsg["title"] = title;
        wsMsg["content"] = content;
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
        std::string title = "New article pending review";
        std::string content = data.author_name + " submitted article \"" + data.article_title + "\" for review.";

        std::vector<int64_t> userIds;
        UserRepository::GetAllIds(userIds, true, GetDB());
        for (auto targetId : userIds) {
            auto u = UserRepository::Get(targetId, GetDB());
            if (u && (u->getRole() == UserRole::ADMIN || u->getRole() == UserRole::CHECKER)) {
                NotificationData notif_data{};
                notif_data.user_id = targetId;
                notif_data.title = title;
                notif_data.content = content;
                notif_data.type = "article_review_request";
                notif_data.sender_id = data.author_id;
                notif_data.article_id = data.article_id;

                auto notif_info = NotificationService::AddNotification(notif_data);
                if (notif_info) {
                    Json::Value wsMsg;
                    wsMsg["id"] = notif_info->getId();
                    wsMsg["title"] = title;
                    wsMsg["content"] = content;
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
        std::string title = approved ? "Article approved" : "Article rejected";
        std::string content =
            "Your article \"" + data.article_title + "\" " + (approved ? "has been approved." : "was not approved.");

        NotificationData notif_data{};
        notif_data.user_id = data.author_id;
        notif_data.title = title;
        notif_data.content = content;
        notif_data.type = notif_type;
        notif_data.sender_id = data.reviewer_id;
        notif_data.article_id = data.article_id;

        auto notif_info = NotificationService::AddNotification(notif_data);
        if (notif_info) {
            Json::Value wsMsg;
            wsMsg["id"] = notif_info->getId();
            wsMsg["title"] = title;
            wsMsg["content"] = content;
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

    std::string title = "Article status changed";
    std::string content = "Your article \"" + data.article_title
        + "\" status has been changed by an administrator to \""
        + (data.new_state == ArticleStatus::PUBLISHED ? "Published" : "Private") + "\".";

    NotificationData notif_data{};
    notif_data.user_id = data.author_id;
    notif_data.title = title;
    notif_data.content = content;
    notif_data.type = "article_state_changed";
    notif_data.sender_id = 0;
    notif_data.article_id = data.article_id;

    auto notif = NotificationService::AddNotification(notif_data);
    if (notif) {
        Json::Value wsMsg;
        wsMsg["id"] = notif->getId();
        wsMsg["title"] = title;
        wsMsg["content"] = content;
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

    std::string notify_title = "Article liked";
    std::string notify_content = data.liker_name + " liked your article \"" + data.article_title + "\".";

    NotificationData notif_data{};
    notif_data.user_id = data.author_id;
    notif_data.title = notify_title;
    notif_data.content = notify_content;
    notif_data.type = "praise";
    notif_data.sender_id = data.liker_id;
    notif_data.article_id = data.article_id;

    auto notif_info = NotificationService::AddNotification(notif_data);
    if (notif_info) {
        Json::Value wsMsg;
        wsMsg["id"] = notif_info->getId();
        wsMsg["title"] = notify_title;
        wsMsg["content"] = notify_content;
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

    auto send_notify = [&](int64_t target_uid, const std::string& title, const std::string& content) {
        if (target_uid == data.commenter_id) {
            return;
        }

        NotificationData notif_data{};
        notif_data.user_id = target_uid;
        notif_data.title = title;
        notif_data.content = content;
        notif_data.type = "comment";
        notif_data.sender_id = data.commenter_id;
        notif_data.article_id = data.article_id;

        auto notif_info = NotificationService::AddNotification(notif_data);
        if (notif_info) {
            Json::Value wsMsg;
            wsMsg["id"] = notif_info->getId();
            wsMsg["title"] = title;
            wsMsg["content"] = content;
            wsMsg["type"] = "comment";
            wsMsg["article_id"] = data.article_id;
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif_info->getCreateTime();
            SendNotificationToUser(target_uid, chen::JsonUtil::ToString(wsMsg));
        }
    };

    // notify article author
    send_notify(data.author_id, "Article comment",
        data.commenter_name + " commented on your article \"" + data.article_title + "\".");

    // notify parent comment author on reply
    if (data.parent_comment_id > 0 && data.parent_comment_author_id > 0 && data.parent_comment_author_id != data.author_id) {
        send_notify(data.parent_comment_author_id, "Comment reply", data.commenter_name + " replied to your comment.");
    }
}

static void EventCommentPraise(const std::any& d)
{
    auto data = std::any_cast<const EventCommentPraiseData&>(d);

    std::string notify_title = "Comment liked";
    std::string notify_content = data.liker_name + " liked your comment.";

    NotificationData notif_data{};
    notif_data.user_id = data.comment_author_id;
    notif_data.title = notify_title;
    notif_data.content = notify_content;
    notif_data.type = "comment_praise";
    notif_data.sender_id = data.liker_id;
    notif_data.article_id = data.article_id;
    notif_data.comment_id = data.comment_id;

    auto notif_info = NotificationService::AddNotification(notif_data);
    if (notif_info) {
        Json::Value wsMsg;
        wsMsg["id"] = notif_info->getId();
        wsMsg["title"] = notify_title;
        wsMsg["content"] = notify_content;
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

    std::string notify_title = "New follower";
    std::string notify_content = data.follower_name + " followed you.";

    NotificationData notif_data{};
    notif_data.user_id = data.following_id;
    notif_data.title = notify_title;
    notif_data.content = notify_content;
    notif_data.type = "follow";
    notif_data.sender_id = data.follower_id;

    auto notif_info = NotificationService::AddNotification(notif_data);
    if (notif_info) {
        Json::Value wsMsg;
        wsMsg["id"] = notif_info->getId();
        wsMsg["title"] = notify_title;
        wsMsg["content"] = notify_content;
        wsMsg["type"] = "follow";
        wsMsg["is_read"] = false;
        wsMsg["create_time"] = notif_info->getCreateTime();
        SendNotificationToUser(data.following_id, chen::JsonUtil::ToString(wsMsg));
    }
}

static void EventUserAdminAction(const std::any& d)
{
    auto data = std::any_cast<const EventUserAdminData&>(d);

    std::string title, content;
    if (data.type == "password_reset") {
        title = "Password reset";
        content = "Your account password was reset by an administrator. Please change it as soon as possible.";
    } else if (data.type == "account_recovered") {
        title = "Account recovered";
        content = "Your account has been restored by an administrator and can now be used normally.";
    } else if (data.type == "account_deleted") {
        title = "Account disabled";
        content = "Your account has been disabled by an administrator. Please contact an administrator if you have "
                  "questions.";
    } else {
        return;
    }

    NotificationData notif_data{};
    notif_data.user_id = data.user_id;
    notif_data.title = title;
    notif_data.content = content;
    notif_data.type = data.type;
    notif_data.sender_id = 0;

    auto notif = NotificationService::AddNotification(notif_data);
    if (notif) {
        Json::Value wsMsg;
        wsMsg["id"] = notif->getId();
        wsMsg["title"] = title;
        wsMsg["content"] = content;
        wsMsg["type"] = data.type;
        wsMsg["is_read"] = false;
        wsMsg["create_time"] = notif->getCreateTime();
        SendNotificationToUser(data.user_id, chen::JsonUtil::ToString(wsMsg));
    }
}

static void EventOrgDeleted(const std::any& d)
{
    auto data = std::any_cast<const EventOrgDeletedData&>(d);

    std::string title = "Organization deleted";
    std::string content = "Organization \"" + data.org_name + "\" has been deleted by an administrator.";

    NotificationData notif_data{};
    notif_data.user_id = data.member_id;
    notif_data.title = title;
    notif_data.content = content;
    notif_data.type = "org_deleted";
    notif_data.sender_id = 0;

    auto notif = NotificationService::AddNotification(notif_data);
    if (notif) {
        Json::Value wsMsg;
        wsMsg["id"] = notif->getId();
        wsMsg["title"] = title;
        wsMsg["content"] = content;
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
        std::string title = "Organization role changed";
        std::string content =
            "Your role in organization \"" + data.org_name + "\" has been updated to " + data.new_role_name + ".";

        NotificationData notif_data{};
        notif_data.user_id = data.target_user_id;
        notif_data.title = title;
        notif_data.content = content;
        notif_data.type = "org_role_change";
        notif_data.sender_id = data.operator_id;

        auto notif_info = NotificationService::AddNotification(notif_data);
        if (notif_info) {
            Json::Value wsMsg;
            wsMsg["id"] = notif_info->getId();
            wsMsg["title"] = title;
            wsMsg["content"] = content;
            wsMsg["type"] = "org_role_change";
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif_info->getCreateTime();
            SendNotificationToUser(data.target_user_id, chen::JsonUtil::ToString(wsMsg));
        }
    } else if (data.type == "kicked") {
        std::string title = "Member removed from organization";
        std::string content = data.operator_name + " removed \"" + data.applicant_name + "\" from organization \""
            + data.org_name + "\".";
        // Notify all org admins
        std::vector<DB::OrganizationUserRelInfo::ptr> members;
        OrganizationUserRelRepository::ListByOrgPages(members, data.org_id, 0, 10000, -1, true, GetDB());
        for (auto& m : members) {
            if (m->getRole() == OrgRole::ORG_ADMIN) {
                NotificationData notif_data{};
                notif_data.user_id = m->getUserId();
                notif_data.title = title;
                notif_data.content = content;
                notif_data.type = "org_member_kicked";
                notif_data.sender_id = data.operator_id;

                auto notif_info = NotificationService::AddNotification(notif_data);
                if (notif_info) {
                    Json::Value wsMsg;
                    wsMsg["id"] = notif_info->getId();
                    wsMsg["title"] = title;
                    wsMsg["content"] = content;
                    wsMsg["type"] = "org_member_kicked";
                    wsMsg["is_read"] = false;
                    wsMsg["create_time"] = notif_info->getCreateTime();
                    SendNotificationToUser(m->getUserId(), chen::JsonUtil::ToString(wsMsg));
                }
            }
        }
        // Notify kicked user
        {
            std::string kicked_title = "Removed from organization";
            std::string kicked_content = "You have been removed from organization \"" + data.org_name + "\".";

            NotificationData notif_data{};
            notif_data.user_id = data.kicked_user_id;
            notif_data.title = kicked_title;
            notif_data.content = kicked_content;
            notif_data.type = "org_member_kicked";
            notif_data.sender_id = data.operator_id;

            auto notif_info = NotificationService::AddNotification(notif_data);
            if (notif_info) {
                Json::Value wsMsg;
                wsMsg["id"] = notif_info->getId();
                wsMsg["title"] = kicked_title;
                wsMsg["content"] = kicked_content;
                wsMsg["type"] = "org_member_kicked";
                wsMsg["is_read"] = false;
                wsMsg["create_time"] = notif_info->getCreateTime();
                SendNotificationToUser(data.kicked_user_id, chen::JsonUtil::ToString(wsMsg));
            }
        }
    } else if (data.type == "join_request") {
        auto applicant = UserRepository::Get(data.applicant_id, GetDB());
        std::string applicant_name = applicant ? applicant->getName() : std::to_string(data.applicant_id);
        std::string title = "New join request";
        std::string content =
            "User \"" + applicant_name + "\" requested to join organization \"" + data.org_name + "\".";
        std::vector<DB::OrganizationUserRelInfo::ptr> members;
        OrganizationUserRelRepository::ListByOrgPages(members, data.org_id, 0, 10000, -1, true, GetDB());
        for (auto& m : members) {
            if (m->getRole() == OrgRole::ORG_ADMIN) {
                NotificationData notif_data{};
                notif_data.user_id = m->getUserId();
                notif_data.title = title;
                notif_data.content = content;
                notif_data.type = "org_join_request";
                notif_data.sender_id = data.applicant_id;

                auto notif_info = NotificationService::AddNotification(notif_data);
                if (notif_info) {
                    Json::Value wsMsg;
                    wsMsg["id"] = notif_info->getId();
                    wsMsg["title"] = title;
                    wsMsg["content"] = content;
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
        std::string title = approved ? "Join request approved" : "Join request rejected";
        std::string content = approved
            ? "Your request to join organization \"" + data.org_name + "\" has been approved."
            : "Your request to join organization \"" + data.org_name + "\" has been rejected.";

        NotificationData notif_data{};
        notif_data.user_id = data.applicant_id;
        notif_data.title = title;
        notif_data.content = content;
        notif_data.type = notif_type;
        notif_data.sender_id = data.operator_id;

        auto notif_info = NotificationService::AddNotification(notif_data);
        if (notif_info) {
            Json::Value wsMsg;
            wsMsg["id"] = notif_info->getId();
            wsMsg["title"] = title;
            wsMsg["content"] = content;
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
        std::string title = "New organization application";
        std::string content =
            "User \"" + data.applicant_name + "\" applied to create organization \"" + data.org_name + "\".";
        std::vector<DB::UserInfo::ptr> admins;
        UserRepository::ListByPages(admins, 0, 10000, UserRole::ADMIN, -1, -1, true, GetDB());
        for (auto& admin : admins) {
            NotificationData notif_data{};
            notif_data.user_id = admin->getId();
            notif_data.title = title;
            notif_data.content = content;
            notif_data.type = "org_apply_request";
            notif_data.sender_id = 0;
            notif_data.article_id = data.apply_id;

            auto notif = NotificationService::AddNotification(notif_data);
            if (notif) {
                Json::Value wsMsg;
                wsMsg["id"] = notif->getId();
                wsMsg["title"] = title;
                wsMsg["content"] = content;
                wsMsg["type"] = "org_apply_request";
                wsMsg["apply_id"] = data.apply_id;
                wsMsg["is_read"] = false;
                wsMsg["create_time"] = notif->getCreateTime();
                SendNotificationToUser(admin->getId(), chen::JsonUtil::ToString(wsMsg));
            }
        }
    } else if (data.type == "approved") {
        std::string title = "Organization application approved";
        std::string content = "Your application to create organization \"" + data.org_name + "\" has been approved.";

        NotificationData notif_data{};
        notif_data.user_id = data.applicant_id;
        notif_data.title = title;
        notif_data.content = content;
        notif_data.type = "org_apply_approved";
        notif_data.sender_id = 0;
        notif_data.article_id = data.org_id;

        auto notif = NotificationService::AddNotification(notif_data);
        if (notif) {
            Json::Value wsMsg;
            wsMsg["id"] = notif->getId();
            wsMsg["title"] = title;
            wsMsg["content"] = content;
            wsMsg["type"] = "org_apply_approved";
            wsMsg["org_id"] = data.org_id;
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif->getCreateTime();
            SendNotificationToUser(data.applicant_id, chen::JsonUtil::ToString(wsMsg));
        }
    } else if (data.type == "rejected") {
        std::string title = "Organization application rejected";
        std::string reject_reason = data.reason.empty() ? "" : " Reason: " + data.reason;
        std::string content =
            "Your application to create organization \"" + data.org_name + "\" was not approved." + reject_reason;

        NotificationData notif_data{};
        notif_data.user_id = data.applicant_id;
        notif_data.title = title;
        notif_data.content = content;
        notif_data.type = "org_apply_rejected";
        notif_data.sender_id = 0;

        auto notif = NotificationService::AddNotification(notif_data);
        if (notif) {
            Json::Value wsMsg;
            wsMsg["id"] = notif->getId();
            wsMsg["title"] = title;
            wsMsg["content"] = content;
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
        std::string title = "Assignment deleted";
        std::string content = "Assignment \"" + data.assignment_name + "\" has been deleted by an administrator.";

        NotificationData notif_data{};
        notif_data.user_id = data.submitter_id;
        notif_data.title = title;
        notif_data.content = content;
        notif_data.type = "assignment_deleted";
        notif_data.sender_id = 0;

        auto notif = NotificationService::AddNotification(notif_data);
        if (notif) {
            Json::Value wsMsg;
            wsMsg["id"] = notif->getId();
            wsMsg["title"] = title;
            wsMsg["content"] = content;
            wsMsg["type"] = "assignment_deleted";
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif->getCreateTime();
            SendNotificationToUser(data.submitter_id, chen::JsonUtil::ToString(wsMsg));
        }
    } else if (data.type == "created") {
        std::string title = "New assignment published";
        std::string content = "A new assignment \"" + data.assignment_name + "\" (" + data.subject_name
            + ") has been published. Please complete it on time.";
        for (auto uid : data.member_user_ids) {
            NotificationData notif_data{};
            notif_data.user_id = uid;
            notif_data.title = title;
            notif_data.content = content;
            notif_data.type = "assignment_created";
            notif_data.sender_id = 0;
            notif_data.article_id = data.assignment_id;

            auto notif = NotificationService::AddNotification(notif_data);
            if (notif) {
                Json::Value wsMsg;
                wsMsg["id"] = notif->getId();
                wsMsg["title"] = title;
                wsMsg["content"] = content;
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

    std::string title = "Assignment submitted";
    std::string content = "User \"" + data.submitter_name + "\" submitted assignment \"" + data.assignment_name + "\".";
    for (auto admin_id : data.admin_user_ids) {
        NotificationData notif_data{};
        notif_data.user_id = admin_id;
        notif_data.title = title;
        notif_data.content = content;
        notif_data.type = "assignment_submitted";
        notif_data.sender_id = 0;
        notif_data.article_id = data.assignment_id;

        auto notif = NotificationService::AddNotification(notif_data);
        if (notif) {
            Json::Value wsMsg;
            wsMsg["id"] = notif->getId();
            wsMsg["title"] = title;
            wsMsg["content"] = content;
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

    std::string title, notif_type, item_label;
    if (data.type == "task") {
        title = "New task assigned";
        notif_type = "task_assigned";
        item_label = "Task";
    } else if (data.type == "requirement") {
        title = "New requirement assigned";
        notif_type = "requirement_assigned";
        item_label = "Requirement";
    } else if (data.type == "bug") {
        title = "New bug assigned";
        notif_type = "bug_assigned";
        item_label = "Bug";
    } else {
        return;
    }
    std::string content =
        item_label + " \"" + data.item_title + "\" has been assigned to you. Please handle it as soon as possible.";

    NotificationData notif_data{};
    notif_data.user_id = data.assignee_id;
    notif_data.title = title;
    notif_data.content = content;
    notif_data.type = notif_type;
    notif_data.sender_id = data.creator_id;
    notif_data.article_id = data.item_id;

    auto notif_info = NotificationService::AddNotification(notif_data);
    if (notif_info) {
        Json::Value wsMsg;
        wsMsg["id"] = notif_info->getId();
        wsMsg["title"] = title;
        wsMsg["content"] = content;
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
