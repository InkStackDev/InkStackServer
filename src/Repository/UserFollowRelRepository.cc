#include "UserFollowRelRepository.hpp"

#include <algorithm>
#include <memory>
#include <unordered_set>

#include <Chen/DB/Redis.hpp>
#include <Chen/Log/Log.hpp>

#include "../Cache/EntityCache.hpp"
#include "../Cache/CacheUtil.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

struct UserFollowRelRepositoryCodec {
    static DB::UserFollowRelInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::UserFollowRelInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setFollowerId(CacheJsonInt64(v, "follower_id"));
        info->setFollowingId(CacheJsonInt64(v, "following_id"));
        info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
        info->setCreateTime(CacheJsonTime(v, "create_time"));
        info->setUpdateTime(CacheJsonTime(v, "update_time"));
        info->markClean();
        return info;
    }
};

const EntityCache<DB::UserFollowRelInfo, UserFollowRelRepositoryCodec> g_follow_cache{{"flw", 300}};

static std::string FollowKey(int64_t follower_id, int64_t following_id)
{
    return "flw:" + std::to_string(follower_id) + ":" + std::to_string(following_id);
}

DB::UserFollowRelInfo::ptr UserFollowRelRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return g_follow_cache.Get(id, [id, conn]() {
        return DB::UserFollowRelInfoDao::Query(id, conn);
    });
}

DB::UserFollowRelInfo::ptr UserFollowRelRepository::GetByFollowerAndFollowing(int64_t follower_id,
    int64_t following_id,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return nullptr;
    }
    const std::string key = FollowKey(follower_id, following_id);
    int64_t cached_id = GetCachedIdMapping(key);
    if (cached_id > 0) {
        auto info = Get(cached_id, conn);
        if (info) {
            return info;
        }
        RedisCacheDel("cache:map:" + key);
    }

    auto info = DB::UserFollowRelInfoDao::QueryByFollowerIdFollowingId(follower_id, following_id, conn);
    if (info) {
        Put(info);
        CacheIdMapping(key, info->getId());
    }
    return info;
}

bool UserFollowRelRepository::Insert(DB::UserFollowRelInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    if (DB::UserFollowRelInfoDao::Insert(info, conn) != 0) {
        return false;
    }
    CacheIdMapping(FollowKey(info->getFollowerId(), info->getFollowingId()), info->getId());
    return true;
}

bool UserFollowRelRepository::Update(DB::UserFollowRelInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::UserFollowRelInfoDao::Update(info, conn) == 0;
}

void UserFollowRelRepository::Put(const DB::UserFollowRelInfo::ptr& info)
{
    if (!info) {
        return;
    }
    g_follow_cache.Put(info);
    CacheIdMapping(FollowKey(info->getFollowerId(), info->getFollowingId()), info->getId());
}

void UserFollowRelRepository::Evict(int64_t id)
{
    g_follow_cache.Evict(id);
}

void UserFollowRelRepository::ListFollowing(std::vector<DB::UserFollowRelInfo::ptr>& results,
    int64_t follower_id,
    uint64_t offset,
    uint64_t size,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return;
    }
    auto qb = DB::UserFollowRelInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->where("follower_id", "=", follower_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);
    DB::UserFollowRelInfoDao::QueryByBuilder(results, qb, conn);
}

void UserFollowRelRepository::ListFollowers(std::vector<DB::UserFollowRelInfo::ptr>& results,
    int64_t following_id,
    uint64_t offset,
    uint64_t size,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return;
    }
    auto qb = DB::UserFollowRelInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->where("following_id", "=", following_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);
    DB::UserFollowRelInfoDao::QueryByBuilder(results, qb, conn);
}

int64_t UserFollowRelRepository::CountFollowing(int64_t follower_id, chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::UserFollowRelInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("follower_id", "=", follower_id);
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    if (qb->executeCount(total, conn)) {
        return 0;
    }
    return total;
}

int64_t UserFollowRelRepository::CountFollowers(int64_t following_id, chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::UserFollowRelInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("following_id", "=", following_id);
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    if (qb->executeCount(total, conn)) {
        return 0;
    }
    return total;
}

void UserFollowRelRepository::ListMutualFollowing(std::vector<int64_t>& user_ids, int64_t uid, chen::IDB::ptr conn)
{
    if (!conn) {
        return;
    }
    auto qb = DB::UserFollowRelInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->select("following_id");
    qb->where("follower_id", "=", uid);
    qb->where("is_deleted", "=", (int64_t)0);
    std::vector<int64_t> following_ids;
    if (qb->queryColumn<int64_t>(following_ids, conn, "following_id")) {
        ERROR(logger) << "ListMutualFollowing queryColumn fail";
        return;
    }
    if (following_ids.empty()) {
        return;
    }
    std::unordered_set<int64_t> following_set(following_ids.begin(), following_ids.end());

    auto qb2 = DB::UserFollowRelInfoDao::newQuery();
    if (!qb2) {
        return;
    }
    qb2->select("follower_id");
    qb2->where("following_id", "=", uid);
    qb2->where("is_deleted", "=", (int64_t)0);
    qb2->orderBy("id", "DESC");
    std::vector<int64_t> follower_ids;
    if (qb2->queryColumn<int64_t>(follower_ids, conn, "follower_id")) {
        ERROR(logger) << "ListMutualFollowing queryColumn fail";
        return;
    }
    user_ids.reserve(std::min(following_ids.size(), follower_ids.size()));
    for (auto follower_id : follower_ids) {
        if (following_set.count(follower_id)) {
            user_ids.push_back(follower_id);
        }
    }
}
