#include "RdRequirementDeleteServlet.hpp"

#include "../../Manager/UserManager.hpp"
#include "../../Permission.hpp"
#include "../../Service/RequirementService.hpp"
#include "../../Util/Util.hpp"

#include <json/json.h>

#include <set>
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"

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

        int32_t org_role = 0;
        if (!is_platform_admin) {
            auto rel = OrganizationUserRelRepository::GetByOrgAndUser(org_id, uid, GetDB());
            if (!rel || rel->getStatus() != OrgMemberStatus::APPROVED) {
                result->setErrno(ACCESS_DENIED);
                break;
            }
            org_role = rel->getRole();
        }

        std::vector<DB::RequirementInfo::ptr> delItems;
        if (!RequirementService::DeleteMany(org_id, delIds, uid, org_role, is_platform_admin, delItems)) {
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
