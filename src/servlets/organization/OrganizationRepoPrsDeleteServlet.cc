/**
 * @file OrganizationRepoPrsDeleteServlet.cc
 * @brief 组织仓库 PR 删除接口实现
 * @author Christins
 * @date 2026-06-24
 * @copyright Apache 2.0
 */
#include "OrganizationRepoPrsDeleteServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Manager/UserManager.hpp"
#include "../../Repository/OrganizationRepoPrRepository.hpp"
#include "../../Util/Util.hpp"
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"
#include "../../Repository/OrganizationRepoRepository.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationRepoPrsDeleteServlet::OrganizationRepoPrsDeleteServlet()
    : BlogLoginedServlet("OrganizationRepoPrsDeleteServlet")
{
}

int32_t OrganizationRepoPrsDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        auto pr = OrganizationRepoPrRepository::Get(id, GetDB());
        if (!pr) {
            result->setErrno(PARAM_INVALID, "PR 记录不存在");
            break;
        }

        auto repo = OrganizationRepoRepository::Get(pr->getRepoId(), GetDB());
        if (!repo) {
            result->setErrno(ORG_REPO_NOT_FOUND);
            break;
        }
        int64_t org_id = repo->getOrgId();

        // 权限：系统管理员或组织内研发主管及以上
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
                result->setErrno(ACCESS_DENIED, "仅研发主管及以上角色可删除审查记录");
                break;
            }
        }

        auto db = GetDB();
        if (!db) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        if (OrganizationRepoPrRepository::DeleteById(id, db)) {
            result->setErrno(DB_OPERATION_FAILED, "删除审查记录失败");
            ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }

        OrganizationRepoPrRepository::Evict(id);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
