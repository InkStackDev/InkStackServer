#include "ConversationMessageServlet.hpp"

#include <Chen/Http/Http.hpp>
#include <json/json.h>

#include "MessageJson.hpp"
#include "../../Repository/ConversationRepository.hpp"
#include "../../Service/ConversationService.hpp"
#include "../../Message/MessageRouter.hpp"

ConversationMessageServlet::ConversationMessageServlet()
    : BlogLoginedServlet("ConversationMessageServlet")
{
}

int32_t ConversationMessageServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        if (CheckChatPermission(result, uid) != SUCCESS) {
            break;
        }

        std::string id_str = request->getParam("id");
        if (id_str.empty()) {
            result->setErrno(PARAM_MISSING, "param id is required");
            break;
        }
        int64_t conversation_id = chen::TypeUtil::Atoi(id_str);
        if (!conversation_id) {
            result->setErrno(PARAM_INVALID, "invalid id");
            break;
        }

        auto conv = ConversationRepository::Get(conversation_id, GetDB());
        if (!conv) {
            result->setErrno(MESSAGE_CONVERSATION_NOT_FOUND);
            break;
        }
        if (uid != conv->getUserAId() && uid != conv->getUserBId()) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        int64_t peer_id = (conv->getUserAId() == uid) ? conv->getUserBId() : conv->getUserAId();

        if (request->getMethod() == chen::http::HttpMethod::GET) {
            // 会话消息记录
            int32_t offset = request->getParamAs<int32_t>("offset", 0);
            int32_t size = request->getParamAs<int32_t>("size", 50);

            std::vector<DB::ConversationMessageInfo::ptr> msgs;
            ConversationService::ListMessages(msgs, conversation_id, offset, size);

            Json::Value arr(Json::arrayValue);
            for (auto& m : msgs) {
                Json::Value item;
                BuildMessageJson(item, m, uid);
                arr.append(item);
            }

            result->setErrno(SUCCESS);
            result->set("list", arr);
            break;
        }

        if (request->getMethod() == chen::http::HttpMethod::POST) {
            // 发送消息
            std::string text = request->getParam("text");
            if (text.empty()) {
                result->setErrno(PARAM_MISSING, "param text is required");
                break;
            }
            if (text.size() > ConversationService::kMaxMessageLen) {
                result->setErrno(MESSAGE_TEXT_TOO_LONG);
                break;
            }

            auto msg = ConversationService::SendMessage(conversation_id, uid, text);
            if (!msg) {
                result->setErrno(MESSAGE_SEND_FAILED);
                break;
            }

            Json::Value data;
            BuildMessageJson(data, msg, uid);
            result->set("data", data);

            result->setErrno(SUCCESS);

            // HTTP 兜底发送也走聊天 WS 推送给接收方
            Json::Value frame;
            frame["type"] = "message";
            frame["conversation_id"] = conversation_id;
            Json::Value msg_json;
            BuildMessageJson(msg_json, msg, peer_id);
            frame["message"] = msg_json;
            SendChatToUser(peer_id, chen::JsonUtil::ToString(frame));
            break;
        }

        result->setErrno(INVALID_METHOD);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
