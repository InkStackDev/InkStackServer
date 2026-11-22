#include "RdBugDetailServlet.hpp"

#include "../../Permission.hpp"
#include "RdHelper.hpp"
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"
#include "../../Util/Util.hpp"
#include "../../Repository/BugRepository.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Domain/UserTypes.hpp"

RdBugDetailServlet::RdBugDetailServlet()
    : BlogLoginedServlet("RdBugDetailServlet")
{
}

int32_t RdBugDetailServlet::handle(
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
        auto user = UserRepository::Get(uid, GetDB());
        if (!user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        int32_t system_role = user->getRole();
        bool is_platform_admin = (system_role == UserRole::ADMIN);

        auto info = BugRepository::Get(id, GetDB());
        if (!info || info->getIsDeleted()) {
            result->setErrno(BUG_NOT_FOUND);
            break;
        }

        int64_t info_org_id = info->getOrgId();
        if (!is_platform_admin) {
            auto rel = OrganizationUserRelRepository::GetByOrgAndUser(info_org_id, uid, GetDB());
            if (!rel || rel->getStatus() != OrgMemberStatus::APPROVED) {
                result->setErrno(ACCESS_DENIED);
                break;
            }
            if (!CanViewBug(rel->getRole(), uid, info->getCreatorId())) {
                result->setErrno(ACCESS_DENIED);
                break;
            }
        }

        Json::Value data;
        BuildBugJson(data, info);
        result->set("data", data);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
