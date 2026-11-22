#include "CommentService.hpp"

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Manager/CommentManager.hpp"
#include "../Repository/ArticleRepository.hpp"
#include "../Repository/CommentRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

CommentService::CreateCommentResult CommentService::CreateComment(const CreateCommentInput& input,
    DB::CommentInfo::ptr& out)
{
    out.reset();

    auto db = GetDB();
    if (!db) {
        return CreateCommentResult::DbError;
    }

    auto article = ArticleRepository::Get(input.article_id, db);
    if (!article || article->getIsDeleted()) {
        return CreateCommentResult::ArticleNotFound;
    }
    if (input.parent_id > 0) {
        auto parent = CommentRepository::Get(input.parent_id, db);
        if (!parent || parent->getIsDeleted()) {
            return CreateCommentResult::ParentNotFound;
        }
    }

    auto info = std::make_shared<DB::CommentInfo>();
    info->setArticleId(input.article_id);
    info->setUserId(input.user_id);
    info->setContent(input.content);
    info->setParentId(input.parent_id);
    info->setIp(input.ip);
    info->setUserAgent(input.user_agent);
    info->setStatus(CommentManager::Status::APPROVED);
    info->setIsReported(0);
    info->setReportCount(0);
    info->setIsDeleted(0);
    int64_t now = chen::TimeUtil::GetCurrentSec();
    info->setCreateTime(now);
    info->setUpdateTime(now);

    if (!CommentRepository::Insert(info, db)) {
        ERROR(logger) << "CreateComment insert failed, article_id=" << input.article_id;
        return CreateCommentResult::DbError;
    }
    CommentRepository::Put(info);
    out = info;
    return CreateCommentResult::Ok;
}

CommentService::DeleteCommentResult CommentService::DeleteComment(int64_t uid, int64_t comment_id)
{
    auto db = GetDB();
    if (!db) {
        return DeleteCommentResult::DbError;
    }
    auto info = CommentRepository::Get(comment_id, db);
    if (!info || info->getIsDeleted()) {
        return DeleteCommentResult::CommentNotFound;
    }
    if (info->getUserId() != uid) {
        return DeleteCommentResult::AccessDenied;
    }

    info->setIsDeleted(1);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!CommentRepository::Update(info, db)) {
        ERROR(logger) << "DeleteComment update failed, id=" << comment_id;
        return DeleteCommentResult::DbError;
    }
    CommentRepository::Put(info);
    return DeleteCommentResult::Ok;
}
