#include "OrganizationUserListServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Manager/UserManager.hpp"
#include "../../Util/Util.hpp"
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationUserListServlet::OrganizationUserListServlet()
    : BlogLoginedServlet("OrganizationUserListServlet")
{
}

int32_t OrganizationUserListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");
        DEFINE_AND_CHECK_TYPE(result, int32_t, status, "status");

        if (status == OrgMemberStatus::PENDING) {
            int64_t uid = getUserId(request);
            auto info = OrganizationUserRelRepository::GetByOrgAndUser(id, uid, GetDB());
            if (!info) {
                result->setErrno(ORG_NOT_MEMBER);
                break;
            }
            auto current_user = UserMgr::GetInstance()->get(uid);
            if (!current_user) {
                result->setErrno(USER_NOT_FOUND);
                break;
            }
            if (info->getRole() != OrgRole::ORG_ADMIN && info->getRole() != OrgRole::DEV_LEAD
                && current_user->getRole() != UserManager::Role::ADMIN) {
                result->setErrno(ACCESS_DENIED);
                break;
            }
        }

        uint64_t offset = (page_num - 1) * page_size;

        std::vector<DB::OrganizationUserRelInfo::ptr> rels;
        int64_t total =
            OrganizationUserRelRepository::ListByOrgPages(rels, id, offset, page_size, status, true, GetDB());

        std::sort(rels.begin(), rels.end(), [](const auto& a, const auto& b) {
            return a->getCreateTime() < b->getCreateTime();
        });

        result->set("total", total);
        Json::Value list;
        for (auto& i : rels) {
            auto user = UserMgr::GetInstance()->get(i->getUserId());
            Json::Value item;
            item["id"] = i->getId();
            item["user_id"] = i->getUserId();
            item["name"] = user->getName();
            item["avatar"] = user->getAvatar();
            item["email"] = user->getEmail();
            item["role"] = i->getRole();
            item["status"] = i->getStatus();
            item["join_time"] = i->getCreateTime();
            list.append(item);
        }
        result->set("list", list);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
