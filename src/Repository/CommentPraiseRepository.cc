#include "CommentPraiseRepository.hpp"

#include <memory>
#include <string>

#include <Chen/DB/Redis.hpp>

#include "../Cache/EntityCache.hpp"
#include "../Config/AppConfig.hpp"
#include "../Cache/CacheUtil.hpp"
#include "../Util/Util.hpp"

struct CommentPraiseRepositoryCodec {
    static DB::CommentPraiseRelInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::CommentPraiseRelInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setUserId(CacheJsonInt64(v, "user_id"));
        info->setCommentId(CacheJsonInt64(v, "comment_id"));
        info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
        info->setCreateTime(CacheJsonTime(v, "create_time"));
        info->setUpdateTime(CacheJsonTime(v, "update_time"));
        info->markClean();
        return info;
    }
};

const EntityCache<DB::CommentPraiseRelInfo, CommentPraiseRepositoryCodec> g_praise_cache{{"cpra", 300}};

static std::string UserCommentKey(int64_t user_id, int64_t comment_id)
{
    return "cpra:" + std::to_string(user_id) + ":" + std::to_string(comment_id);
}

static void InvalidateCount(int64_t comment_id)
{
    chen::RedisUtil::Cmd(GetRedisPoolName(), "del cache:count:cpra:cnt:%lld", (long long)comment_id);
}

DB::CommentPraiseRelInfo::ptr CommentPraiseRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return g_praise_cache.Get(id, [id, conn]() {
        return DB::CommentPraiseRelInfoDao::Query(id, conn);
    });
}

DB::CommentPraiseRelInfo::ptr CommentPraiseRepository::GetByUserAndComment(int64_t user_id,
    int64_t comment_id,
    chen::IDB::ptr conn)
{
    if (!conn || user_id <= 0 || comment_id <= 0) {
        return nullptr;
    }
    const std::string key = UserCommentKey(user_id, comment_id);
    int64_t cached_id = GetCachedIdMapping(key);
    if (cached_id > 0) {
        auto info = Get(cached_id, conn);
        if (info) {
            return info;
        }
        RedisCacheDel("cache:map:" + key);
    }

    auto info = DB::CommentPraiseRelInfoDao::QueryByUserIdCommentId(user_id, comment_id, conn);
    if (info) {
        Put(info);
        CacheIdMapping(key, info->getId());
    }
    return info;
}

bool CommentPraiseRepository::Insert(DB::CommentPraiseRelInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    if (DB::CommentPraiseRelInfoDao::Insert(info, conn) != 0) {
        return false;
    }
    InvalidateCount(info->getCommentId());
    return true;
}

bool CommentPraiseRepository::Update(DB::CommentPraiseRelInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    if (DB::CommentPraiseRelInfoDao::Update(info, conn) != 0) {
        return false;
    }
    InvalidateCount(info->getCommentId());
    return true;
}

void CommentPraiseRepository::Put(const DB::CommentPraiseRelInfo::ptr& info)
{
    if (!info) {
        return;
    }
    g_praise_cache.Put(info);
    CacheIdMapping(UserCommentKey(info->getUserId(), info->getCommentId()), info->getId());
}

void CommentPraiseRepository::Evict(int64_t id)
{
    g_praise_cache.Evict(id);
}

int64_t CommentPraiseRepository::CountByComment(int64_t comment_id, chen::IDB::ptr conn)
{
    if (!conn || comment_id <= 0) {
        return 0;
    }
    auto qb = DB::CommentPraiseRelInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("comment_id", "=", comment_id);
    qb->where("is_deleted", "=", (int64_t)0);
    return ExecuteCountCached(qb, conn, "cpra:cnt:" + std::to_string(comment_id));
}

bool CommentPraiseRepository::IsPraising(int64_t user_id, int64_t comment_id, chen::IDB::ptr conn)
{
    auto info = GetByUserAndComment(user_id, comment_id, conn);
    return info && info->getIsDeleted() == 0;
}
