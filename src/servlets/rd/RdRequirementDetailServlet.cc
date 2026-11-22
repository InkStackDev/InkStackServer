#include "RdRequirementDetailServlet.hpp"

#include "../../Service/RequirementService.hpp"
#include "../../Util/Util.hpp"
#include "RdHelper.hpp"

RdRequirementDetailServlet::RdRequirementDetailServlet()
    : BlogLoginedServlet("RdRequirementDetailServlet")
{
}

int32_t RdRequirementDetailServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t id = request->getParamAs<int64_t>("id", 0);
        if (!id) {
            result->setErrno(PARAM_MISSING, "id is required");
            break;
        }

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        DB::RequirementInfo::ptr info;
        auto rt = RequirementService::GetDetail(viewer, id, info);
        if (rt == RequirementService::ViewResult::NotFound) {
            result->setErrno(REQUIREMENT_NOT_FOUND);
            break;
        }
        if (rt == RequirementService::ViewResult::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt != RequirementService::ViewResult::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value data;
        BuildRequirementJson(data, info);
        result->set("data", data);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
