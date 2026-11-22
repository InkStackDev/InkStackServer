#include "UserAssignmentListServlet.hpp"

#include "../../Service/UserQueryService.hpp"
#include "../../Util/Util.hpp"

UserAssignmentListServlet::UserAssignmentListServlet()
    : BlogLoginedServlet("UserAssignmentListServlet")
{
}

int32_t UserAssignmentListServlet::handle(
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

        std::vector<UserQueryService::AssignmentItem> assignments;
        if (!UserQueryService::ListAssignments(viewer.user_id, assignments)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value list;
        for (const auto& assign : assignments) {
            Json::Value item;
            item["id"] = assign.id;
            item["name"] = assign.name;
            item["subject_name"] = assign.subject_name;
            item["end_time"] = assign.end_time;
            item["status"] = assign.status;
            item["priority"] = assign.priority;
            item["create_time"] = assign.create_time;
            item["description"] = assign.description;
            item["file_type"] = assign.file_type;
            item["file_size"] = assign.file_size;
            list.append(item);
        }
        result->set("list", list);
        result->set("total", (int64_t)list.size());
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
