#include "OrganizationReposTokenServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Manager/UserManager.hpp"
#include "../../Service/OrganizationRepoService.hpp"
#include "../../Util/Util.hpp"
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"
#include "../../Repository/OrganizationRepoRepository.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationReposTokenServlet::OrganizationReposTokenServlet()
    : BlogLoginedServlet("OrganizationReposTokenServlet")
{
}

int32_t OrganizationReposTokenServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, repo_id, "repo_id");
        DEFINE_AND_CHECK_STRING(result, token, "token");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        // 检查仓库是否存在
        auto repo = OrganizationRepoRepository::Get(repo_id, GetDB());
        if (!repo) {
            result->setErrno(ORG_REPO_NOT_FOUND);
            break;
        }
        int64_t org_id = repo->getOrgId();

        // 权限检查：系统管理员或组织内研发主管及以上
        auto user = UserMgr::GetInstance()->get(uid);
        if (!user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        int32_t system_role = user->getRole();
        if (system_role != UserManager::Role::ADMIN) {
            auto rel = OrganizationUserRelRepository::GetByOrgAndUser(org_id, uid, GetDB());
            if (!rel || rel->getStatus() != OrgMemberStatus::APPROVED) {
                result->setErrno(ACCESS_DENIED);
                break;
            }
            int32_t org_role = rel->getRole();
            if (org_role != OrgRole::ORG_ADMIN && org_role != OrgRole::DEV_LEAD) {
                result->setErrno(ACCESS_DENIED, "仅研发主管及以上角色可管理仓库 Token");
                break;
            }
        }

        if (OrganizationRepoService::UpdateToken(repo_id, token, repo) != OrganizationRepoService::Result::Ok) {
            result->setErrno(DB_OPERATION_FAILED, "保存 Token 失败");
            break;
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
