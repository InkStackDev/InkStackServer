#include "MessageJson.hpp"

#include "../../Domain/UserTypes.hpp"
#include "../../Message/MessageRouter.hpp"
#include "../../Service/UserQueryService.hpp"
#include "../../Util/Util.hpp"

void BuildMessageJson(Json::Value& json, DB::ConversationMessageInfo::ptr msg, int64_t uid)
{
    json["id"] = msg->getId();
    json["from_me"] = (msg->getSenderId() == uid);
    json["text"] = msg->getContent();
    json["create_time"] = msg->getCreateTime();
}

void BuildConversationJson(Json::Value& json, DB::ConversationInfo::ptr conv, int64_t uid)
{
    int64_t peer_id = (conv->getUserAId() == uid) ? conv->getUserBId() : conv->getUserAId();
    int32_t unread = (conv->getUserAId() == uid) ? conv->getUnreadA() : conv->getUnreadB();

    json["id"] = conv->getId();
    json["peer_id"] = peer_id;
    json["online"] = MessageRouter::Get()->IsUserOnline(peer_id);

    UserQueryService::Brief peer;
    UserQueryService::GetUserBrief(peer_id, peer);
    json["name"] = peer.name;
    json["avatar"] = peer.avatar;

    json["last_time"] = conv->getLastMessageTime();
    json["unread"] = unread;

    Json::Value arr(Json::arrayValue);
    if (conv->getLastMessageId() > 0) {
        Json::Value last;
        last["id"] = conv->getLastMessageId();
        last["from_me"] = (conv->getLastSenderId() == uid);
        last["text"] = conv->getLastMessage();
        last["create_time"] = conv->getLastMessageTime();
        arr.append(last);
    }
    json["messages"] = arr;
}

int32_t CheckChatPermission(Result::ptr result, const Viewer& viewer)
{
    if (!viewer.valid()) {
        result->setErrno(USER_NOT_FOUND);
        return USER_NOT_FOUND;
    }
    if (viewer.system_role == UserRole::USER) {
        result->setErrno(ACCESS_DENIED);
        return ACCESS_DENIED;
    }
    return SUCCESS;
}
