#include "RdOrganizationMembersServlet.hpp"

#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"
#include "RdHelper.hpp"

RdOrganizationMembersServlet::RdOrganizationMembersServlet()
    : BlogLoginedServlet("RdOrganizationMembersServlet")
{
}

int32_t RdOrganizationMembersServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        std::vector<OrganizationService::OrganizationMemberView> members;
        auto rt = OrganizationService::ListMembers(viewer, org_id, members);
        if (rt == OrganizationService::ListResult::AccessDenied) {
            result->setErrno(ORG_NOT_MEMBER);
            break;
        }
        if (rt != OrganizationService::ListResult::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value arr(Json::arrayValue);
        for (const auto& member : members) {
            Json::Value item;
            item["user_id"] = member.user_id;
            item["name"] = member.name;
            item["role"] = OrgRoleToFrontend(member.role);
            item["avatar"] = member.avatar;
            arr.append(item);
        }

        result->set("list", arr);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
