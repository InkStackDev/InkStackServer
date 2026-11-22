#include "OrganizationApplyListServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Util/Util.hpp"
#include "../../Repository/OrganizationApplyRepository.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Domain/UserTypes.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationApplyListServlet::OrganizationApplyListServlet()
    : BlogLoginedServlet("OrganizationApplyListServlet")
{
}

int32_t OrganizationApplyListServlet::handle(
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

        auto uinfo = UserRepository::Get(uid, GetDB());
        if (!uinfo || uinfo->getRole() != UserRole::ADMIN) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        uint64_t offset = (page_num - 1) * page_size;
        std::vector<DB::OrganizationApplyInfo::ptr> applies;
        int64_t total = OrganizationApplyRepository::ListByPages(applies, offset, page_size, status, true, GetDB());

        result->set("total", total);
        Json::Value list;
        for (auto& i : applies) {
            Json::Value item;
            item["id"] = i->getId();
            item["user_id"] = i->getUserId();

            // Lookup user name
            auto apply_user = UserRepository::Get(i->getUserId(), GetDB());
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
