#include "CommentService.hpp"

#include <memory>

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Manager/CommentManager.hpp"
#include "../Repository/ArticleRepository.hpp"
#include "../Repository/CommentPraiseRepository.hpp"
#include "../Repository/CommentQueryRepository.hpp"
#include "../Repository/CommentRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

namespace {

void InvalidateAfterChange(const DB::CommentInfo::ptr& info)
{
    if (!info) {
        return;
    }
    CommentQueryRepository::InvalidateArticle(info->getArticleId());
    CommentQueryRepository::InvalidateStats();
    if (info->getParentId() > 0) {
        CommentQueryRepository::InvalidateReplies(info->getParentId());
    }
}

} // namespace

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
    InvalidateAfterChange(info);
    out = info;
    return CreateCommentResult::Ok;
}

CommentService::UpdateCommentResult CommentService::UpdateComment(int64_t uid,
    int64_t comment_id,
    const std::string& content,
    DB::CommentInfo::ptr& out)
{
    out.reset();

    auto db = GetDB();
    if (!db) {
        return UpdateCommentResult::DbError;
    }
    auto info = CommentRepository::Get(comment_id, db);
    if (!info || info->getIsDeleted()) {
        return UpdateCommentResult::CommentNotFound;
    }
    if (info->getUserId() != uid) {
        return UpdateCommentResult::AccessDenied;
    }

    info->setContent(content);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!CommentRepository::Update(info, db)) {
        ERROR(logger) << "UpdateComment update failed, id=" << comment_id;
        return UpdateCommentResult::DbError;
    }
    CommentRepository::Put(info);
    InvalidateAfterChange(info);
    out = info;
    return UpdateCommentResult::Ok;
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
    InvalidateAfterChange(info);
    return DeleteCommentResult::Ok;
}

int64_t CommentService::BatchUpdateStatus(const std::vector<int64_t>& ids, int32_t status)
{
    if (ids.empty()) {
        return 0;
    }
    auto db = GetDB();
    if (!db) {
        return 0;
    }

    std::vector<DB::CommentInfo::ptr> infos;
    infos.reserve(ids.size());
    for (auto id : ids) {
        auto info = CommentRepository::Get(id, db);
        if (info && !info->getIsDeleted()) {
            infos.push_back(info);
        }
    }
    if (infos.empty()) {
        return 0;
    }

    auto qb = DB::CommentInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->set("status", (int64_t)status);
    qb->set("update_time", (int64_t)chen::TimeUtil::GetCurrentSec());
    qb->whereIn("id", ids);
    if (qb->executeUpdate(db)) {
        ERROR(logger) << "BatchUpdateStatus executeUpdate fail errno=" << db->getErrno();
        return 0;
    }

    int64_t now = chen::TimeUtil::GetCurrentSec();
    for (auto& info : infos) {
        info->setStatus(status);
        info->setUpdateTime(now);
        CommentRepository::Put(info);
        InvalidateAfterChange(info);
    }
    return (int64_t)infos.size();
}

int64_t CommentService::BatchDelete(const std::vector<int64_t>& ids)
{
    if (ids.empty()) {
        return 0;
    }
    auto db = GetDB();
    if (!db) {
        return 0;
    }

    std::vector<DB::CommentInfo::ptr> infos;
    infos.reserve(ids.size());
    for (auto id : ids) {
        auto info = CommentRepository::Get(id, db);
        if (info && !info->getIsDeleted()) {
            infos.push_back(info);
        }
    }
    if (infos.empty()) {
        return 0;
    }

    auto qb = DB::CommentInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->set("is_deleted", (int64_t)1);
    qb->set("update_time", (int64_t)chen::TimeUtil::GetCurrentSec());
    qb->whereIn("id", ids);
    if (qb->executeUpdate(db)) {
        ERROR(logger) << "BatchDelete executeUpdate fail errno=" << db->getErrno();
        return 0;
    }

    int64_t now = chen::TimeUtil::GetCurrentSec();
    for (auto& info : infos) {
        info->setIsDeleted(1);
        info->setUpdateTime(now);
        CommentRepository::Put(info);
        InvalidateAfterChange(info);
    }
    return (int64_t)infos.size();
}

CommentService::PraiseResult CommentService::TogglePraise(int64_t uid,
    int64_t comment_id,
    bool& is_praising,
    DB::CommentInfo::ptr& out)
{
    out.reset();
    is_praising = false;

    auto db = GetDB();
    if (!db) {
        return PraiseResult::DbError;
    }
    auto comment = CommentRepository::Get(comment_id, db);
    if (!comment || comment->getIsDeleted()) {
        return PraiseResult::CommentNotFound;
    }
    out = comment;

    auto existing = CommentPraiseRepository::GetByUserAndComment(uid, comment_id, db);
    int64_t now = chen::TimeUtil::GetCurrentSec();
    if (existing) {
        bool praising = existing->getIsDeleted() == 0;
        existing->setIsDeleted(praising ? 1 : 0);
        existing->setUpdateTime(now);
        if (!CommentPraiseRepository::Update(existing, db)) {
            ERROR(logger) << "TogglePraise update failed, user=" << uid << " comment=" << comment_id;
            return PraiseResult::DbError;
        }
        CommentPraiseRepository::Put(existing);
        is_praising = !praising;
        return PraiseResult::Ok;
    }

    auto info = std::make_shared<DB::CommentPraiseRelInfo>();
    info->setUserId(uid);
    info->setCommentId(comment_id);
    info->setIsDeleted(0);
    info->setCreateTime(now);
    info->setUpdateTime(now);
    if (!CommentPraiseRepository::Insert(info, db)) {
        ERROR(logger) << "TogglePraise insert failed, user=" << uid << " comment=" << comment_id;
        return PraiseResult::DbError;
    }
    CommentPraiseRepository::Put(info);
    is_praising = true;
    return PraiseResult::Ok;
}

int64_t CommentService::CountPraise(int64_t comment_id)
{
    return CommentPraiseRepository::CountByComment(comment_id, GetDB());
}

bool CommentService::IsPraising(int64_t uid, int64_t comment_id)
{
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto info = CommentPraiseRepository::GetByUserAndComment(uid, comment_id, db);
    return info && info->getIsDeleted() == 0;
}

int64_t CommentService::CountReplies(int64_t parent_id)
{
    return CommentQueryRepository::CountReplies(parent_id, GetDB());
}
