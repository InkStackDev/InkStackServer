#include "RdRequirementDeleteServlet.hpp"

#include "../../Service/RequirementService.hpp"
#include "../../Util/Util.hpp"

#include <json/json.h>

#include <set>

RdRequirementDeleteServlet::RdRequirementDeleteServlet()
    : BlogLoginedServlet("RdRequirementDeleteServlet")
{
}

int32_t RdRequirementDeleteServlet::handle(chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");
        std::string idsStr = request->getParam("ids");

        std::set<int64_t> delIds;
        if (id) {
            delIds.insert(id);
        }
        if (!idsStr.empty()) {
            for (auto& s : chen::StringUtil::Split(idsStr, ",")) {
                delIds.insert(chen::TypeUtil::Atoi(s));
            }
        }

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        std::vector<DB::RequirementInfo::ptr> delItems;
        auto rt = RequirementService::DeleteMany(viewer, org_id, delIds, delItems);
        if (rt == RequirementService::DeleteResult::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt != RequirementService::DeleteResult::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }
        if (!delItems.empty()) {
            Json::Value json_ids;
            for (auto& i : delItems) {
                json_ids.append(i->getId());
            }
            result->set("ids", json_ids);
        }
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
