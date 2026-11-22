#include "OrganizationAdminListsServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Manager/UserManager.hpp"
#include "../../Util/Util.hpp"
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationRepository.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationAdminListsServlet::OrganizationAdminListsServlet()
    : BlogLoginedServlet("OrganizationAdminListsServlet")
{
}

int32_t OrganizationAdminListsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");
        int32_t status = request->getParamAs<int32_t>("status", -1);

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        int32_t role = current_user->getRole();
        if (role != UserManager::Role::ADMIN) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        uint64_t offset = (page_num - 1) * page_size;
        std::vector<DB::OrganizationInfo::ptr> orgs;
        uint64_t total = OrganizationQueryRepository::ListByPages(orgs, offset, page_size, status, true, GetDB());

        result->set("total", total);
        Json::Value list;
        for (auto& i : orgs) {
            Json::Value item;
            item["id"] = i->getId();
            item["name"] = i->getName();
            item["type"] = i->getType();
            item["status"] = i->getStatus();
            item["description"] = i->getDescription();
            item["create_time"] = i->getCreateTime();
            int32_t sts = OrgMemberStatus::APPROVED;
            item["count"] = OrganizationUserRelRepository::CountByOrg(i->getId(), sts, true, GetDB());
            list.append(item);
        }
        result->set("list", list);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
