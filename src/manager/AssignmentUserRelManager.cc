#include "AssignmentUserRelManager.hpp"

#include <Chen/Log/Log.hpp>

#include "../Util/Util.hpp"
#include "CacheUtil.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

DB::AssignmentUserRelInfo::ptr AssignmentUserRelManager::ParseRow(chen::ISQLData::ptr rt)
{
    return DB::AssignmentUserRelInfoDao::ParseRow(rt);
}

DB::AssignmentUserRelInfo::ptr AssignmentUserRelManager::get(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    return DB::AssignmentUserRelInfoDao::Query(id, db);
}

DB::AssignmentUserRelInfo::ptr AssignmentUserRelManager::getByAssignAndUser(int64_t assign_id, int64_t user_id)
{
    int64_t cachedId = GetCachedIdMapping("assign_usr:" + std::to_string(assign_id) + ":" + std::to_string(user_id));
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = DB::AssignmentUserRelInfoDao::QueryByAssignmentIdUserId(assign_id, user_id, db);
    if (info) {
        CacheIdMapping("assign_usr:" + std::to_string(assign_id) + ":" + std::to_string(user_id), info->getId());
    }
    return info;
}
