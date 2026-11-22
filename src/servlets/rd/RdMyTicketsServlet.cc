#include "RdMyTicketsServlet.hpp"

#include "../../Service/RdTicketService.hpp"
#include "../../Util/Util.hpp"
#include "RdHelper.hpp"

RdMyTicketsServlet::RdMyTicketsServlet()
    : BlogLoginedServlet("RdMyTicketsServlet")
{
}

int32_t RdMyTicketsServlet::handle(
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

        std::string type = request->getParam("type");
        if (type.empty()) {
            result->setErrno(PARAM_MISSING, "type is required");
            break;
        }

        int64_t org_id = request->getParamAs<int64_t>("org_id", 0);
        uint64_t page = request->getParamAs<uint64_t>("page", 1);
        uint64_t page_size = request->getParamAs<uint64_t>("page_size", 10);
        uint64_t offset = 0;
        uint64_t size = 0;
        ParsePagination(page, page_size, offset, size);

        std::string search = request->getParam("search");
        std::string status_str = request->getParam("status");
        int32_t status = -1;
        if (!status_str.empty()) {
            if (type == "requirement") {
                status = StringToRequirementStatus(status_str);
            } else if (type == "bug") {
                status = StringToBugStatus(status_str);
            } else if (type == "task") {
                status = StringToTaskStatus(status_str);
            }
            if (status < 0) {
                result->set("list", Json::Value(Json::arrayValue));
                result->set("total", 0);
                break;
            }
        }

        std::vector<RdTicketService::Item> items;
        int64_t total = 0;
        auto rt = RdTicketService::ListMine(viewer, type, org_id, status, search, offset, size, items, total);
        if (rt == RdTicketService::Result::InvalidType) {
            result->setErrno(RD_INVALID_TYPE);
            break;
        }
        if (rt != RdTicketService::Result::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value arr(Json::arrayValue);
        for (const auto& item : items) {
            Json::Value row;
            row["id"] = item.id;
            row["title"] = item.title;
            row["description"] = item.description;
            row["priority"] = PriorityToString(item.priority);
            row["creator"] = item.creator;
            row["creator_id"] = item.creator_id;
            row["assignee"] = item.assignee;
            row["assignee_avatar"] = item.assignee_avatar;
            row["assignee_id"] = item.assignee_id;
            row["org_id"] = item.org_id;
            row["org_name"] = item.org_name;
            row["created_at"] = item.created_at;
            row["updated_at"] = item.updated_at;

            if (type == "requirement") {
                row["status"] = RequirementStatusToString(item.status);
                row["deadline"] = item.deadline;
            } else if (type == "bug") {
                row["status"] = BugStatusToString(item.status);
                row["severity"] = SeverityToString(item.severity);
                row["module"] = item.module;
            } else {
                row["status"] = TaskStatusToString(item.status);
                row["deadline"] = item.deadline;
            }
            arr.append(row);
        }

        result->set("list", arr);
        result->set("total", total);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
