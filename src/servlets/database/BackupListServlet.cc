#include "BackupListServlet.hpp"

#include "../../Manager/UserManager.hpp"
#include "../../Repository/RecordRepository.hpp"

BackupListServlet::BackupListServlet()
    : BlogLoginedServlet("BackupListServlet")
{
}

int32_t BackupListServlet::handle(
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

        std::string search = request->getParam("search");
        std::string type = request->getParam("type");
        std::string status = request->getParam("status");
        int32_t page = 1, pageSize = 15;
        request->checkGetParamAs("page", page);
        request->checkGetParamAs("pageSize", pageSize);
        if (page < 1)
            page = 1;
        if (pageSize < 1)
            pageSize = 15;
        if (pageSize > 100)
            pageSize = 100;

        int32_t offset = (page - 1) * pageSize;

        std::vector<DB::BackupRecordInfo::ptr> list;
        int64_t total = 0;
        if (!BackupRecordRepository::ListByPages(list, total, search, type, status, offset, pageSize, GetDB())) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value arr(Json::arrayValue);
        for (auto& info : list) {
            Json::Value item;
            item["id"] = info->getId();
            item["name"] = info->getName();
            item["type"] = info->getType();
            item["size"] = info->getSize();
            item["file_count"] = info->getFileCount();
            item["status"] = info->getStatus();
            item["created_at"] = info->getCreateTime();
            item["completed_at"] = info->getCompletedAt();
            auto created_by_user = UserMgr::GetInstance()->get(info->getCreatedBy());
            if (created_by_user) {
                item["created_by"] = created_by_user->getName();
            }
            item["description"] = info->getDescription();
            if (!info->getFilePath().empty()) {
                item["download_url"] = "/api/v1/admin/database/backups/" + std::to_string(info->getId()) + "/download";
            }
            arr.append(item);
        }

        result->set("list", arr);
        result->set("total", total);
        result->setErrno(SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
