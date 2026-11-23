#include "ConversationService.hpp"

#include <algorithm>
#include <memory>
#include <mutex>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Repository/ConversationMessageRepository.hpp"
#include "../Repository/ConversationRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

namespace {
/// 会话读改写锁（未读数、隐藏标志），避免同一 worker 内并发丢更新
std::mutex g_conversation_mutex;
} // namespace

DB::ConversationInfo::ptr ConversationService::GetOrCreate(int64_t uid_a, int64_t uid_b)
{
    if (uid_a <= 0 || uid_b <= 0) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(g_conversation_mutex);
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "GetOrCreate: get db failed";
        return nullptr;
    }

    auto conv = ConversationRepository::GetByUsers(uid_a, uid_b, db);
    if (!conv) {
        auto info = std::make_shared<DB::ConversationInfo>();
        info->setUserAId(ConversationRepository::NormalizeUserA(uid_a, uid_b));
        info->setUserBId(ConversationRepository::NormalizeUserB(uid_a, uid_b));
        info->setLastMessageId(0);
        info->setLastSenderId(0);
        info->setLastMessage("");
        info->setLastMessageTime(0);
        info->setUnreadA(0);
        info->setUnreadB(0);
        info->setUserADeleted(0);
        info->setUserBDeleted(0);
        info->setIsDeleted(0);
        int64_t now = chen::TimeUtil::GetCurrentSec();
        info->setCreateTime(now);
        info->setUpdateTime(now);

        if (!ConversationRepository::Insert(info, db)) {
            // 并发下唯一索引冲突时回查
            conv = ConversationRepository::GetByUsers(uid_a, uid_b, db);
            if (!conv) {
                ERROR(logger) << "GetOrCreate insert failed, a=" << uid_a << " b=" << uid_b;
            }
            return conv;
        }
        ConversationRepository::Put(info);
        return info;
    }

    // 本端曾隐藏过该会话，重新发起时复活
    bool need_update = false;
    if (conv->getUserAId() == uid_a && conv->getUserADeleted()) {
        conv->setUserADeleted(0);
        need_update = true;
    } else if (conv->getUserBId() == uid_a && conv->getUserBDeleted()) {
        conv->setUserBDeleted(0);
        need_update = true;
    }
    if (need_update) {
        conv->setUpdateTime(chen::TimeUtil::GetCurrentSec());
        if (ConversationRepository::Update(conv, db)) {
            ConversationRepository::Put(conv);
        }
    }
    return conv;
}

ConversationService::GetResult ConversationService::GetForUser(int64_t conversation_id,
    int64_t uid,
    DB::ConversationInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return GetResult::DbError;
    }
    auto conv = ConversationRepository::Get(conversation_id, db);
    if (!conv) {
        return GetResult::NotFound;
    }
    if (uid != conv->getUserAId() && uid != conv->getUserBId()) {
        return GetResult::AccessDenied;
    }
    out = conv;
    return GetResult::Ok;
}

void ConversationService::ListConversations(std::vector<DB::ConversationInfo::ptr>& results, int64_t uid)
{
    ConversationRepository::ListForUser(results, uid, GetDB());
}

void ConversationService::ListMessages(std::vector<DB::ConversationMessageInfo::ptr>& results,
    int64_t conversation_id,
    int32_t offset,
    int32_t limit)
{
    ConversationMessageRepository::ListByConversation(results, conversation_id, offset, limit, GetDB());
}

DB::ConversationMessageInfo::ptr ConversationService::SendMessage(int64_t conversation_id,
    int64_t sender_id,
    const std::string& text)
{
    std::lock_guard<std::mutex> lock(g_conversation_mutex);
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "SendMessage: get db failed";
        return nullptr;
    }
    auto trans = db->openTransaction();
    if (!trans) {
        ERROR(logger) << "SendMessage: open transaction failed";
        return nullptr;
    }

    auto msg = std::make_shared<DB::ConversationMessageInfo>();
    msg->setConversationId(conversation_id);
    msg->setSenderId(sender_id);
    msg->setContent(text);
    msg->setIsRead(0);
    msg->setReadTime(0);
    msg->setIsDeleted(0);
    int64_t now = chen::TimeUtil::GetCurrentSec();
    msg->setCreateTime(now);
    msg->setUpdateTime(now);

    if (!ConversationMessageRepository::Insert(msg, db)) {
        ERROR(logger) << "SendMessage insert failed, conversation_id=" << conversation_id;
        trans->rollback();
        return nullptr;
    }

    auto conv = ConversationRepository::Get(conversation_id, db);
    if (!conv) {
        ERROR(logger) << "SendMessage conversation not found, id=" << conversation_id;
        trans->rollback();
        return nullptr;
    }

    conv->setLastMessageId(msg->getId());
    conv->setLastSenderId(sender_id);
    conv->setLastMessage(text);
    conv->setLastMessageTime(msg->getCreateTime());
    conv->setUpdateTime(msg->getCreateTime());
    if (sender_id == conv->getUserAId()) {
        conv->setUnreadB(conv->getUnreadB() + 1);
        // 对端曾隐藏过该会话，收到新消息时复活
        if (conv->getUserBDeleted()) {
            conv->setUserBDeleted(0);
        }
    } else {
        conv->setUnreadA(conv->getUnreadA() + 1);
        if (conv->getUserADeleted()) {
            conv->setUserADeleted(0);
        }
    }
    if (!ConversationRepository::Update(conv, db)) {
        ERROR(logger) << "SendMessage update conversation failed, id=" << conversation_id;
        trans->rollback();
        return nullptr;
    }
    if (!trans->commit()) {
        ERROR(logger) << "SendMessage commit failed";
        return nullptr;
    }

    ConversationRepository::Put(conv);
    return msg;
}

bool ConversationService::MarkRead(int64_t conversation_id, int64_t uid)
{
    std::lock_guard<std::mutex> lock(g_conversation_mutex);
    auto db = GetDB();
    if (!db) {
        return false;
    }

    auto conv = ConversationRepository::Get(conversation_id, db);
    if (!conv) {
        return false;
    }
    if (uid == conv->getUserAId()) {
        conv->setUnreadA(0);
    } else if (uid == conv->getUserBId()) {
        conv->setUnreadB(0);
    } else {
        return false;
    }
    conv->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!ConversationRepository::Update(conv, db)) {
        return false;
    }

    if (!ConversationMessageRepository::MarkConversationRead(conversation_id, uid, db)) {
        ERROR(logger) << "MarkRead messages update failed, conversation_id=" << conversation_id;
    }
    ConversationRepository::Put(conv);
    return true;
}

bool ConversationService::DeleteForUser(int64_t conversation_id, int64_t uid)
{
    std::lock_guard<std::mutex> lock(g_conversation_mutex);
    auto db = GetDB();
    if (!db) {
        return false;
    }

    auto conv = ConversationRepository::Get(conversation_id, db);
    if (!conv) {
        return false;
    }
    if (uid == conv->getUserAId()) {
        if (conv->getUserADeleted()) {
            return true;
        }
        conv->setUserADeleted(1);
    } else if (uid == conv->getUserBId()) {
        if (conv->getUserBDeleted()) {
            return true;
        }
        conv->setUserBDeleted(1);
    } else {
        return false;
    }
    conv->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!ConversationRepository::Update(conv, db)) {
        return false;
    }
    ConversationRepository::Put(conv);
    return true;
}
