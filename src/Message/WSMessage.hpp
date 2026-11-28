/**
 * @file WSMessage.hpp
 * @brief WebSocket 下行消息包装
 * @author Christins
 * @date 2026-11-11
 * @copyright Apache 2.0
 */
#pragma once

#include <string>

#include <Chen/Util/JsonUtil.hpp>

#include <json/json.h>

/**
 * @brief 为下行 JSON 添加 channel 字段
 * @param message 原始 JSON 字符串
 * @param channel system/notification/presence/chat
 * @return 添加 channel 后的 JSON；解析失败时原样返回
 */
inline std::string BuildChannelMessage(const std::string& message, const std::string& channel)
{
    if (message.empty() || channel.empty()) {
        return message;
    }

    Json::Value root;
    if (!chen::JsonUtil::FromString(root, message) || !root.isObject()) {
        return message;
    }
    root["channel"] = channel;
    return chen::JsonUtil::ToString(root);
}
