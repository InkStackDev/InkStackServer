#include "ArticleRepository.hpp"

#include <memory>

#include "../Cache/EntityCache.hpp"
#include "../Manager/CacheUtil.hpp"

struct ArticleRepositoryCodec {
    static DB::ArticleInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::ArticleInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setUserId(CacheJsonInt64(v, "user_id"));
        info->setTitle(CacheJsonString(v, "title"));
        info->setContent(CacheJsonString(v, "content"));
        info->setType(CacheJsonInt32(v, "type"));
        info->setState(CacheJsonInt32(v, "state"));
        info->setChannel(CacheJsonInt64(v, "channel"));
        info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
        info->setPublishTime(CacheJsonTime(v, "publish_time"));
        info->setWeight(CacheJsonInt64(v, "weight"));
        info->setViews(CacheJsonInt64(v, "views"));
        info->setPraise(CacheJsonInt64(v, "praise"));
        info->setFavorites(CacheJsonInt64(v, "favorites"));
        info->setCreateTime(CacheJsonTime(v, "create_time"));
        info->setUpdateTime(CacheJsonTime(v, "update_time"));
        info->markClean();
        return info;
    }
};

const EntityCache<DB::ArticleInfo, ArticleRepositoryCodec> g_article_cache{{"art", 300}};

DB::ArticleInfo::ptr ArticleRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return g_article_cache.Get(id, [id, conn]() {
        return DB::ArticleInfoDao::Query(id, conn);
    });
}

bool ArticleRepository::Insert(DB::ArticleInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::ArticleInfoDao::Insert(info, conn) == 0;
}

bool ArticleRepository::Update(DB::ArticleInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::ArticleInfoDao::Update(info, conn) == 0;
}

bool ArticleRepository::InsertOrUpdate(DB::ArticleInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::ArticleInfoDao::InsertOrUpdate(info, conn) == 0;
}

void ArticleRepository::Put(const DB::ArticleInfo::ptr& info)
{
    g_article_cache.Put(info);
}

void ArticleRepository::Evict(int64_t id)
{
    g_article_cache.Evict(id);
}
