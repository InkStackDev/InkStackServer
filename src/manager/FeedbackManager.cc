#include "FeedbackManager.hpp"

#include "../Util/Util.hpp"

#include <Chen/Log/Log.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

FeedbackManager::FeedbackManager()
{
}

bool FeedbackManager::list(std::vector<DB::UserFeedbackInfo::ptr>& infos,
    const std::string& type,
    int32_t page,
    int32_t page_size,
    int64_t* total)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    auto qb = DB::UserFeedbackInfoDao::newQuery();
    qb->where("is_deleted", "=", (int64_t)0);

    if (!type.empty()) {
        qb->where("type", "=", type);
    }

    qb->orderBy("id", "DESC");

    int32_t offset = (page - 1) * page_size;
    int64_t total_cnt = 0;
    if (DB::UserFeedbackInfoDao::QueryByBuilderPages(infos, total_cnt, qb, offset, page_size, db)) {
        ERROR(logger) << "QueryByBuilderPages failed";
        return false;
    }
    if (total) {
        *total = total_cnt;
    }
    return true;
}

DB::UserFeedbackInfo::ptr FeedbackManager::get(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    auto qb = DB::UserFeedbackInfoDao::newQuery();
    qb->where("id", "=", id);
    qb->where("is_deleted", "=", (int64_t)0);

    std::vector<DB::UserFeedbackInfo::ptr> results;
    if (DB::UserFeedbackInfoDao::QueryByBuilder(results, qb, db) || results.empty()) {
        return nullptr;
    }
    return results[0];
}

bool FeedbackManager::remove(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    auto info = get(id);
    if (!info) {
        ERROR(logger) << "feedback not found: " << id;
        return false;
    }

    info->setIsDeleted(1);
    info->setUpdateTime(time(0));

    if (DB::UserFeedbackInfoDao::Update(info, db)) {
        ERROR(logger) << "soft delete feedback failed, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
        return false;
    }

    return true;
}
