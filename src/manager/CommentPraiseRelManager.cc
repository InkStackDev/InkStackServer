#include "CommentPraiseRelManager.hpp"

#include <memory>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Repository/CommentPraiseRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

void CommentPraiseRelManager::add(DB::CommentPraiseRelInfo::ptr info)
{
    CommentPraiseRepository::Put(info);
}

DB::CommentPraiseRelInfo::ptr CommentPraiseRelManager::get(int64_t id)
{
    return CommentPraiseRepository::Get(id, GetDB());
}

DB::CommentPraiseRelInfo::ptr CommentPraiseRelManager::getByUserAndComment(int64_t user_id, int64_t comment_id)
{
    return CommentPraiseRepository::GetByUserAndComment(user_id, comment_id, GetDB());
}

DB::CommentPraiseRelInfo::ptr CommentPraiseRelManager::praise(int64_t user_id, int64_t comment_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    auto existing = CommentPraiseRepository::GetByUserAndComment(user_id, comment_id, db);
    int64_t now = chen::TimeUtil::GetCurrentSec();
    if (existing) {
        if (!existing->getIsDeleted()) {
            return existing;
        }
        existing->setIsDeleted(0);
        existing->setUpdateTime(now);
        if (!CommentPraiseRepository::Update(existing, db)) {
            ERROR(logger) << "praise update failed, user=" << user_id << " comment=" << comment_id;
            return nullptr;
        }
        CommentPraiseRepository::Put(existing);
        return existing;
    }

    auto info = std::make_shared<DB::CommentPraiseRelInfo>();
    info->setUserId(user_id);
    info->setCommentId(comment_id);
    info->setIsDeleted(0);
    info->setCreateTime(now);
    info->setUpdateTime(now);
    if (!CommentPraiseRepository::Insert(info, db)) {
        ERROR(logger) << "praise insert failed, user=" << user_id << " comment=" << comment_id;
        return nullptr;
    }
    CommentPraiseRepository::Put(info);
    return info;
}

bool CommentPraiseRelManager::unpraise(int64_t user_id, int64_t comment_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto info = CommentPraiseRepository::GetByUserAndComment(user_id, comment_id, db);
    if (!info || info->getIsDeleted()) {
        return false;
    }

    info->setIsDeleted(1);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!CommentPraiseRepository::Update(info, db)) {
        ERROR(logger) << "unpraise update failed, user=" << user_id << " comment=" << comment_id;
        return false;
    }
    CommentPraiseRepository::Put(info);
    return true;
}

bool CommentPraiseRelManager::isPraising(int64_t user_id, int64_t comment_id)
{
    auto info = getByUserAndComment(user_id, comment_id);
    return info && !info->getIsDeleted();
}

int64_t CommentPraiseRelManager::countByComment(int64_t comment_id)
{
    return CommentPraiseRepository::CountByComment(comment_id, GetDB());
}
