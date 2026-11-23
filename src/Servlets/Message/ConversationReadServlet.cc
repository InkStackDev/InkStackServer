#include "ConversationReadServlet.hpp"

#include "MessageJson.hpp"
#include "../../Service/ConversationService.hpp"
#include "../../Util/Util.hpp"

ConversationReadServlet::ConversationReadServlet()
    : BlogLoginedServlet("ConversationReadServlet")
{
}

int32_t ConversationReadServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        if (CheckChatPermission(result, viewer) != SUCCESS) {
            break;
        }
        int64_t uid = viewer.user_id;

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

        DB::ConversationInfo::ptr conv;
        auto get_rt = ConversationService::GetForUser(conversation_id, uid, conv);
        if (get_rt == ConversationService::GetResult::NotFound) {
            result->setErrno(MESSAGE_CONVERSATION_NOT_FOUND);
            break;
        }
        if (get_rt == ConversationService::GetResult::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (get_rt != ConversationService::GetResult::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        if (!ConversationService::MarkRead(conversation_id, uid)) {
            result->setErrno(DB_OPERATION_FAILED, "mark read failed");
            break;
        }

        result->setErrno(SUCCESS);
        result->set("unread", (int32_t)0);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
