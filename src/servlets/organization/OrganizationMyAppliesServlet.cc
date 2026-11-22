#include "OrganizationMyAppliesServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Manager/UserManager.hpp"
#include "../../Util/Util.hpp"
#include "../../Repository/OrganizationApplyRepository.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationMyAppliesServlet::OrganizationMyAppliesServlet()
    : BlogLoginedServlet("OrganizationMyAppliesServlet")
{
}

int32_t OrganizationMyAppliesServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        uint64_t offset = (page_num - 1) * page_size;
        std::vector<DB::OrganizationApplyInfo::ptr> applies;
        int64_t total = OrganizationApplyRepository::ListByUser(applies, uid, offset, page_size, true, GetDB());

        result->set("total", total);
        Json::Value list;
        for (auto& i : applies) {
            Json::Value item;
            item["id"] = i->getId();
            item["user_id"] = i->getUserId();

            auto apply_user = UserMgr::GetInstance()->get(i->getUserId());
            item["user_name"] = apply_user ? apply_user->getName() : "";

            item["org_name"] = i->getOrgName();
            item["org_type"] = i->getOrgType();
            item["org_description"] = i->getOrgDescription();
            item["status"] = i->getStatus();
            item["review_reason"] = i->getReviewReason();
            item["created_at"] = i->getCreatedAt();
            item["reviewed_at"] = i->getReviewedAt();
            list.append(item);
        }
        result->set("list", list);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
