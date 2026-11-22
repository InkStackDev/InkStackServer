#include "RdOrganizationsServlet.hpp"

#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationRepository.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"
#include "../../Util/Util.hpp"

RdOrganizationsServlet::RdOrganizationsServlet()
    : BlogLoginedServlet("RdOrganizationsServlet")
{
}

int32_t RdOrganizationsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);

        std::vector<DB::OrganizationUserRelInfo::ptr> user_orgs;
        OrganizationUserRelRepository::ListByUser(user_orgs, uid, -1, true, GetDB());

        Json::Value arr(Json::arrayValue);
        for (auto& rel : user_orgs) {
            if (rel->getRole() < OrgRole::REPORTER) {
                continue;
            }

            auto org = OrganizationRepository::Get(rel->getOrgId(), GetDB());
            if (!org) {
                continue;
            }

            Json::Value item;
            item["org_id"] = org->getId();
            item["org_name"] = org->getName();
            item["role"] = rel->getRole();
            arr.append(item);
        }

        result->set("list", arr);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
