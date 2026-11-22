#include "NotificationRepository.hpp"

#include <memory>

#include "../Cache/EntityCache.hpp"

namespace {

struct NotificationCodec {
    static DB::NotificationInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::NotificationInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setUserId(CacheJsonInt64(v, "user_id"));
        info->setTitle(CacheJsonString(v, "title"));
        info->setContent(CacheJsonString(v, "content"));
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

const EntityCache<DB::NotificationInfo, NotificationCodec> g_notification_cache{{"notif", 120}};

} // namespace

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
