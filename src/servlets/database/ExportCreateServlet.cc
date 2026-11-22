#include "ExportCreateServlet.hpp"

#include "../../Manager/UserManager.hpp"
#include "../../Service/RecordService.hpp"

ExportCreateServlet::ExportCreateServlet()
    : BlogLoginedServlet("ExportCreateServlet")
{
}

int32_t ExportCreateServlet::handle(
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
        if (current_user->getRole() != UserManager::Role::ADMIN) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        std::string type;
        if (!request->checkGetParamAs("type", type) || type.empty()) {
            result->setErrno(PARAM_MISSING, "param type is required");
            break;
        }
        if (type != "articles" && type != "users" && type != "comments" && type != "full") {
            result->setErrno(PARAM_INVALID, "type must be articles/users/comments/full");
            break;
        }

        std::string timestamp = std::to_string(time(0));
        std::string name = "export_" + type + "_" + timestamp;

        auto info = ExportRecordService::Create(uid, type, name);
        if (!info) {
            result->setErrno(DB_OPERATION_FAILED, "create export failed");
            break;
        }

        Json::Value item;
        item["id"] = info->getId();
        item["name"] = info->getName();
        item["type"] = info->getType();
        std::string format = info->getFormat();
        if (format.empty()) {
            format = "json";
        }
        item["format"] = format;
        item["size"] = info->getSize();
        item["record_count"] = info->getRecordCount();
        item["status"] = info->getStatus();
        item["created_at"] = info->getCreateTime();
        item["created_by"] = info->getCreatedBy();

        result->set("data", item);
        result->setErrno(SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
