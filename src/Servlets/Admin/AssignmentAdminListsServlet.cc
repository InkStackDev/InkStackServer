#include "AssignmentAdminListsServlet.hpp"

#include "../../Service/AdminAssignmentService.hpp"
#include "../../Util/Util.hpp"

AssignmentAdminListsServlet::AssignmentAdminListsServlet()
    : BlogAdminServlet("AssignmentAdminListsServlet")
{
}

int32_t AssignmentAdminListsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");
        int32_t state = request->getParamAs<int32_t>("state", -1);

        uint64_t offset = (page_num - 1) * page_size;
        std::vector<AdminAssignmentService::Item> infos;
        int64_t total = 0;
        if (!AdminAssignmentService::List(offset, page_size, state, infos, total)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total", total);
        Json::Value list;
        for (const auto& i : infos) {
            Json::Value item;
            item["id"] = i.id;
            item["name"] = i.name;
            item["subject_name"] = i.subject_name;
            item["end_time"] = i.end_time;
            item["status"] = i.status;
            item["priority"] = i.priority;
            item["create_time"] = i.create_time;
            item["description"] = i.description;
            item["file_type"] = i.file_type;
            item["file_size"] = i.file_size;
            if (!i.organization_name.empty()) {
                item["organization_name"] = i.organization_name;
            }
            if (!i.assigned_by.empty()) {
                item["assigned_by"] = i.assigned_by;
            }
            list.append(item);
        }
        result->set("list", list);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
