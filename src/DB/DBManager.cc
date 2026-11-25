#include "DBManager.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/DB/MySQL.hpp>

#include <ranges>

#include "../Config/AppConfig.hpp"
#include "DB/ArticleCategoryRelInfo.hpp"
#include "DB/ArticleInfo.hpp"
#include "DB/ArticleLabelRelInfo.hpp"
#include "DB/ArticlePraiseRelInfo.hpp"
#include "DB/AssignmentInfo.hpp"
#include "DB/AssignmentOrganizationRelInfo.hpp"
#include "DB/AssignmentUserRelInfo.hpp"
#include "DB/BackupRecordInfo.hpp"
#include "DB/BadgeInfo.hpp"
#include "DB/BugInfo.hpp"
#include "DB/CategoryInfo.hpp"
#include "DB/CommentInfo.hpp"
#include "DB/CommentPraiseRelInfo.hpp"
#include "DB/ConversationInfo.hpp"
#include "DB/ConversationMessageInfo.hpp"
#include "DB/EmailVerificationInfo.hpp"
#include "DB/ExportRecordInfo.hpp"
#include "DB/HelpFaqsInfo.hpp"
#include "DB/LabelInfo.hpp"
#include "DB/NotificationInfo.hpp"
#include "DB/OrganizationApplyInfo.hpp"
#include "DB/OrganizationInfo.hpp"
#include "DB/OrganizationRepoPrsInfo.hpp"
#include "DB/OrganizationReposInfo.hpp"
#include "DB/OrganizationUserRelInfo.hpp"
#include "DB/RequirementInfo.hpp"
#include "DB/ResourceInfo.hpp"
#include "DB/SystemSettingsInfo.hpp"
#include "DB/TaskInfo.hpp"
#include "DB/UserAiConfigInfo.hpp"
#include "DB/UserFeedbackInfo.hpp"
#include "DB/UserFollowRelInfo.hpp"
#include "DB/UserInfo.hpp"
#include "DB/UserLoginDeviceInfo.hpp"
#include "DB/UserOauthInfo.hpp"
#include "DB/UserPasskeyInfo.hpp"
#include "DB/UserRecoveryCodeInfo.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

bool DBManager::CreateAllTables()
{
    const auto mysql_dbs = GetMysqlDbs();
    for (const auto& params : mysql_dbs | std::views::values) {
        auto mysql = std::make_shared<chen::MySQL>(params);
        if (!mysql->connect()) {
            ERROR(logger) << "connect mysql failed";
            return false;
        }

#define XX(class_name, table_name)                                                                                     \
    if (DB::class_name::CreateTableMySQL(mysql)) {                                                                     \
        ERROR(logger) << "create " table_name " table failed";                                                         \
        return false;                                                                                                  \
    }
        XX(EmailVerificationInfoDao, "email_verification")
        XX(UserInfoDao, "user")
        XX(ArticleInfoDao, "article")
        XX(CategoryInfoDao, "category")
        XX(LabelInfoDao, "label")
        XX(ArticleCategoryRelInfoDao, "article_category_rel")
        XX(ArticleLabelRelInfoDao, "article_label_rel")
        XX(AssignmentInfoDao, "assignment")
        XX(OrganizationApplyInfoDao, "organization_apply")
        XX(OrganizationInfoDao, "organization")
        XX(OrganizationRepoPrsInfoDao, "organization_repo_prs")
        XX(OrganizationReposInfoDao, "organization_repos")
        XX(OrganizationUserRelInfoDao, "organization_user_rel")
        XX(AssignmentOrganizationRelInfoDao, "assignment_organization_rel")
        XX(AssignmentUserRelInfoDao, "assignment_user_rel")
        XX(ResourceInfoDao, "resource")
        XX(NotificationInfoDao, "notification")
        XX(UserFollowRelInfoDao, "user_follow_rel")
        XX(ArticlePraiseRelInfoDao, "article_praise_rel")
        XX(BackupRecordInfoDao, "backup_record")
        XX(ExportRecordInfoDao, "export_record")
        XX(CommentInfoDao, "comment")
        XX(CommentPraiseRelInfoDao, "comment_praise_rel")
        XX(ConversationInfoDao, "conversation")
        XX(ConversationMessageInfoDao, "conversation_message")
        XX(RequirementInfoDao, "requirement")
        XX(BugInfoDao, "bug")
        XX(TaskInfoDao, "task")
        XX(SystemSettingsInfoDao, "system_settings")
        XX(BadgeInfoDao, "badge")
        XX(HelpFaqsInfoDao, "help_faqs")
        XX(UserFeedbackInfoDao, "user_feedback")
        XX(UserAiConfigInfoDao, "user_ai_config")
        XX(UserLoginDeviceInfoDao, "user_login_device")
        XX(UserOauthInfoDao, "user_oauth")
        XX(UserPasskeyInfoDao, "user_passkey")
        XX(UserRecoveryCodeInfoDao, "user_recovery_code")
#undef XX
    }

    return true;
}

bool DBManager::MigrateAllTables()
{
    INFO(logger) << "migrate database begin";

    const auto mysql_dbs = GetMysqlDbs();
    for (const auto& params : mysql_dbs | std::views::values) {
        auto mysql = std::make_shared<chen::MySQL>(params);
        if (!mysql->connect()) {
            ERROR(logger) << "connect mysql failed";
            return false;
        }
#define XX(clazz)                                                                                                      \
    if (DB::clazz::MigrateTableMySQL(mysql) != 0) {                                                                    \
        ERROR(logger) << "migrate table mysql failed";                                                                 \
        return false;                                                                                                  \
    }
        XX(EmailVerificationInfoDao)
        XX(UserInfoDao)
        XX(ArticleInfoDao)
        XX(CategoryInfoDao)
        XX(LabelInfoDao)
        XX(ArticleCategoryRelInfoDao)
        XX(ArticleLabelRelInfoDao)
        XX(AssignmentInfoDao)
        XX(OrganizationApplyInfoDao)
        XX(OrganizationInfoDao)
        XX(OrganizationRepoPrsInfoDao)
        XX(OrganizationReposInfoDao)
        XX(OrganizationUserRelInfoDao)
        XX(AssignmentOrganizationRelInfoDao)
        XX(AssignmentUserRelInfoDao)
        XX(ResourceInfoDao)
        XX(NotificationInfoDao)
        XX(UserFollowRelInfoDao)
        XX(ArticlePraiseRelInfoDao)
        XX(CommentInfoDao)
        XX(CommentPraiseRelInfoDao)
        XX(ConversationInfoDao)
        XX(ConversationMessageInfoDao)
        XX(BackupRecordInfoDao)
        XX(ExportRecordInfoDao)
        XX(RequirementInfoDao)
        XX(BugInfoDao)
        XX(TaskInfoDao)
        XX(SystemSettingsInfoDao)
        XX(BadgeInfoDao)
        XX(HelpFaqsInfoDao)
        XX(UserFeedbackInfoDao)
        XX(UserAiConfigInfoDao)
        XX(UserLoginDeviceInfoDao)
        XX(UserOauthInfoDao)
        XX(UserPasskeyInfoDao)
        XX(UserRecoveryCodeInfoDao)
#undef XX
        INFO(logger) << "migrate database end";
    }

    return true;
}
