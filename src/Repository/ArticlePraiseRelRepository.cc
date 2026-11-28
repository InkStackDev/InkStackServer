#include "ArticlePraiseRelRepository.hpp"

#include <memory>

#include <Chen/DB/Redis.hpp>

#include "../Cache/EntityCache.hpp"
#include "../Cache/CacheUtil.hpp"

struct ArticlePraiseRelRepositoryCodec {
    static DB::ArticlePraiseRelInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::ArticlePraiseRelInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setUserId(CacheJsonInt64(v, "user_id"));
        info->setArticleId(CacheJsonInt64(v, "article_id"));
        info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
        info->setCreateTime(CacheJsonTime(v, "create_time"));
        info->setUpdateTime(CacheJsonTime(v, "update_time"));
        info->markClean();
        return info;
    }
};

const EntityCache<DB::ArticlePraiseRelInfo, ArticlePraiseRelRepositoryCodec> g_article_praise_cache{{"apra", 300}};

static std::string UserArticleKey(int64_t user_id, int64_t article_id)
{
    return "pra:" + std::to_string(user_id) + ":" + std::to_string(article_id);
}

DB::ArticlePraiseRelInfo::ptr ArticlePraiseRelRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return g_article_praise_cache.Get(id, [id, conn]() {
        return DB::ArticlePraiseRelInfoDao::Query(id, conn);
    });
}

DB::ArticlePraiseRelInfo::ptr ArticlePraiseRelRepository::GetByUserAndArticle(int64_t user_id,
    int64_t article_id,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return nullptr;
    }
    const std::string key = UserArticleKey(user_id, article_id);
    int64_t cached_id = GetCachedIdMapping(key);
    if (cached_id > 0) {
        auto info = Get(cached_id, conn);
        if (info) {
            return info;
        }
        RedisCacheDel("cache:map:" + key);
    }

    auto info = DB::ArticlePraiseRelInfoDao::QueryByUserIdArticleId(user_id, article_id, conn);
    if (info) {
        Put(info);
        CacheIdMapping(key, info->getId());
    }
    return info;
}

bool ArticlePraiseRelRepository::Insert(DB::ArticlePraiseRelInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    if (DB::ArticlePraiseRelInfoDao::Insert(info, conn) != 0) {
        return false;
    }
    CacheIdMapping(UserArticleKey(info->getUserId(), info->getArticleId()), info->getId());
    return true;
}

bool ArticlePraiseRelRepository::Update(DB::ArticlePraiseRelInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::ArticlePraiseRelInfoDao::Update(info, conn) == 0;
}

void ArticlePraiseRelRepository::Put(const DB::ArticlePraiseRelInfo::ptr& info)
{
    if (!info) {
        return;
    }
    g_article_praise_cache.Put(info);
    CacheIdMapping(UserArticleKey(info->getUserId(), info->getArticleId()), info->getId());
}

void ArticlePraiseRelRepository::Evict(int64_t id)
{
    g_article_praise_cache.Evict(id);
}

void ArticlePraiseRelRepository::ListByArticle(std::vector<DB::ArticlePraiseRelInfo::ptr>& results,
    int64_t article_id,
    uint64_t offset,
    uint64_t size,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return;
    }
    auto qb = DB::ArticlePraiseRelInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->where("article_id", "=", article_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);
    DB::ArticlePraiseRelInfoDao::QueryByBuilder(results, qb, conn);
}

void ArticlePraiseRelRepository::ListByUser(std::vector<DB::ArticlePraiseRelInfo::ptr>& results,
    int64_t user_id,
    uint64_t offset,
    uint64_t size,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return;
    }
    auto qb = DB::ArticlePraiseRelInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->where("user_id", "=", user_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);
    DB::ArticlePraiseRelInfoDao::QueryByBuilder(results, qb, conn);
}

int64_t ArticlePraiseRelRepository::CountByArticle(int64_t article_id, chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::ArticlePraiseRelInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("article_id", "=", article_id);
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    if (qb->executeCount(total, conn)) {
        return 0;
    }
    return total;
}

int64_t ArticlePraiseRelRepository::CountByUser(int64_t user_id, chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::ArticlePraiseRelInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("user_id", "=", user_id);
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    if (qb->executeCount(total, conn)) {
        return 0;
    }
    return total;
}
