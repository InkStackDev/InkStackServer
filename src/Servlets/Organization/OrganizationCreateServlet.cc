#include "OrganizationCreateServlet.hpp"

#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"

OrganizationCreateServlet::OrganizationCreateServlet()
    : BlogLoginedServlet("OrganizationCreateServlet")
{
}

int32_t OrganizationCreateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, name, "name");
        DEFINE_AND_CHECK_STRING(result, type, "type");
        DEFINE_AND_CHECK_TYPE(result, int32_t, status, "status");
        std::string desc = request->getParamAs<std::string>("desc");
        int64_t oid = request->getParamAs<int64_t>("id", 0);

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        OrganizationInput input;
        input.id = oid;
        input.name = name;
        input.type = type;
        input.description = desc;
        input.status = status;
        input.owner_id = viewer.user_id;

        DB::OrganizationInfo::ptr info;
        bool created = false;
        auto rt = OrganizationService::CreateOrUpdate(viewer, input, info, created);
        if (rt == OrganizationService::Result::NotFound) {
            result->setErrno(ORG_NOT_FOUND);
            break;
        }
        if (rt == OrganizationService::Result::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt != OrganizationService::Result::Ok) {
            result->setErrno(ORG_UPDATE_FAILED);
            break;
        }

        result->set("id", info->getId());
        result->set("name", info->getName());
        result->set("type", info->getType());
        result->set("status", info->getStatus());
        result->set("description", info->getDescription());
        result->set("create_time", info->getCreateTime());
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
