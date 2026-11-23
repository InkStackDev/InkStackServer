#include "OrganizationAssignmentListServlet.hpp"

#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"

#include <algorithm>

OrganizationAssignmentListServlet::OrganizationAssignmentListServlet()
    : BlogLoginedServlet("OrganizationAssignmentListServlet")
{
}

int32_t OrganizationAssignmentListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");
        // -1 all, 0 draft, 1 active, 2 closed(inactive)
        int32_t status = request->getParamAs<int32_t>("status", -1);

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        uint64_t offset = (page_num - 1) * page_size;

        std::vector<OrganizationService::OrganizationAssignmentView> assignments;
        int64_t total = 0;
        auto rt = OrganizationService::ListAssignments(viewer, org_id, status, offset, page_size, assignments, total);
        if (rt == OrganizationService::ListResult::NotMember) {
            result->setErrno(ORG_NOT_MEMBER);
            break;
        }
        if (rt == OrganizationService::ListResult::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt != OrganizationService::ListResult::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value list;
        for (const auto& item : assignments) {
            Json::Value row;
            row["id"] = item.id;
            row["assign_id"] = item.assign_id;
            row["assigned_by"] = item.assigned_by;
            row["name"] = item.name;
            row["subject_name"] = item.subject_name;
            row["end_time"] = item.end_time;
            row["max_size"] = item.max_size;
            row["status"] = item.status;
            row["priority"] = item.priority;
            row["file_type"] = item.file_type;
            row["description"] = item.description;
            list.append(row);
        }
        result->set("list", list);
        result->set("total", std::min(total, static_cast<int64_t>(list.size())));
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
