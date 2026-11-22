#include "OrganizationApplyCreateServlet.hpp"

#include "../../Event/EventDefine.hpp"
#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"
#include "../../Repository/UserRepository.hpp"

OrganizationApplyCreateServlet::OrganizationApplyCreateServlet()
    : BlogLoginedServlet("OrganizationApplyCreateServlet")
{
}

int32_t OrganizationApplyCreateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, name, "name");
        DEFINE_AND_CHECK_STRING(result, type, "type");
        std::string desc = request->getParamAs<std::string>("desc");

        int64_t user_id = getUserId(request);
        if (!user_id) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        auto uinfo = UserRepository::Get(user_id, GetDB());
        if (!uinfo) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }

        DB::OrganizationApplyInfo::ptr info;
        auto rt = OrganizationService::SubmitApply(user_id, name, type, desc, info);
        if (rt != OrganizationService::Result::Ok) {
            result->setErrno(ORG_APPLY_INSERT_FAILED);
            break;
        }

        result->set("apply_id", info->getId());

        // Notify all ADMIN users about new org application
        {
            EventOrgApplyData data = {};
            data.type = "created";
            data.applicant_id = user_id;
            data.applicant_name = uinfo->getName();
            data.org_name = name;
            data.apply_id = info->getId();

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ORG_APPLY, std::move(data));
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
