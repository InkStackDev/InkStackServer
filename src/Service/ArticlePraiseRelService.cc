#include "ArticlePraiseRelService.hpp"

#include <memory>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Repository/ArticlePraiseRelRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

DB::ArticlePraiseRelInfo::ptr ArticlePraiseRelService::Praise(int64_t user_id, int64_t article_id)
{
    auto db = GetDB();
    if (!db) {
        return nullptr;
    }

    auto existing = ArticlePraiseRelRepository::GetByUserAndArticle(user_id, article_id, db);
    int64_t now = chen::TimeUtil::GetCurrentSec();
    if (existing) {
        if (!existing->getIsDeleted()) {
            return existing;
        }
        existing->setIsDeleted(0);
        existing->setUpdateTime(now);
        if (!ArticlePraiseRelRepository::Update(existing, db)) {
            ERROR(logger) << "ArticlePraiseRelService::Praise update failed";
            return nullptr;
        }
        ArticlePraiseRelRepository::Put(existing);
        return existing;
    }

    auto info = std::make_shared<DB::ArticlePraiseRelInfo>();
    info->setUserId(user_id);
    info->setArticleId(article_id);
    info->setIsDeleted(0);
    info->setCreateTime(now);
    info->setUpdateTime(now);
    if (!ArticlePraiseRelRepository::Insert(info, db)) {
        ERROR(logger) << "ArticlePraiseRelService::Praise insert failed";
        return nullptr;
    }
    ArticlePraiseRelRepository::Put(info);
    return info;
}

bool ArticlePraiseRelService::Unpraise(int64_t user_id, int64_t article_id)
{
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto info = ArticlePraiseRelRepository::GetByUserAndArticle(user_id, article_id, db);
    if (!info || info->getIsDeleted()) {
        return false;
    }
    info->setIsDeleted(1);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!ArticlePraiseRelRepository::Update(info, db)) {
        ERROR(logger) << "ArticlePraiseRelService::Unpraise update failed";
        return false;
    }
    ArticlePraiseRelRepository::Put(info);
    return true;
}

bool ArticlePraiseRelService::IsPraising(int64_t user_id, int64_t article_id)
{
    auto info = ArticlePraiseRelRepository::GetByUserAndArticle(user_id, article_id, GetDB());
    return info && !info->getIsDeleted();
}
