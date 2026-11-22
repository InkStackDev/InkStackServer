/**
 * @file ConversationService.hpp
 * @brief 私信用例：会话创建、消息收发、已读与隐藏
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/ConversationInfo.hpp"
#include "DB/ConversationMessageInfo.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class ConversationService {
public:
    /// 单条消息最大长度
    static const size_t kMaxMessageLen = 2000;

    /**
     * @brief 获取或创建两人会话（跨 worker 依赖唯一索引 + 回查）
     */
    static DB::ConversationInfo::ptr GetOrCreate(int64_t uid_a, int64_t uid_b);

    static void ListConversations(std::vector<DB::ConversationInfo::ptr>& results, int64_t uid);

    static void ListMessages(std::vector<DB::ConversationMessageInfo::ptr>& results,
        int64_t conversation_id,
        int32_t offset,
        int32_t limit);

    /**
     * @brief 发送消息并更新会话冗余信息与接收方未读数
     */
    static DB::ConversationMessageInfo::ptr SendMessage(int64_t conversation_id,
        int64_t sender_id,
        const std::string& text);

    /**
     * @brief 清空本端未读数并把对方消息标记为已读
     */
    static bool MarkRead(int64_t conversation_id, int64_t uid);

    /**
     * @brief 在本端隐藏会话（不影响对方与消息数据）
     */
    static bool DeleteForUser(int64_t conversation_id, int64_t uid);
};
