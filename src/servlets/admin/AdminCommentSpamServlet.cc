#include "AdminCommentSpamServlet.hpp"

#include "../../Event/EventDefine.hpp"
#include "../../Manager/UserManager.hpp"
#include "../../Service/CommentService.hpp"
#include "../../Util/Util.hpp"

#include <json/json.h>

#include <sstream>
#include "../../Repository/CommentRepository.hpp"
#include "../../Domain/CommentTypes.hpp"

AdminCommentSpamServlet::AdminCommentSpamServlet()
    : BlogLoginedServlet("AdminCommentSpamServlet")
{
}

int32_t AdminCommentSpamServlet::handle(
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
        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        int32_t role = current_user->getRole();
        if (role != UserManager::Role::ADMIN) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        DEFINE_AND_CHECK_STRING(result, ids_str, "ids");

        std::vector<int64_t> ids;
        std::stringstream ss(ids_str);
        std::string token;
        while (std::getline(ss, token, ',')) {
            try {
                ids.push_back(chen::TypeUtil::Atoi(token));
            } catch (...) {
            }
        }

        int64_t affected = CommentService::BatchUpdateStatus(ids, CommentStatus::SPAM);

        Json::Value idList(Json::arrayValue);
        for (auto& id : ids) {
            idList.append(std::to_string(id));
        }
        result->set("ids", idList);
        result->set("affected", affected);
        result->setErrno(SUCCESS);

        // Notify comment authors
        for (auto& cid : ids) {
            auto comment = CommentRepository::Get(cid, GetDB());
            if (!comment) {
                continue;
            }
            int64_t author_id = comment->getUserId();
            if (author_id == uid) {
                continue;
            }

            // 评论被标记为垃圾事件
            {
                EventCommentData data = {};
                data.comment_id = cid;
                data.author_id = author_id;
                data.type = "comment_spam";

                chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_COMMENT, std::move(data));
            }
        }
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
