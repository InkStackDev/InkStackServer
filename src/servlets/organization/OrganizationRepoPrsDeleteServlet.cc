/**
 * @file OrganizationRepoPrsDeleteServlet.cc
 * @brief 组织仓库 PR 删除接口实现
 * @author Christins
 * @date 2026-06-24
 * @copyright Apache 2.0
 */
#include "OrganizationRepoPrsDeleteServlet.hpp"

#include "../../Repository/OrganizationRepoPrRepository.hpp"
#include "../../Service/OrganizationRepoService.hpp"
#include "../../Util/Util.hpp"

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

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        auto pr = OrganizationRepoPrRepository::Get(id, GetDB());
        if (!pr) {
            result->setErrno(PARAM_INVALID, "PR 记录不存在");
            break;
        }

        DB::OrganizationReposInfo::ptr repo;
        auto rt = OrganizationRepoService::CheckRepoAccess(viewer, pr->getRepoId(), true, repo);
        if (rt == OrganizationRepoService::Result::NotFound) {
            result->setErrno(ORG_REPO_NOT_FOUND);
            break;
        }
        if (rt == OrganizationRepoService::Result::NotMember) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt == OrganizationRepoService::Result::AccessDenied) {
            result->setErrno(ACCESS_DENIED, "仅研发主管及以上角色可删除审查记录");
            break;
        }
        if (rt != OrganizationRepoService::Result::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        auto db = GetDB();
        if (!db) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        if (OrganizationRepoPrRepository::DeleteById(id, db)) {
            result->setErrno(DB_OPERATION_FAILED, "删除审查记录失败");
            break;
        }

        OrganizationRepoPrRepository::Evict(id);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
