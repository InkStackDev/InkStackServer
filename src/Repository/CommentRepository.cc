#include "CommentRepository.hpp"

#include <memory>

#include "../Cache/EntityCache.hpp"
#include "../Manager/CacheUtil.hpp"

namespace {

struct CommentCodec {
    static DB::CommentInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::CommentInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setArticleId(CacheJsonInt64(v, "article_id"));
        info->setUserId(CacheJsonInt64(v, "user_id"));
        info->setParentId(CacheJsonInt64(v, "parent_id"));
        info->setContent(CacheJsonString(v, "content"));
        info->setIp(CacheJsonString(v, "ip"));
        info->setUserAgent(CacheJsonString(v, "user_agent"));
        info->setStatus(CacheJsonInt32(v, "status"));
        info->setIsReported(CacheJsonInt32(v, "is_reported"));
        info->setReportCount(CacheJsonInt32(v, "report_count"));
        info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
        info->setCreateTime(CacheJsonTime(v, "create_time"));
        info->setUpdateTime(CacheJsonTime(v, "update_time"));
        info->markClean();
        return info;
    }
};

const EntityCache<DB::CommentInfo, CommentCodec> g_comment_cache{{"cmt", 300}};

} // namespace

DB::CommentInfo::ptr CommentRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return g_comment_cache.Get(id, [id, conn]() {
        return DB::CommentInfoDao::Query(id, conn);
    });
}

bool CommentRepository::Insert(DB::CommentInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::CommentInfoDao::Insert(info, conn) == 0;
}

bool CommentRepository::Update(DB::CommentInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::CommentInfoDao::Update(info, conn) == 0;
}

bool CommentRepository::InsertOrUpdate(DB::CommentInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::CommentInfoDao::InsertOrUpdate(info, conn) == 0;
}

void CommentRepository::Put(const DB::CommentInfo::ptr& info)
{
    g_comment_cache.Put(info);
}

void CommentRepository::Evict(int64_t id)
{
    g_comment_cache.Evict(id);
}
