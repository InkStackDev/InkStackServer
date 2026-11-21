#include "UserFollowRelManager.hpp"

#include "../util/Util.hpp"
#include "CacheUtil.hpp"

#include <chen/log/log.h>
#include <chen/util/json_util.h>

#include <algorithm>
#include <unordered_set>

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 300;
static const char* kCachePrefix = "cache:obj:flw:";

static std::string UserFollowRelCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static data::UserFollowRelInfo::ptr ParseUserFollowRelCache(const Json::Value& v)
{
    auto info = std::make_shared<data::UserFollowRelInfo>();
    info->setId(CacheJsonInt64(v, "id"));
    info->setFollowerId(CacheJsonInt64(v, "follower_id"));
    info->setFollowingId(CacheJsonInt64(v, "following_id"));
    info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
    info->setCreateTime(CacheJsonTime(v, "create_time"));
    info->setUpdateTime(CacheJsonTime(v, "update_time"));
    info->markClean();
    return info;
}

static data::UserFollowRelInfo::ptr GetUserFollowRelCache(int64_t id)
{
    std::string key = UserFollowRelCacheKey(id);
    std::string data = RedisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        RedisCacheDel(key);
        return nullptr;
    }
    return ParseUserFollowRelCache(v);
}

static void SetUserFollowRelCache(const data::UserFollowRelInfo::ptr& info)
{
    if (!info) {
        return;
    }
    RedisCacheSetex(UserFollowRelCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

data::UserFollowRelInfo::ptr UserFollowRelManager::ParseRow(chen::ISQLData::ptr rt)
{
    return data::UserFollowRelInfoDao::ParseRow(rt);
}

void UserFollowRelManager::add(data::UserFollowRelInfo::ptr info)
{
    SetUserFollowRelCache(info);
}

data::UserFollowRelInfo::ptr UserFollowRelManager::get(int64_t id)
{
    auto v = GetUserFollowRelCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::UserFollowRelInfoDao::Query(id, db);
    if (v) {
        SetUserFollowRelCache(v);
    }
    return v;
}

data::UserFollowRelInfo::ptr UserFollowRelManager::getByFollowerAndFollowing(int64_t follower_id, int64_t following_id)
{
    std::string ck = "flw:" + std::to_string(follower_id) + ":" + std::to_string(following_id);
    int64_t cachedId = GetCachedIdMapping(ck);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::UserFollowRelInfoDao::QueryByFollowerIdFollowingId(follower_id, following_id, db);
    if (info) {
        SetUserFollowRelCache(info);
        CacheIdMapping(ck, info->getId());
    }
    return info;
}

data::UserFollowRelInfo::ptr UserFollowRelManager::follow(int64_t follower_id, int64_t following_id)
{
    auto existing = getByFollowerAndFollowing(follower_id, following_id);
    if (existing) {
        if (!existing->getIsDeleted()) {
            return existing;
        }
        auto db = GetDB();
        if (!db) {
            ERROR(logger) << "Get DB connection fail";
            return nullptr;
        }
        existing->setIsDeleted(0);
        existing->setUpdateTime(time(0));
        if (data::UserFollowRelInfoDao::Update(existing, db)) {
            ERROR(logger) << "UserFollowRelManager follow Update fail";
            return nullptr;
        }
        SetUserFollowRelCache(existing);
        return existing;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    auto info = std::make_shared<data::UserFollowRelInfo>();
    info->setFollowerId(follower_id);
    info->setFollowingId(following_id);
    info->setIsDeleted(0);
    info->setCreateTime(time(0));
    info->setUpdateTime(time(0));

    if (data::UserFollowRelInfoDao::Insert(info, db)) {
        ERROR(logger) << "UserFollowRelManager follow Insert fail";
        return nullptr;
    }

    SetUserFollowRelCache(info);
    return info;
}

bool UserFollowRelManager::unfollow(int64_t follower_id, int64_t following_id)
{
    auto info = getByFollowerAndFollowing(follower_id, following_id);
    if (!info || info->getIsDeleted()) {
        return false;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    info->setIsDeleted(1);
    info->setUpdateTime(time(0));
    if (data::UserFollowRelInfoDao::Update(info, db)) {
        ERROR(logger) << "UserFollowRelManager unfollow Update fail";
        return false;
    }
    SetUserFollowRelCache(info);
    return true;
}

bool UserFollowRelManager::isFollowing(int64_t follower_id, int64_t following_id)
{
    auto info = getByFollowerAndFollowing(follower_id, following_id);
    return info && !info->getIsDeleted();
}

void UserFollowRelManager::listFollowing(std::vector<data::UserFollowRelInfo::ptr>& results, int64_t follower_id, uint64_t offset, uint64_t size)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = data::UserFollowRelInfoDao::newQuery();
    qb->where("follower_id", "=", follower_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);
    if (data::UserFollowRelInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
}

void UserFollowRelManager::listFollowers(std::vector<data::UserFollowRelInfo::ptr>& results, int64_t following_id, uint64_t offset, uint64_t size)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = data::UserFollowRelInfoDao::newQuery();
    qb->where("following_id", "=", following_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);
    if (data::UserFollowRelInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
}

int64_t UserFollowRelManager::countFollowing(int64_t follower_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::UserFollowRelInfoDao::newQuery();
    qb->where("follower_id", "=", follower_id);
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    if (qb->executeCount(total, db)) {
        ERROR(logger) << "executeCount fail errno=" << db->getErrno();
        return 0;
    }
    return total;
}

int64_t UserFollowRelManager::countFollowers(int64_t following_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::UserFollowRelInfoDao::newQuery();
    qb->where("following_id", "=", following_id);
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    if (qb->executeCount(total, db)) {
        ERROR(logger) << "executeCount fail errno=" << db->getErrno();
        return 0;
    }
    return total;
}

void UserFollowRelManager::listMutualFollowing(std::vector<int64_t>& user_ids, int64_t uid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }

    // 我关注的人
    auto qb = data::UserFollowRelInfoDao::newQuery();
    qb->select("following_id");
    qb->where("follower_id", "=", uid);
    qb->where("is_deleted", "=", (int64_t)0);
    std::vector<int64_t> following_ids;
    if (qb->queryColumn<int64_t>(following_ids, db, "following_id")) {
        ERROR(logger) << "listMutualFollowing queryColumn fail";
        return;
    }
    if (following_ids.empty()) {
        return;
    }
    std::unordered_set<int64_t> following_set(following_ids.begin(), following_ids.end());

    // 粉丝中同时是我关注的人 = 互相关注
    auto qb2 = data::UserFollowRelInfoDao::newQuery();
    qb2->select("follower_id");
    qb2->where("following_id", "=", uid);
    qb2->where("is_deleted", "=", (int64_t)0);
    qb2->orderBy("id", "DESC");
    std::vector<int64_t> follower_ids;
    if (qb2->queryColumn<int64_t>(follower_ids, db, "follower_id")) {
        ERROR(logger) << "listMutualFollowing queryColumn fail";
        return;
    }
    user_ids.reserve(std::min(following_ids.size(), follower_ids.size()));
    for (auto follower_id : follower_ids) {
        if (following_set.count(follower_id)) {
            user_ids.push_back(follower_id);
        }
    }
}
