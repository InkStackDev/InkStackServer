#include "OrganizationDetailServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Util/Util.hpp"
#include "../../Repository/OrganizationRepository.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationDetailServlet::OrganizationDetailServlet()
    : BlogLoginedServlet("OrganizationDetailServlet")
{
}

int32_t OrganizationDetailServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");

        auto org = OrganizationRepository::Get(id, GetDB());
        if (!org) {
            result->setErrno(ORG_NOT_FOUND);
            break;
        }

        int64_t uid = getUserId(request);
        auto rel = OrganizationUserRelRepository::GetByOrgAndUser(id, uid, GetDB());
        if (rel) {
            result->set("user_in_org", rel->getStatus());
            result->set("user_role", rel->getRole());
        }

        result->set("id", org->getId());
        result->set("name", org->getName());
        result->set("description", org->getDescription());
        result->set("type", org->getType());
        result->set("status", org->getStatus());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
