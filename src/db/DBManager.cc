#include "DBManager.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/DB/MySQL.hpp>

#include <ranges>

#include "db/article_category_rel_info.h"
#include "db/article_info.h"
#include "db/article_label_rel_info.h"
#include "db/article_praise_rel_info.h"
#include "db/assignment_info.h"
#include "db/assignment_organization_rel_info.h"
#include "db/assignment_user_rel_info.h"
#include "db/backup_record_info.h"
#include "db/badge_info.h"
#include "db/bug_info.h"
#include "db/category_info.h"
#include "db/chunk_upload_info.h"
#include "db/comment_info.h"
#include "db/comment_praise_rel_info.h"
#include "db/conversation_info.h"
#include "db/conversation_message_info.h"
#include "db/email_verification_info.h"
#include "db/export_record_info.h"
#include "db/help_faqs_info.h"
#include "db/label_info.h"
#include "db/notification_info.h"
#include "db/organization_apply_info.h"
#include "db/organization_info.h"
#include "db/organization_repo_prs_info.h"
#include "db/organization_repos_info.h"
#include "db/organization_user_rel_info.h"
#include "db/requirement_info.h"
#include "db/resource_info.h"
#include "db/system_settings_info.h"
#include "db/task_info.h"
#include "db/user_ai_config_info.h"
#include "db/user_feedback_info.h"
#include "db/user_follow_rel_info.h"
#include "db/user_info.h"
#include "db/user_login_device_info.h"
#include "db/user_oauth_info.h"
#include "db/user_passkey_info.h"
#include "db/user_recovery_code_info.h"
#include "../Config/AppConfig.hpp"

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

#define XX(class_name, table_name)                             \
    if (db::class_name::CreateTableMySQL(mysql)) {             \
        ERROR(logger) << "create " table_name " table failed"; \
        return false;                                          \
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
        XX(ChunkUploadInfoDao, "chunk_upload")
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
#define XX(clazz)                                      \
    if (db::clazz::MigrateTableMySQL(mysql) != 0) {    \
        ERROR(logger) << "migrate table mysql failed"; \
        return false;                                  \
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
        XX(ChunkUploadInfoDao)
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
