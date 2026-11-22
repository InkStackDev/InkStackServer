#include "RdRequirementEditServlet.hpp"

#include "../../Event/EventDefine.hpp"
#include "../../Service/RequirementService.hpp"
#include "../../Util/Util.hpp"
#include "RdHelper.hpp"

RdRequirementEditServlet::RdRequirementEditServlet()
    : BlogLoginedServlet("RdRequirementEditServlet")
{
}

int32_t RdRequirementEditServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t id = request->getParamAs<int64_t>("id", 0);
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        DEFINE_AND_CHECK_STRING(result, title, "title");

        RequirementInput input;
        input.id = id;
        input.org_id = org_id;
        input.creator_id = viewer.user_id;
        input.title = title;
        input.description = request->getParam("description");

        std::string priority_str = request->getParam("priority");
        if (!priority_str.empty()) {
            int32_t p = StringToPriority(priority_str);
            if (p >= 0) {
                input.priority = p;
            }
        }

        std::string status_str = request->getParam("status");
        if (!status_str.empty()) {
            int32_t s = StringToRequirementStatus(status_str);
            if (s >= 0) {
                input.status = s;
            }
        }

        input.assignee_id = request->getParamAs<int64_t>("assignee_id", 0);
        input.deadline = request->getParamAs<int64_t>("deadline", 0);
        input.iteration = request->getParam("iteration");
        input.category = request->getParam("category");
        input.source = request->getParam("source");

        DB::RequirementInfo::ptr info;
        auto rt = RequirementService::Save(viewer, input, info);
        if (rt == RequirementService::SaveResult::NotFound) {
            result->setErrno(REQUIREMENT_NOT_FOUND);
            break;
        }
        if (rt == RequirementService::SaveResult::NotMember) {
            result->setErrno(ORG_NOT_MEMBER);
            break;
        }
        if (rt == RequirementService::SaveResult::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt != RequirementService::SaveResult::Ok) {
            result->setErrno(RD_UPDATE_FAILED);
            break;
        }

        Json::Value data;
        BuildRequirementJson(data, info);
        result->setData(data);

        notifyAssignee(input.assignee_id, info);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

void RdRequirementEditServlet::notifyAssignee(int64_t assignee_id, DB::RequirementInfo::ptr requirement)
{
    if (!assignee_id) {
        return;
    }

    EventRdAssignData data = {};
    data.type = "requirement";
    data.assignee_id = assignee_id;
    data.item_id = requirement->getId();
    data.item_title = requirement->getTitle();
    data.creator_id = requirement->getCreatorId();

    chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_RD_ASSIGN, std::move(data));
}
