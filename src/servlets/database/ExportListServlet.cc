#include "ExportListServlet.hpp"

#include "../../Service/RecordService.hpp"
#include "../../Domain/UserTypes.hpp"
#include "../../Util/Util.hpp"

ExportListServlet::ExportListServlet()
    : BlogAdminServlet("ExportListServlet")
{
}

int32_t ExportListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {

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

        std::vector<DB::ExportRecordInfo::ptr> list;
        int64_t total = 0;
        if (!ExportRecordService::List(list, total, search, type, status, offset, pageSize)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value arr(Json::arrayValue);
        for (auto& info : list) {
            Json::Value item;
            item["id"] = info->getId();
            item["name"] = info->getName();
            item["type"] = info->getType();
            std::string format = info->getFormat();
            if (format.empty())
                format = "json";
            item["format"] = format;
            item["size"] = info->getSize();
            item["record_count"] = info->getRecordCount();
            item["status"] = info->getStatus();
            item["created_at"] = info->getCreateTime();
            item["created_by"] = info->getCreatedBy();
            if (!info->getFilePath().empty()) {
                item["download_url"] = "/api/v1/admin/database/exports/" + std::to_string(info->getId()) + "/download";
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
