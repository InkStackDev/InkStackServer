#include "AssignmentDeleteServlet.hpp"

#include "../../Event/EventDefine.hpp"
#include "../../Service/AssignmentService.hpp"
#include "../../Util/Util.hpp"

#include <json/json.h>

#include <set>

AssignmentDeleteServlet::AssignmentDeleteServlet()
    : BlogLoginedServlet("AssignmentDeleteServlet")
{
}

int32_t AssignmentDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, ids, "ids");
        std::set<int64_t> assignment_id_set;
        for (auto& i : chen::StringUtil::Split(ids, ",")) {
            if (!i.empty()) {
                assignment_id_set.insert(chen::TypeUtil::Atoi(i));
            }
        }
        std::vector<int64_t> assignment_ids(assignment_id_set.begin(), assignment_id_set.end());

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        if (!viewer.is_platform_admin) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        std::vector<DB::AssignmentInfo::ptr> deleted;
        if (!AssignmentService::DeleteMany(assignment_ids, deleted)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }
        if (deleted.empty()) {
            break;
        }

        Json::Value json_ids;
        for (const auto& info : deleted) {
            json_ids.append(info->getId());
        }
        result->set("ids", json_ids);

        for (const auto& assign : deleted) {
            std::vector<int64_t> submitters;
            AssignmentService::ListSubmitters(assign->getId(), submitters);
            std::set<int64_t> notified;
            for (auto submitter_id : submitters) {
                if (!notified.insert(submitter_id).second) {
                    continue;
                }

                EventAssignmentData data = {};
                data.type = "deleted";
                data.assignment_id = assign->getId();
                data.assignment_name = assign->getName();
                data.submitter_id = submitter_id;

                chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ASSIGNMENT, std::move(data));
            }
        }
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
