#include "RdRequirementServlet.hpp"

#include "../../Service/RdListService.hpp"
#include "../../Service/RequirementService.hpp"
#include "../../Util/Util.hpp"
#include "RdHelper.hpp"

RdRequirementServlet::RdRequirementServlet()
    : BlogLoginedServlet("RdRequirementServlet")
{
}

int32_t RdRequirementServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t id = request->getParamAs<int64_t>("id", 0);
        int64_t org_id = request->getParamAs<int64_t>("org_id", 0);

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        if (id) {
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
            break;
        }

        uint64_t page = request->getParamAs<uint64_t>("page", 1);
        uint64_t page_size = request->getParamAs<uint64_t>("page_size", 10);
        uint64_t offset = 0;
        uint64_t size = 0;
        ParsePagination(page, page_size, offset, size);

        RdListQueryRepository::Filter filter;
        filter.offset = offset;
        filter.limit = size;
        filter.search = request->getParam("search");
        std::string status_str = request->getParam("status");
        if (!status_str.empty()) {
            filter.status = StringToRequirementStatus(status_str);
        }
        std::string priority_str = request->getParam("priority");
        if (!priority_str.empty()) {
            filter.priority = StringToPriority(priority_str);
        }

        std::vector<DB::RequirementInfo::ptr> requirements;
        int64_t total = 0;
        if (!RdListService::ListRequirements(viewer, org_id, filter, requirements, total)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value arr(Json::arrayValue);
        for (const auto& info : requirements) {
            Json::Value item;
            BuildRequirementJson(item, info);
            arr.append(item);
        }

        result->set("total", total);
        result->set("list", arr);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
