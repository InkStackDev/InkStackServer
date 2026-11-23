#include "ConversationMessageRepository.hpp"

#include <algorithm>

#include <Chen/Util/TimeUtil.hpp>

bool ConversationMessageRepository::Insert(DB::ConversationMessageInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::ConversationMessageInfoDao::Insert(info, conn) == 0;
}

void ConversationMessageRepository::ListByConversation(std::vector<DB::ConversationMessageInfo::ptr>& results,
    int64_t conversation_id,
    int32_t offset,
    int32_t limit,
    chen::IDB::ptr conn)
{
    if (!conn || conversation_id <= 0) {
        return;
    }
    auto qb = DB::ConversationMessageInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->where("conversation_id", "=", conversation_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    qb->limit(limit);
    qb->offset(offset);

    std::vector<DB::ConversationMessageInfo::ptr> tmp;
    if (DB::ConversationMessageInfoDao::QueryByBuilder(tmp, qb, conn)) {
        return;
    }
    std::reverse(tmp.begin(), tmp.end());
    results.swap(tmp);
}

bool ConversationMessageRepository::MarkConversationRead(int64_t conversation_id,
    int64_t reader_id,
    chen::IDB::ptr conn)
{
    if (!conn || conversation_id <= 0) {
        return false;
    }
    auto qb = DB::ConversationMessageInfoDao::newQuery();
    if (!qb) {
        return false;
    }
    int64_t now = chen::TimeUtil::GetCurrentSec();
    qb->set("is_read", (int64_t)1);
    qb->set("read_time", now);
    qb->set("update_time", now);
    qb->where("conversation_id", "=", conversation_id);
    qb->where("sender_id", "!=", reader_id);
    qb->where("is_read", "=", (int64_t)0);
    qb->where("is_deleted", "=", (int64_t)0);
    return qb->executeUpdate(conn) == 0;
}
