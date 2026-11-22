#include "ArticleService.hpp"

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Manager/ArticleManager.hpp"
#include "../Repository/ArticleRepository.hpp"
#include "../Search/SearchSync.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

ArticleService::DeleteArticlesResult ArticleService::DeleteArticles(int64_t uid,
    const std::vector<int64_t>& ids,
    std::vector<int64_t>& deleted)
{
    deleted.clear();
    if (ids.empty()) {
        return DeleteArticlesResult::Ok;
    }

    auto db = GetDB();
    if (!db) {
        return DeleteArticlesResult::DbError;
    }

    std::vector<DB::ArticleInfo::ptr> targets;
    for (auto id : ids) {
        auto info = ArticleRepository::Get(id, db);
        if (!info || info->getUserId() != uid || info->getIsDeleted()) {
            continue;
        }
        targets.push_back(info);
    }
    if (targets.empty()) {
        return DeleteArticlesResult::Ok;
    }

    auto trans = db->openTransaction();
    if (!trans) {
        return DeleteArticlesResult::DbError;
    }
    int64_t now = chen::TimeUtil::GetCurrentSec();
    for (auto& info : targets) {
        info->setIsDeleted(1);
        info->setUpdateTime(now);
        if (!ArticleRepository::Update(info, db)) {
            ERROR(logger) << "DeleteArticles update failed, id=" << info->getId();
            return DeleteArticlesResult::DbError;
        }
    }
    if (!trans->commit()) {
        ERROR(logger) << "DeleteArticles commit failed";
        return DeleteArticlesResult::DbError;
    }

    for (auto& info : targets) {
        ArticleRepository::Put(info);
        deleted.push_back(info->getId());

        SearchSyncMgr::GetInstance()->markDirty(info->getId(), SearchSync::OP_REMOVE);
        ArticleMgr::GetInstance()->unscheduleArticle(info->getId());
        if (info->getState() == ArticleManager::Status::PUBLISHED && info->getPublishTime() > 0) {
            ArticleMgr::GetInstance()->clearCalendarCache(info->getUserId(), info->getPublishTime());
        }
    }
    return DeleteArticlesResult::Ok;
}
