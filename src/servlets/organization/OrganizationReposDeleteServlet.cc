#include "OrganizationReposDeleteServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Manager/OrganizationManager.hpp"
#include "../../Manager/OrganizationRepoManager.hpp"
#include "../../Manager/OrganizationUserRelManager.hpp"
#include "../../Manager/UserManager.hpp"
#include "../../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationReposDeleteServlet::OrganizationReposDeleteServlet()
    : BlogLoginedServlet("OrganizationReposDeleteServlet")
{
}

int32_t OrganizationReposDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        // 检查仓库是否存在且属于该组织
        auto repo = OrganizationRepoMgr::GetInstance()->get(id);
        if (!repo || repo->getOrgId() != org_id) {
            result->setErrno(ORG_REPO_NOT_FOUND, "仓库不存在");
            break;
        }

        // 检查用户权限：需要研发主管及以上（role >= 4），系统管理员可管理任意组织仓库
        auto user = UserMgr::GetInstance()->get(uid);
        if (!user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        int32_t system_role = user->getRole();
        bool is_admin = (system_role == UserManager::Role::ADMIN);

        if (!is_admin) {
            auto rel = OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(org_id, uid);
            if (!rel || rel->getStatus() != OrganizationUserRelManager::Status::APPROVED) {
                result->setErrno(ACCESS_DENIED, "无权访问该组织");
                break;
            }
            int32_t org_role = rel->getRole();
            if (org_role != OrganizationManager::Role::ORG_ADMIN && org_role != OrganizationManager::Role::DEV_LEAD) {
                result->setErrno(ACCESS_DENIED, "仅研发主管及以上角色可删除仓库");
                break;
            }
        }

        auto db = GetDB();
        if (!db) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        if (DB::OrganizationReposInfoDao::DeleteById(id, db)) {
            result->setErrno(DB_OPERATION_FAILED, "删除仓库失败");
            ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }

        OrganizationRepoMgr::GetInstance()->del(id);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
