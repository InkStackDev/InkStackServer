/**
 * @file MessageJson.hpp
 * @brief 私信会话与消息的 JSON 组装工具（供各私信 servlet 共用）
 * @author Christins
 * @date 2026-09-01
 * @copyright Apache 2.0
 */
#pragma once

#include "../../Domain/AccessTypes.hpp"
#include "../../SessionData.hpp"

#include <json/json.h>

#include "DB/ConversationInfo.hpp"
#include "DB/ConversationMessageInfo.hpp"

void BuildMessageJson(Json::Value& json, DB::ConversationMessageInfo::ptr msg, int64_t uid);

void BuildConversationJson(Json::Value& json, DB::ConversationInfo::ptr conv, int64_t uid);

int32_t CheckChatPermission(Result::ptr result, const Viewer& viewer);
