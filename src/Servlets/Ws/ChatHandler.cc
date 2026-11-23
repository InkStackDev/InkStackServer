#include "ChatHandler.hpp"

#include "../../Manager/WSConnectionManager.hpp"
#include "../Message/MessageJson.hpp"

#include <Chen/Log/Log.hpp>
#include <Chen/Util/JsonUtil.hpp>
#include "../../Domain/UserTypes.hpp"
#include "../../Service/UserQueryService.hpp"
#include "../../Service/ConversationService.hpp"
#include "../../Message/MessageRouter.hpp"
#include "../../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

static void SendChatError(chen::http::WSSession::ptr session,
    int32_t errno_,
    const std::string& errstr,
    const std::string& client_id = "")
{
    if (!session || !session->isConnected()) {
        return;
    }

    Json::Value err;
    err["channel"] = "chat";
    err["type"] = "send_error";
    if (!client_id.empty()) {
        err["client_id"] = client_id;
    }
    err["errno"] = errno_;
    err["errstr"] = errstr;

    std::string message = chen::JsonUtil::ToString(err);
    if (WSMgr::GetInstance()->sendToSession(session, message) != 0) {
        session->sendMessage(message);
    }
}

static bool CheckConversationAccess(chen::http::WSSession::ptr session,
    int64_t uid,
    int64_t conversation_id,
    DB::ConversationInfo::ptr& conv,
    int64_t& peer_id,
    const std::string& client_id = "")
{
    auto get_rt = ConversationService::GetForUser(conversation_id, uid, conv);
    if (get_rt == ConversationService::GetResult::NotFound) {
        SendChatError(session, MESSAGE_CONVERSATION_NOT_FOUND, "Conversation not found", client_id);
        return false;
    }
    if (get_rt == ConversationService::GetResult::AccessDenied) {
        SendChatError(session, ACCESS_DENIED, "Access denied", client_id);
        return false;
    }
    if (get_rt != ConversationService::GetResult::Ok) {
        SendChatError(session, DB_OPERATION_FAILED, "Database error", client_id);
        return false;
    }
    peer_id = conv->getUserAId() == uid ? conv->getUserBId() : conv->getUserAId();
    return true;
}

static int32_t HandleSend(int64_t uid, const Json::Value& root, chen::http::WSSession::ptr session)
{
    std::string client_id = root.get("client_id", "").asString();
    int64_t conversation_id = root.get("conversation_id", 0).asInt64();
    std::string text = root.get("text", "").asString();

    if (conversation_id <= 0 || text.empty()) {
        SendChatError(session, PARAM_MISSING, "conversation_id and text required", client_id);
        return 0;
    }
    if (text.size() > ConversationService::kMaxMessageLen) {
        SendChatError(session, MESSAGE_TEXT_TOO_LONG, "Message too long", client_id);
        return 0;
    }

    DB::ConversationInfo::ptr conv;
    int64_t peer_id = 0;
    if (!CheckConversationAccess(session, uid, conversation_id, conv, peer_id, client_id)) {
        return 0;
    }

    auto msg_info = ConversationService::SendMessage(conversation_id, uid, text);
    if (!msg_info) {
        SendChatError(session, MESSAGE_SEND_FAILED, "Send failed", client_id);
        return 0;
    }

    INFO(logger) << "[ChatWS] send OK: uid=" << uid << " conv=" << conversation_id << " msg_id=" << msg_info->getId() << " client_id=" << client_id;

    // ACK 只回发起设备，client_id 是单标签页的
    {
        Json::Value ack;
        ack["channel"] = "chat";
        ack["type"] = "message_ack";
        ack["client_id"] = client_id;
        ack["conversation_id"] = conversation_id;
        Json::Value mj;
        BuildMessageJson(mj, msg_info, uid);
        ack["message"] = mj;
        ack["recipient_online"] = MessageRouter::Get()->IsUserOnline(peer_id);
        WSMgr::GetInstance()->sendToSession(session, chen::JsonUtil::ToString(ack));
    }

    // 推送给接收方所有设备
    if (MessageRouter::Get()->IsUserOnline(peer_id)) {
        Json::Value push;
        push["type"] = "message";
        push["conversation_id"] = conversation_id;
        Json::Value mj;
        BuildMessageJson(mj, msg_info, peer_id);
        push["message"] = mj;
        SendChatToUser(peer_id, chen::JsonUtil::ToString(push));
    }

    INFO(logger) << "[ChatWS] ack->" << uid << " push->" << peer_id
                 << " peer_online=" << MessageRouter::Get()->IsUserOnline(peer_id);
    return 0;
}

static int32_t HandleRead(int64_t uid, const Json::Value& root, chen::http::WSSession::ptr session)
{
    int64_t conversation_id = root.get("conversation_id", 0).asInt64();
    if (conversation_id <= 0) {
        SendChatError(session, PARAM_MISSING, "conversation_id required");
        return 0;
    }

    DB::ConversationInfo::ptr conv;
    int64_t peer_id = 0;
    if (!CheckConversationAccess(session, uid, conversation_id, conv, peer_id)) {
        return 0;
    }

    ConversationService::MarkRead(conversation_id, uid);

    // 已读回执给对端所有设备
    if (MessageRouter::Get()->IsUserOnline(peer_id)) {
        Json::Value rcpt;
        rcpt["type"] = "read";
        rcpt["conversation_id"] = conversation_id;
        rcpt["unread"] = 0;
        SendChatToUser(peer_id, chen::JsonUtil::ToString(rcpt));
    }
    return 0;
}

int32_t ChatHandler::Handle(int64_t uid, const Json::Value& root, chen::http::WSSession::ptr session)
{
    if (uid <= 0 || !session) {
        return -1;
    }

    std::string type = root.get("type", "").asString();
    if (type != "send" && type != "read") {
        return -1;
    }

    // 兜底：连接鉴权之外的二次校验，防止角色变动后仍可用私信
    int32_t role = 0;
    bool deleted = false;
    if (!UserQueryService::GetRole(uid, role, deleted) || deleted || role == UserRole::USER) {
        SendChatError(
            session, ACCESS_DENIED, "Chat not available for normal users", root.get("client_id", "").asString());
        return 0;
    }

    if (type == "send") {
        return HandleSend(uid, root, session);
    }
    return HandleRead(uid, root, session);
}
