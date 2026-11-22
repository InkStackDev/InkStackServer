/**
 * @file MessageManager.hpp
 * @brief 私信会话与消息管理（会话、消息、未读数、已读标记）
 * @author Christins
 * @date 2026-09-01
 * @copyright Apache 2.0
 */
#pragma once

#include <Chen/Util/Singleton.hpp>

#include "DB/ConversationInfo.hpp"
#include "DB/ConversationMessageInfo.hpp"

#include "../Service/ConversationService.hpp"

#include <cstdint>
#include <string>
#include <vector>

class MessageManager {
public:
    /// 单条消息最大长度
    static const size_t kMaxMessageLen = 2000;

    /// 会话相关
    DB::ConversationInfo::ptr get(int64_t id);

    DB::ConversationInfo::ptr getByUsers(int64_t uid_a, int64_t uid_b);

    DB::ConversationInfo::ptr getOrCreate(int64_t uid_a, int64_t uid_b);

    void listConversations(std::vector<DB::ConversationInfo::ptr>& results, int64_t uid);

    /// 消息相关
    void listMessages(std::vector<DB::ConversationMessageInfo::ptr>& results,
        int64_t conversation_id,
        int32_t offset,
        int32_t limit);

    DB::ConversationMessageInfo::ptr sendMessage(int64_t conversation_id, int64_t sender_id, const std::string& text);

    bool markRead(int64_t conversation_id, int64_t uid);

    /// 在本端隐藏会话（不影响对方与消息数据）
    bool deleteForUser(int64_t conversation_id, int64_t uid);

    /// 聊天 WS 推送（连接统一由 WSMgr 管理）
    bool isChatConnected(int64_t user_id);

    int32_t sendToUser(int64_t user_id, const std::string& message);

    /// 参与者归一化：user_a_id 恒为较小值
    static int64_t NormalizeUserA(int64_t uid_a, int64_t uid_b) { return uid_a < uid_b ? uid_a : uid_b; }

    static int64_t NormalizeUserB(int64_t uid_a, int64_t uid_b) { return uid_a < uid_b ? uid_b : uid_a; }
};

typedef chen::Singleton<MessageManager> MessageMgr;
