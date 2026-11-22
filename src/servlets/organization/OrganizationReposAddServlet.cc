#include "OrganizationReposAddServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Manager/UserManager.hpp"
#include "../../Service/OrganizationRepoService.hpp"
#include "../../Util/Util.hpp"
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationRepository.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"
#include "../../Repository/OrganizationRepoRepository.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

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

        // 校验 name 长度
        if (name.size() > 128) {
            result->setErrno(PARAM_INVALID, "仓库名称最长128字符");
            break;
        }
        // 校验 url 长度
        if (url.size() > 512) {
            result->setErrno(PARAM_INVALID, "仓库地址最长512字符");
            break;
        }
        // 仅支持 GitHub 仓库
        if (url.find("github.com") == std::string::npos) {
            result->setErrno(PARAM_INVALID, "仅支持 GitHub 仓库");
            break;
        }
        // 校验 language 长度
        if (language.size() > 64) {
            result->setErrno(PARAM_INVALID, "编程语言最长64字符");
            break;
        }

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        // 检查组织是否存在
        auto org = OrganizationRepository::Get(org_id, GetDB());
        if (!org) {
            result->setErrno(ORG_NOT_FOUND);
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
            auto rel = OrganizationUserRelRepository::GetByOrgAndUser(org_id, uid, GetDB());
            if (!rel || rel->getStatus() != OrgMemberStatus::APPROVED) {
                result->setErrno(ACCESS_DENIED, "无权访问该组织");
                break;
            }
            int32_t org_role = rel->getRole();
            if (org_role != OrgRole::ORG_ADMIN && org_role != OrgRole::DEV_LEAD) {
                result->setErrno(ACCESS_DENIED, "仅研发主管及以上角色可添加仓库");
                break;
            }
        }

        // 检查同组织下名称是否重复
        auto existing = OrganizationRepoRepository::GetByOrgAndName(org_id, name, GetDB());
        if (existing) {
            result->setErrno(ORG_REPO_NAME_EXISTS, "该组织下已存在同名仓库");
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
        auto rt = OrganizationRepoService::Add(input, info);
        if (rt == OrganizationRepoService::Result::NameExists) {
            result->setErrno(ORG_REPO_NAME_EXISTS, "该组织下已存在同名仓库");
            break;
        }
        if (rt != OrganizationRepoService::Result::Ok) {
            result->setErrno(DB_OPERATION_FAILED, "添加仓库失败");
            break;
        }

        result->set("id", info->getId());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
