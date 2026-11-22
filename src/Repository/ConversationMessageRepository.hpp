/**
 * @file ConversationMessageRepository.hpp
 * @brief 私信消息单表读写
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/ConversationMessageInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <vector>

class ConversationMessageRepository {
public:
    static bool Insert(DB::ConversationMessageInfo::ptr info, chen::IDB::ptr conn);

    /**
     * @brief 按倒序取一页后翻转，返回时间正序
     */
    static void ListByConversation(std::vector<DB::ConversationMessageInfo::ptr>& results,
        int64_t conversation_id,
        int32_t offset,
        int32_t limit,
        chen::IDB::ptr conn);

    /**
     * @brief 把会话中对方发来的未读消息标记为已读
     * @return 是否执行成功
     */
    static bool MarkConversationRead(int64_t conversation_id, int64_t reader_id, chen::IDB::ptr conn);
};
