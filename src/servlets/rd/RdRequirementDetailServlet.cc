#include "RdRequirementDetailServlet.hpp"

#include "../../Permission.hpp"
#include "../../Manager/UserManager.hpp"
#include "RdHelper.hpp"
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"
#include "../../Util/Util.hpp"
#include "../../Repository/RequirementRepository.hpp"

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

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        auto user = UserMgr::GetInstance()->get(uid);
        if (!user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        int32_t system_role = user->getRole();
        bool is_platform_admin = (system_role == UserManager::Role::ADMIN);

        auto info = RequirementRepository::Get(id, GetDB());
        if (!info || info->getIsDeleted()) {
            result->setErrno(REQUIREMENT_NOT_FOUND);
            break;
        }

        int64_t info_org_id = info->getOrgId();
        if (!is_platform_admin) {
            auto rel = OrganizationUserRelRepository::GetByOrgAndUser(info_org_id, uid, GetDB());
            if (!rel || rel->getStatus() != OrgMemberStatus::APPROVED) {
                result->setErrno(ACCESS_DENIED);
                break;
            }
            if (!CanViewRequirement(rel->getRole(), uid, info->getCreatorId())) {
                result->setErrno(ACCESS_DENIED);
                break;
            }
        }

        Json::Value data;
        BuildRequirementJson(data, info);
        result->set("data", data);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
