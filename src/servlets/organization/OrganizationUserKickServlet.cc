#include "OrganizationUserKickServlet.hpp"

#include <json/json.h>

#include "../../Permission.hpp"
#include "../../Event/EventDefine.hpp"
#include "../../Manager/OrganizationManager.hpp"
#include "../../Manager/OrganizationUserRelManager.hpp"
#include "../../Manager/UserManager.hpp"
#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"

OrganizationUserKickServlet::OrganizationUserKickServlet()
    : BlogLoginedServlet("OrganizationUserKickServlet")
{
}

int32_t OrganizationUserKickServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");           // organization_user_rel id
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");   // organization id
        DEFINE_AND_CHECK_TYPE(result, int64_t, user_id, "user_id"); // user_id

        // 操作者权限检查
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

        auto operator_rel = OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(org_id, uid);
        if (!operator_rel) {
            result->setErrno(ORG_NOT_MEMBER);
            break;
        }
        int32_t org_role = operator_rel->getRole();

        if (!CanManageMembers(system_role, org_role)) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        DB::OrganizationUserRelInfo::ptr rel;
        auto rt = OrganizationService::Kick(org_id, user_id, rel);
        if (rt == OrganizationService::MemberResult::RelNotFound) {
            result->setErrno(ARTICLE_INVALID_ID);
            break;
        }
        if (rt != OrganizationService::MemberResult::Ok) {
            result->setErrno(ORG_USER_REL_FAILED);
            break;
        }

        // Notify org admins and kicked user
        {
            auto org = OrganizationMgr::GetInstance()->get(org_id);
            auto kicked_user = UserMgr::GetInstance()->get(user_id);
            auto oper_user = UserMgr::GetInstance()->get(uid);
            std::string org_name = org ? org->getName() : std::to_string(org_id);
            std::string kicked_name = kicked_user ? kicked_user->getName() : std::to_string(user_id);
            std::string oper_name = oper_user ? oper_user->getName() : std::to_string(uid);

            EventOrgMemberData data = {};
            data.type = "kicked";
            data.org_id = org_id;
            data.org_name = org_name;
            data.operator_id = uid;
            data.operator_name = oper_name;
            data.kicked_user_id = user_id;
            data.applicant_name = kicked_name;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ORG_MEMBER, std::move(data));
        }

        result->set("success", true);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
