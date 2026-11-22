#include "RdCheckAccessServlet.hpp"

#include "../../Permission.hpp"
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"
#include "../../Util/Util.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Domain/UserTypes.hpp"

RdCheckAccessServlet::RdCheckAccessServlet()
    : BlogLoginedServlet("RdCheckAccessServlet")
{
}

int32_t RdCheckAccessServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t current_uid = getUserId(request);
        DB::UserInfo::ptr currentUser = UserRepository::Get(current_uid, GetDB());
        if (!currentUser || currentUser->getState() != 1) {
            result->setErrno(RD_INVALID_USER);
            break;
        }

        int64_t target_uid = current_uid;
        std::string uidParam = request->getParam("user_id");
        if (!uidParam.empty()) {
            target_uid = atol(uidParam.c_str());
            // 只有管理员可以查询其他用户的平台访问权限
            if (target_uid != current_uid && currentUser->getRole() != UserRole::ADMIN) {
                result->setErrno(ACCESS_DENIED);
                break;
            }
        }

        DB::UserInfo::ptr targetUser = UserRepository::Get(target_uid, GetDB());
        if (!targetUser || targetUser->getState() != 1) {
            result->setErrno(SUCCESS);
            result->set("can_access", "0");
            result->set("reason", "user not found or disabled");
            break;
        }

        int32_t system_role = targetUser->getRole();
        std::vector<DB::OrganizationUserRelInfo::ptr> user_orgs;
        OrganizationUserRelRepository::ListByUser(user_orgs, target_uid, OrgMemberStatus::APPROVED, true, GetDB());

        int32_t highest_org_role = 0;
        for (auto& rel : user_orgs) {
            if (rel->getRole() > highest_org_role) {
                highest_org_role = rel->getRole();
            }
        }

        bool canAccess = CanAccessPlatform(system_role, highest_org_role);

        result->setErrno(SUCCESS);
        result->set("can_access", canAccess ? "1" : "0");
        if (!canAccess) {
            result->set("reason", "requires system admin or organization role >= reporter");
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
