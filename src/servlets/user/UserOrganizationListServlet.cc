#include "UserOrganizationListServlet.hpp"

#include <Chen/Log/Log.hpp>

#include <algorithm>

#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationRepository.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"
#include "../../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

UserOrganizationListServlet::UserOrganizationListServlet()
    : BlogLoginedServlet("UserOrganizationListServlet")
{
}

int32_t UserOrganizationListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);

        std::vector<DB::OrganizationUserRelInfo::ptr> orgs;
        OrganizationUserRelRepository::ListByUser(orgs, uid, -1, true, GetDB());

        std::sort(orgs.begin(), orgs.end(), [](const auto& a, const auto& b) {
            return a->getCreateTime() < b->getCreateTime();
        });

        Json::Value list;
        for (auto& i : orgs) {
            Json::Value item;
            auto org = OrganizationRepository::Get(i->getOrgId(), GetDB());
            item["id"] = i->getId();
            item["org_id"] = i->getOrgId();
            item["org_name"] = org->getName();
            item["org_description"] = org->getDescription();
            item["type"] = org->getType();
            item["role"] = i->getRole();
            item["join_time"] = i->getCreateTime();
            // 成员数
            int32_t status = OrgMemberStatus::APPROVED;
            item["count"] = OrganizationUserRelRepository::CountByOrg(i->getOrgId(), status, true, GetDB());
            list.append(item);
        }
        result->set("list", list);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
