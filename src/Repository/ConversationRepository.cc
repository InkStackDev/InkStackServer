#include "ConversationRepository.hpp"

#include <memory>

#include "../Cache/EntityCache.hpp"

namespace {

struct ConversationCodec {
    static DB::ConversationInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::ConversationInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setUserAId(CacheJsonInt64(v, "user_a_id"));
        info->setUserBId(CacheJsonInt64(v, "user_b_id"));
        info->setLastMessageId(CacheJsonInt64(v, "last_message_id"));
        info->setLastSenderId(CacheJsonInt64(v, "last_sender_id"));
        info->setLastMessage(CacheJsonString(v, "last_message"));
        info->setLastMessageTime(CacheJsonInt64(v, "last_message_time"));
        info->setUnreadA(CacheJsonInt32(v, "unread_a"));
        info->setUnreadB(CacheJsonInt32(v, "unread_b"));
        info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
        info->setCreateTime(CacheJsonTime(v, "create_time"));
        info->setUpdateTime(CacheJsonTime(v, "update_time"));
        info->setUserADeleted(CacheJsonInt32(v, "user_a_deleted"));
        info->setUserBDeleted(CacheJsonInt32(v, "user_b_deleted"));
        info->markClean();
        return info;
    }
};

const EntityCache<DB::ConversationInfo, ConversationCodec> g_conversation_cache{{"conv", 300}};

} // namespace

DB::ConversationInfo::ptr ConversationRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return g_conversation_cache.Get(id, [id, conn]() {
        return DB::ConversationInfoDao::Query(id, conn);
    });
}

DB::ConversationInfo::ptr ConversationRepository::GetByUsers(int64_t uid_a, int64_t uid_b, chen::IDB::ptr conn)
{
    if (!conn) {
        return nullptr;
    }
    auto qb = DB::ConversationInfoDao::newQuery();
    if (!qb) {
        return nullptr;
    }
    qb->where("user_a_id", "=", NormalizeUserA(uid_a, uid_b));
    qb->where("user_b_id", "=", NormalizeUserB(uid_a, uid_b));
    qb->where("is_deleted", "=", (int64_t)0);
    std::vector<DB::ConversationInfo::ptr> results;
    if (DB::ConversationInfoDao::QueryByBuilder(results, qb, conn)) {
        return nullptr;
    }
    if (results.empty()) {
        return nullptr;
    }
    Put(results[0]);
    return results[0];
}

bool ConversationRepository::Insert(DB::ConversationInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::ConversationInfoDao::Insert(info, conn) == 0;
}

bool ConversationRepository::Update(DB::ConversationInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::ConversationInfoDao::Update(info, conn) == 0;
}

void ConversationRepository::Put(const DB::ConversationInfo::ptr& info)
{
    g_conversation_cache.Put(info);
}

void ConversationRepository::Evict(int64_t id)
{
    g_conversation_cache.Evict(id);
}
