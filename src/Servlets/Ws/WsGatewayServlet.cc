#include "WsGatewayServlet.hpp"

#include "../../Auth/AuthService.hpp"
#include "../../Auth/SessionStore.hpp"
#include "../../ErrorCodes.hpp"
#include "../../Message/WSConnectionRegistry.hpp"
#include "../../Message/MessageRouter.hpp"
#include "../../Metrics/MetricsService.hpp"
#include "../../SessionData.hpp"
#include "../../Util/Util.hpp"
#include "ChatHandler.hpp"

#include <Chen/Log/Log.hpp>
#include <Chen/Util/JsonUtil.hpp>
#include "DB/UserInfo.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

static void SendFrame(chen::http::WSSession::ptr session, const std::string& message)
{
    if (!session || !session->isConnected()) {
        return;
    }
    if (WSRegistry::GetInstance()->sendToSession(session, message) != 0) {
        session->sendMessage(message);
    }
}

static void SendError(chen::http::WSSession::ptr session, int32_t errno_, const std::string& errstr)
{
    if (!session) {
        return;
    }

    Json::Value err;
    err["channel"] = "system";
    err["type"] = "error";
    err["errno"] = errno_;
    err["errstr"] = errstr;
    SendFrame(session, chen::JsonUtil::ToString(err));
}

WSGatewayServlet::WSGatewayServlet()
    : WSServlet("WSGateway")
{
}

int32_t WSGatewayServlet::onConnect(chen::http::HttpRequest::ptr header, chen::http::WSSession::ptr session)
{
    if (!header || !session) {
        return -1;
    }

    const std::string remote_ip = GetRemoteIP(header, session);
    std::string session_id = header->getCookie(CookieKey::kSessionKey);
    if (session_id.empty()) {
        RecordWsConnect(false, "no_cookie");
        INFO(logger) << "[WS] onConnect FAIL: no session cookie, ip=" << remote_ip;
        SendError(session, NOT_LOGIN, "Please login first");
        return -1;
    }

    int64_t uid = 0;
    DB::UserInfo::ptr user;
    if (!AuthService::ResolveWs(header, uid, user) || !user) {
        RecordWsConnect(false, "not_logged_in");
        INFO(logger) << "[WS] onConnect FAIL: not logged in, ip=" << remote_ip;
        SendError(session, NOT_LOGIN, "Please login first");
        return -1;
    }

    std::string device_id = SessionStore::HashId(session_id);
    if (device_id.empty()) {
        RecordWsConnect(false, "invalid_session");
        ERROR(logger) << "[WS] onConnect FAIL: empty device id uid=" << uid << ", ip=" << remote_ip;
        SendError(session, INTERNAL_ERROR, "Invalid session");
        return -1;
    }

    auto result = WSRegistry::GetInstance()->addConnection(uid, device_id, session, false);
    RecordWsConnect(true, "");
    if (result.replaced_session && result.replaced_session != session) {
        Json::Value replaced;
        replaced["channel"] = "system";
        replaced["type"] = "replaced";
        replaced["reason"] = "connected_elsewhere";
        WSRegistry::GetInstance()->closeSession(result.replaced_session, chen::JsonUtil::ToString(replaced));
        // 旧连接 onClose 不一定会立刻触发，这里主动清理旧 session 映射
        WSRegistry::GetInstance()->removeConnection(result.replaced_session);
        INFO(logger) << "[WS] replace old connection: uid=" << uid << " device=" << device_id.substr(0, 12);
    }

    Json::Value connected;
    connected["channel"] = "system";
    connected["type"] = "connected";
    connected["uid"] = uid;
    connected["device_id"] = device_id;
    connected["online_users"] = static_cast<Json::Int64>(MessageRouter::Get()->OnlineUserCount());
    SendFrame(session, chen::JsonUtil::ToString(connected));

    if (result.presence_changed) {
        MessageRouter::Get()->BroadcastPresence();
    }

    INFO(logger) << "[WS] onConnect OK: uid=" << uid << " device=" << device_id.substr(0, 12);
    return 0;
}

int32_t WSGatewayServlet::onClose(chen::http::HttpRequest::ptr, chen::http::WSSession::ptr session)
{
    if (!session) {
        return 0;
    }

    auto result = WSRegistry::GetInstance()->removeConnection(session);
    if (result.removed) {
        RecordWsDisconnect();
        INFO(logger) << "[WS] onClose: uid=" << result.user_id << " device=" << result.device_id.substr(0, 12);
    }
    return 0;
}

int32_t WSGatewayServlet::handle(chen::http::HttpRequest::ptr header, chen::http::WSFrameMessage::ptr msg, chen::http::WSSession::ptr session)
{
    if (!header || !msg || !session) {
        return -1;
    }

    int64_t uid = 0;
    std::string device_id;
    if (!WSRegistry::GetInstance()->getIdentity(session, uid, device_id)) {
        SendError(session, NOT_LOGIN, "Not logged in");
        return -1;
    }

    // 文本心跳：网页发 "ping"，服务端回 "pong"（4 字节）
    if (msg->getData() == "ping") {
        SendFrame(session, "pong");
        return 0;
    }

    Json::Value root;
    if (!chen::JsonUtil::FromString(root, msg->getData()) || !root.isObject()) {
        SendError(session, PARAM_INVALID, "Invalid frame");
        return 0;
    }

    std::string type = root.get("type", "").asString();
    if (type == "send" || type == "read") {
        if (ChatHandler::Handle(uid, root, session) != 0) {
            SendError(session, PARAM_INVALID, "Unsupported frame type");
        }
        return 0;
    }

    SendError(session, PARAM_INVALID, "Unsupported frame type");
    return 0;
}
