#include "OrganizationJoinCheckServlet.hpp"

#include <json/json.h>

#include "../../Permission.hpp"
#include "../../Event/EventDefine.hpp"
#include "../../Manager/UserManager.hpp"
#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationRepository.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"

OrganizationJoinCheckServlet::OrganizationJoinCheckServlet()
    : BlogLoginedServlet("OrganizationJoinCheckServlet")
{
}

int32_t OrganizationJoinCheckServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, user_id, "user_id");
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");
        DEFINE_AND_CHECK_TYPE(result, int32_t, state, "state");

        if (state != OrgMemberStatus::REJECTED && state != OrgMemberStatus::APPROVED) {
            result->setErrno(PARAM_INVALID, "invalid state");
            break;
        }

        auto user = UserMgr::GetInstance()->get(user_id);
        auto org = OrganizationRepository::Get(org_id, GetDB());
        if (!user || !org) {
            result->setErrno(ARTICLE_INVALID_ID);
            break;
        }

        // check operater permission
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        int32_t system_role = current_user->getRole();

        auto operator_rel = OrganizationUserRelRepository::GetByOrgAndUser(org_id, uid, GetDB());
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
        auto rt = OrganizationService::SetMemberState(org_id, user_id, state, rel);
        if (rt == OrganizationService::MemberResult::RelNotFound) {
            result->setErrno(ARTICLE_INVALID_ID);
            break;
        }
        if (rt == OrganizationService::MemberResult::InvalidState) {
            result->setErrno(ORG_INVALID_STATE);
            break;
        }
        if (rt != OrganizationService::MemberResult::Ok) {
            result->setErrno(ORG_USER_REL_FAILED);
            break;
        }

        // Notify applicant
        {
            EventOrgMemberData data = {};
            data.type = (state == OrgMemberStatus::APPROVED) ? "join_approved" : "join_rejected";
            data.org_id = org_id;
            data.org_name = org->getName();
            data.applicant_id = user_id;
            data.operator_id = uid;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ORG_MEMBER, std::move(data));
        }

        result->set("success", true);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
