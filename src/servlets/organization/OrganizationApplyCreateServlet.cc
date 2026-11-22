#include "OrganizationApplyCreateServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Event/EventDefine.hpp"
#include "../../Manager/OrganizationApplyManager.hpp"
#include "../../Manager/UserManager.hpp"
#include "../../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

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

        auto uinfo = UserMgr::GetInstance()->get(user_id);
        if (!uinfo) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }

        auto info = std::make_shared<DB::OrganizationApplyInfo>();
        info->setUserId(user_id);
        info->setOrgName(name);
        info->setOrgType(type);
        info->setOrgDescription(desc);
        info->setStatus(OrganizationApplyManager::Status::PENDING);
        info->setCreatedAt(time(0));
        info->setIsDeleted(0);

        auto db = GetDB();
        if (!db) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        if (DB::OrganizationApplyInfoDao::Insert(info, db)) {
            result->setErrno(ORG_APPLY_INSERT_FAILED);
            ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }

        result->set("apply_id", info->getId());

        // Notify all ADMIN users about new org application
        {
            auto applicant = UserMgr::GetInstance()->get(user_id);
            EventOrgApplyData data = {};
            data.type = "created";
            data.applicant_id = user_id;
            data.applicant_name = applicant ? applicant->getName() : std::to_string(user_id);
            data.org_name = name;
            data.apply_id = info->getId();

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ORG_APPLY, std::move(data));
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
