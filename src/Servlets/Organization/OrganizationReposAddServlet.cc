#include "OrganizationReposAddServlet.hpp"

#include "../../Service/OrganizationRepoService.hpp"
#include "../../Util/Util.hpp"

OrganizationReposAddServlet::OrganizationReposAddServlet()
    : BlogLoginedServlet("OrganizationReposAddServlet")
{
}

int32_t OrganizationReposAddServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");
        DEFINE_AND_CHECK_STRING(result, name, "name");
        DEFINE_AND_CHECK_STRING(result, url, "url");
        std::string language = request->getParamAs<std::string>("language");
        std::string description = request->getParamAs<std::string>("description");
        std::string token = request->getParamAs<std::string>("token");

        if (name.size() > 128) {
            result->setErrno(PARAM_INVALID);
            break;
        }
        if (url.size() > 512) {
            result->setErrno(PARAM_INVALID);
            break;
        }
        if (url.find("github.com") == std::string::npos) {
            result->setErrno(PARAM_INVALID);
            break;
        }
        if (language.size() > 64) {
            result->setErrno(PARAM_INVALID);
            break;
        }

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        OrganizationRepoInput input;
        input.org_id = org_id;
        input.name = name;
        input.url = url;
        input.language = language;
        input.description = description;
        input.token = token;

        DB::OrganizationReposInfo::ptr info;
        auto rt = OrganizationRepoService::Add(viewer, input, info);
        if (rt == OrganizationRepoService::Result::OrgNotFound) {
            result->setErrno(ORG_NOT_FOUND);
            break;
        }
        if (rt == OrganizationRepoService::Result::NotMember) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt == OrganizationRepoService::Result::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt == OrganizationRepoService::Result::NameExists) {
            result->setErrno(ORG_REPO_NAME_EXISTS);
            break;
        }
        if (rt == OrganizationRepoService::Result::InvalidParam) {
            result->setErrno(PARAM_INVALID);
            break;
        }
        if (rt != OrganizationRepoService::Result::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("id", info->getId());
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
