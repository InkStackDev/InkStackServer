#include "NotificationQueryRepository.hpp"

#include <Chen/DB/Redis.hpp>
#include <Chen/Util/TypeUtil.hpp>

#include "../Config/AppConfig.hpp"
#include "../Cache/CacheUtil.hpp"
#include "../Util/Util.hpp"
#include "NotificationRepository.hpp"

static std::string VersionKey(int64_t user_id)
{
    return "notif:ver:" + std::to_string(user_id);
}

static int64_t GetVersion(int64_t user_id)
{
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get %s", VersionKey(user_id).c_str());
    if (!rpy || !rpy->str) {
        return 0;
    }
    return chen::TypeUtil::Atoi(rpy->str);
}

static std::string TypeKey(const std::string& type)
{
    return type.empty() ? "all" : type;
}

void NotificationQueryRepository::ListByUser(std::vector<DB::NotificationInfo::ptr>& results,
    int64_t user_id,
    uint64_t offset,
    uint64_t size,
    const std::string& type,
    chen::IDB::ptr conn)
{
    if (!conn || user_id <= 0) {
        return;
    }

    std::string list_key = "notif:list:" + std::to_string(user_id) + ":" + TypeKey(type) + ":v"
        + std::to_string(GetVersion(user_id)) + ":" + std::to_string(offset) + ":" + std::to_string(size);
    std::vector<int64_t> cached_ids;
    if (GetCachedListResult(list_key, cached_ids)) {
        for (auto id : cached_ids) {
            auto info = NotificationRepository::Get(id, conn);
            if (info) {
                results.push_back(info);
            }
        }
        return;
    }

    auto qb = DB::NotificationInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->where("user_id", "=", user_id);
    qb->whereIf(!type.empty(), "type", "=", type);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);
    if (DB::NotificationInfoDao::QueryByBuilder(results, qb, conn)) {
        return;
    }

    std::vector<int64_t> ids;
    ids.reserve(results.size());
    for (auto& info : results) {
        ids.push_back(info->getId());
    }
    if (!ids.empty()) {
        CacheListResult(list_key, ids);
    }
}

int64_t NotificationQueryRepository::CountByUser(int64_t user_id, const std::string& type, chen::IDB::ptr conn)
{
    if (!conn || user_id <= 0) {
        return 0;
    }
    auto qb = DB::NotificationInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("user_id", "=", user_id);
    qb->whereIf(!type.empty(), "type", "=", type);
    std::string key =
        "notif:cnt:" + std::to_string(user_id) + ":" + TypeKey(type) + ":v" + std::to_string(GetVersion(user_id));
    return ExecuteCountCached(qb, conn, key);
}

int64_t NotificationQueryRepository::UnreadCount(int64_t user_id, chen::IDB::ptr conn)
{
    if (!conn || user_id <= 0) {
        return 0;
    }
    auto qb = DB::NotificationInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("user_id", "=", user_id);
    qb->where("is_read", "=", (int64_t)0);
    qb->where("is_deleted", "=", (int64_t)0);
    std::string key = "notif:unread:" + std::to_string(user_id) + ":v" + std::to_string(GetVersion(user_id));
    return ExecuteCountCached(qb, conn, key);
}

void NotificationQueryRepository::ListBroadcasts(std::vector<DB::NotificationInfo::ptr>& results,
    uint64_t offset,
    uint64_t size,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return;
    }
    auto qb = DB::NotificationInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->where("is_broadcast", "=", (int64_t)1);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);
    DB::NotificationInfoDao::QueryByBuilder(results, qb, conn);
}

void NotificationQueryRepository::InvalidateUser(int64_t user_id)
{
    if (user_id <= 0) {
        return;
    }
    const std::string version_key = VersionKey(user_id);
    chen::RedisUtil::Cmd(GetRedisPoolName(), "incr %s", version_key.c_str());
    chen::RedisUtil::Cmd(GetRedisPoolName(), "expire %s %d", version_key.c_str(), kIdMapCacheTTL);
}
