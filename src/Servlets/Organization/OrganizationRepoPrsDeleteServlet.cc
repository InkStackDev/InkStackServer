/**
 * @file OrganizationRepoPrsDeleteServlet.cc
 * @brief 组织仓库 PR 删除接口实现
 * @author Christins
 * @date 2026-06-24
 * @copyright Apache 2.0
 */
#include "OrganizationRepoPrsDeleteServlet.hpp"

#include "../../Service/OrganizationRepoPrService.hpp"
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

        auto rt = OrganizationRepoPrService::Remove(viewer, id);
        if (rt == OrganizationRepoPrService::DeleteStatus::PR_NOT_FOUND) {
            result->setErrno(PARAM_INVALID);
            break;
        }
        if (rt == OrganizationRepoPrService::DeleteStatus::REPO_NOT_FOUND) {
            result->setErrno(ORG_REPO_NOT_FOUND);
            break;
        }
        if (rt == OrganizationRepoPrService::DeleteStatus::NOT_MEMBER) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt == OrganizationRepoPrService::DeleteStatus::ACCESS_DENIED) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt != OrganizationRepoPrService::DeleteStatus::OK) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
