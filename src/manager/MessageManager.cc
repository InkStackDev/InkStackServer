#include "MessageManager.hpp"

#include "../Message/MessageRouter.hpp"
#include "../Repository/ConversationRepository.hpp"
#include "../Util/Util.hpp"
#include "WSConnectionManager.hpp"

DB::ConversationInfo::ptr MessageManager::get(int64_t id)
{
    return ConversationRepository::Get(id, GetDB());
}

DB::ConversationInfo::ptr MessageManager::getByUsers(int64_t uid_a, int64_t uid_b)
{
    return ConversationRepository::GetByUsers(uid_a, uid_b, GetDB());
}

DB::ConversationInfo::ptr MessageManager::getOrCreate(int64_t uid_a, int64_t uid_b)
{
    return ConversationService::GetOrCreate(uid_a, uid_b);
}

void MessageManager::listConversations(std::vector<DB::ConversationInfo::ptr>& results, int64_t uid)
{
    ConversationService::ListConversations(results, uid);
}

void MessageManager::listMessages(std::vector<DB::ConversationMessageInfo::ptr>& results,
    int64_t conversation_id,
    int32_t offset,
    int32_t limit)
{
    ConversationService::ListMessages(results, conversation_id, offset, limit);
}

DB::ConversationMessageInfo::ptr MessageManager::sendMessage(int64_t conversation_id,
    int64_t sender_id,
    const std::string& text)
{
    return ConversationService::SendMessage(conversation_id, sender_id, text);
}

bool MessageManager::markRead(int64_t conversation_id, int64_t uid)
{
    return ConversationService::MarkRead(conversation_id, uid);
}

bool MessageManager::deleteForUser(int64_t conversation_id, int64_t uid)
{
    return ConversationService::DeleteForUser(conversation_id, uid);
}

bool MessageManager::isChatConnected(int64_t user_id)
{
    return MessageRouter::Get()->IsUserOnline(user_id);
}

int32_t MessageManager::sendToUser(int64_t user_id, const std::string& message)
{
    return MessageRouter::Get()->SendToUser(user_id, BuildChannelMessage(message, "chat"));
}
