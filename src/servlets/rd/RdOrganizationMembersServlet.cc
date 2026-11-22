#include "RdOrganizationMembersServlet.hpp"

#include "../../Util/Util.hpp"
#include "RdHelper.hpp"
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"

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

        int64_t uid = getUserId(request);
        auto rel = OrganizationUserRelRepository::GetByOrgAndUser(org_id, uid, GetDB());
        if (!rel || rel->getStatus() != OrgMemberStatus::APPROVED) {
            result->setErrno(ORG_NOT_MEMBER);
            break;
        }

        std::vector<DB::OrganizationUserRelInfo::ptr> members;
        OrganizationUserRelRepository::ListByOrgPages(
            members, org_id, 0, INT32_MAX, OrgMemberStatus::APPROVED, true, GetDB());

        Json::Value arr(Json::arrayValue);
        for (auto& m : members) {
            if (m->getRole() < OrgRole::REPORTER) {
                continue;
            }
            Json::Value item;
            item["user_id"] = m->getUserId();
            item["name"] = GetUserName(m->getUserId());
            item["role"] = OrgRoleToFrontend(m->getRole());
            item["avatar"] = GetUserAvatar(m->getUserId());
            arr.append(item);
        }

        result->set("list", arr);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
