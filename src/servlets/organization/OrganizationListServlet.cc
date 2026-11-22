#include "OrganizationListServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationRepository.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"
#include "../../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationListServlet::OrganizationListServlet()
    : BlogLoginedServlet("OrganizationListServlet")
{
}

int32_t OrganizationListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int32_t status = request->getParamAs<int32_t>("status", -1);

        std::vector<DB::OrganizationInfo::ptr> orgs;
        int64_t total = OrganizationQueryRepository::ListByPages(orgs, 0, INT32_MAX, status, true, GetDB());

        result->set("total", total);
        Json::Value list;
        for (auto& i : orgs) {
            Json::Value item;
            item["id"] = i->getId();
            item["name"] = i->getName();
            item["status"] = i->getStatus();
            item["type"] = i->getType();
            item["description"] = i->getDescription();
            // 获取成员数量
            int32_t status = OrgMemberStatus::APPROVED;
            item["count"] = OrganizationUserRelRepository::CountByOrg(i->getId(), status, true, GetDB());
            list.append(item);
        }
        result->set("list", list);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
