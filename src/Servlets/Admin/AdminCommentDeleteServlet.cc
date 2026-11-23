#include "AdminCommentDeleteServlet.hpp"

#include "../../Event/EventDefine.hpp"
#include "../../Service/AdminCommentService.hpp"
#include "../../Service/CommentService.hpp"
#include "../../Util/Util.hpp"

#include <json/json.h>

#include <sstream>
#include "../../Domain/UserTypes.hpp"

AdminCommentDeleteServlet::AdminCommentDeleteServlet()
    : BlogAdminServlet("AdminCommentDeleteServlet")
{
}

int32_t AdminCommentDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);

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

        int64_t affected = CommentService::BatchDelete(ids);

        Json::Value idList(Json::arrayValue);
        for (auto& id : ids) {
            idList.append(std::to_string(id));
        }
        result->set("ids", idList);
        result->set("affected", affected);
        result->setErrno(SUCCESS);

        // Notify comment authors
        for (auto& cid : ids) {
            DB::CommentInfo::ptr comment;
            if (!AdminCommentService::GetComment(cid, comment)) {
                continue;
            }
            int64_t author_id = comment->getUserId();
            if (author_id == uid) {
                continue;
            }
            // 评论被管理员删除事件
            {
                EventCommentData data = {};
                data.comment_id = cid;
                data.author_id = author_id;
                data.type = "comment_admin_deleted";

                chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_COMMENT, std::move(data));
            }
        }
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
