#include "RdRequirementServlet.hpp"

#include "../../Permission.hpp"
#include "../../Manager/RequirementManager.hpp"
#include "../../Manager/UserManager.hpp"
#include "RdHelper.hpp"
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"
#include "../../Util/Util.hpp"

RdRequirementServlet::RdRequirementServlet()
    : BlogLoginedServlet("RdRequirementServlet")
{
}

int32_t RdRequirementServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t id = request->getParamAs<int64_t>("id", 0);
        int64_t org_id = request->getParamAs<int64_t>("org_id", 0);

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        auto user = UserMgr::GetInstance()->get(uid);
        if (!user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        int32_t system_role = user->getRole();
        bool is_platform_admin = (system_role == UserManager::Role::ADMIN);

        if (id) {
            auto info = RequirementMgr::GetInstance()->get(id);
            if (!info || info->getIsDeleted()) {
                result->setErrno(REQUIREMENT_NOT_FOUND);
                break;
            }
            int64_t info_org_id = info->getOrgId();
            if (!is_platform_admin) {
                auto rel = OrganizationUserRelRepository::GetByOrgAndUser(info_org_id, uid, GetDB());
                if (!rel || rel->getStatus() != OrgMemberStatus::APPROVED) {
                    result->setErrno(ACCESS_DENIED);
                    break;
                }
                if (!CanViewRequirement(rel->getRole(), uid, info->getCreatorId())) {
                    result->setErrno(ACCESS_DENIED);
                    break;
                }
            }
            Json::Value data;
            BuildRequirementJson(data, info);
            result->set("data", data);
            break;
        }

        uint64_t page = request->getParamAs<uint64_t>("page", 1);
        uint64_t page_size = request->getParamAs<uint64_t>("page_size", 10);
        uint64_t offset, size;
        ParsePagination(page, page_size, offset, size);

        std::string search = request->getParam("search");
        std::string status_str = request->getParam("status");
        std::string priority_str = request->getParam("priority");

        std::vector<DB::RequirementInfo::ptr> all;
        if (is_platform_admin && org_id) {
            RequirementMgr::GetInstance()->listByOrg(all, org_id, 0, INT32_MAX, -1, true);
        } else if (!is_platform_admin) {
            std::vector<DB::OrganizationUserRelInfo::ptr> user_orgs;
            OrganizationUserRelRepository::ListByUser(user_orgs, uid, OrgMemberStatus::APPROVED, true, GetDB());
            for (auto& rel : user_orgs) {
                if (org_id && rel->getOrgId() != org_id) {
                    continue;
                }
                std::vector<DB::RequirementInfo::ptr> org_reqs;
                RequirementMgr::GetInstance()->listByOrg(org_reqs, rel->getOrgId(), 0, INT32_MAX, -1, true);
                for (auto& req : org_reqs) {
                    if (CanViewRequirement(rel->getRole(), uid, req->getCreatorId())) {
                        all.push_back(req);
                    }
                }
            }
        } else {
            RequirementMgr::GetInstance()->listByPages(all, 0, INT32_MAX, -1, true);
        }

        std::vector<DB::RequirementInfo::ptr> filtered;
        for (auto& info : all) {
            if (!status_str.empty() && StringToRequirementStatus(status_str) != info->getStatus()) {
                continue;
            }
            if (!priority_str.empty() && StringToPriority(priority_str) != info->getPriority()) {
                continue;
            }
            if (!search.empty()) {
                std::string title = info->getTitle();
                std::string desc = info->getDescription();
                std::string creator = GetUserName(info->getCreatorId());
                if (title.find(search) == std::string::npos && desc.find(search) == std::string::npos && creator.find(search) == std::string::npos) {
                    continue;
                }
            }
            filtered.push_back(info);
        }

        uint64_t total = filtered.size();
        Json::Value arr(Json::arrayValue);
        for (uint64_t i = offset; i < filtered.size() && arr.size() < size; ++i) {
            Json::Value item;
            BuildRequirementJson(item, filtered[i]);
            arr.append(item);
        }

        result->set("total", total);
        result->set("list", arr);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
