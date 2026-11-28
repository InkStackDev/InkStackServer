#include "NotificationRepository.hpp"

#include <memory>

#include "../Cache/EntityCache.hpp"

struct NotificationRepositoryCodec {
    static DB::NotificationInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::NotificationInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setUserId(CacheJsonInt64(v, "user_id"));
        info->setTitle(CacheJsonString(v, "title"));
        info->setContent(CacheJsonString(v, "content"));
        info->setTitleId(CacheJsonString(v, "title_id"));
        info->setContentId(CacheJsonString(v, "content_id"));
        info->setParams(CacheJsonString(v, "params"));
        info->setType(CacheJsonString(v, "type"));
        info->setSenderId(CacheJsonInt64(v, "sender_id"));
        info->setArticleId(CacheJsonInt64(v, "article_id"));
        info->setCommentId(CacheJsonInt64(v, "comment_id"));
        info->setIsRead(CacheJsonInt32(v, "is_read"));
        info->setReadTime(CacheJsonTime(v, "read_time"));
        info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
        info->setCreateTime(CacheJsonTime(v, "create_time"));
        info->setUpdateTime(CacheJsonTime(v, "update_time"));
        info->setIsBroadcast(CacheJsonInt32(v, "is_broadcast"));
        info->setLevel(CacheJsonString(v, "level"));
        info->setStartTime(CacheJsonInt64(v, "start_time"));
        info->setEndTime(CacheJsonInt64(v, "end_time"));
        info->markClean();
        return info;
    }
};

const EntityCache<DB::NotificationInfo, NotificationRepositoryCodec> g_notification_cache{{"notif", 120}};

DB::NotificationInfo::ptr NotificationRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return g_notification_cache.Get(id, [id, conn]() {
        return DB::NotificationInfoDao::Query(id, conn);
    });
}

bool NotificationRepository::Insert(DB::NotificationInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::NotificationInfoDao::Insert(info, conn) == 0;
}

bool NotificationRepository::Update(DB::NotificationInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::NotificationInfoDao::Update(info, conn) == 0;
}

void NotificationRepository::Put(const DB::NotificationInfo::ptr& info)
{
    g_notification_cache.Put(info);
}

void NotificationRepository::Evict(int64_t id)
{
    g_notification_cache.Evict(id);
}

bool NotificationRepository::ListUnreadByUser(std::vector<DB::NotificationInfo::ptr>& results,
    int64_t user_id,
    chen::IDB::ptr conn)
{
    if (!conn || user_id <= 0) {
        return false;
    }
    auto qb = DB::NotificationInfoDao::newQuery();
    if (!qb) {
        return false;
    }
    qb->where("user_id", "=", user_id);
    qb->where("is_read", "=", (int64_t)0);
    qb->where("is_deleted", "=", (int64_t)0);
    return DB::NotificationInfoDao::QueryByBuilder(results, qb, conn) == 0;
}

bool NotificationRepository::ListUnreadByUserType(std::vector<DB::NotificationInfo::ptr>& results,
    int64_t user_id,
    const std::string& type,
    chen::IDB::ptr conn)
{
    if (!conn || user_id <= 0 || type.empty()) {
        return false;
    }
    auto qb = DB::NotificationInfoDao::newQuery();
    if (!qb) {
        return false;
    }
    qb->where("user_id", "=", user_id);
    qb->where("is_read", "=", (int64_t)0);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("type", "=", type);
    return DB::NotificationInfoDao::QueryByBuilder(results, qb, conn) == 0;
}

bool NotificationRepository::ClearExpiredBroadcasts(int64_t now, chen::IDB::ptr conn)
{
    if (!conn) {
        return false;
    }
    auto qb = DB::NotificationInfoDao::newQuery();
    if (!qb) {
        return false;
    }
    qb->set("is_broadcast", (int64_t)0);
    qb->set("update_time", now);
    qb->where("is_broadcast", "=", (int64_t)1);
    qb->whereSQL("end_time > ?", (int64_t)0);
    qb->where("end_time", "<=", now);
    return qb->executeUpdate(conn) == 0;
}
