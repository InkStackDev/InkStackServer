#include "RdCheckAccessServlet.hpp"

#include "../../Access/AccessPolicy.hpp"
#include "../../Access/AccessScopeResolver.hpp"
#include "../../Domain/UserTypes.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Util/Util.hpp"

RdCheckAccessServlet::RdCheckAccessServlet()
    : BlogLoginedServlet("RdCheckAccessServlet")
{
}

int32_t RdCheckAccessServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(RD_INVALID_USER);
            break;
        }

        int64_t target_uid = viewer.user_id;
        std::string uid_param = request->getParam("user_id");
        if (!uid_param.empty()) {
            try {
                target_uid = chen::TypeUtil::Atoi(uid_param);
            } catch (...) {
                result->setErrno(PARAM_INVALID, "invalid user_id");
                break;
            }
            if (target_uid != viewer.user_id && !viewer.is_platform_admin) {
                result->setErrno(ACCESS_DENIED);
                break;
            }
        }

        auto db = GetDB();
        if (!db) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Viewer target = viewer;
        if (target_uid != viewer.user_id) {
            auto target_user = UserRepository::Get(target_uid, db);
            if (!target_user || target_user->getState() != UserState::ACTIVE) {
                result->setErrno(SUCCESS);
                result->set("can_access", "0");
                result->set("reason", "user not found or disabled");
                break;
            }
            target.user_id = target_uid;
            target.system_role = target_user->getRole();
            target.is_platform_admin = (target.system_role == UserRole::ADMIN);
        }

        OrgScope scope;
        if (!AccessScopeResolver::ResolveOrgs(scope, target, db)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        bool can_access = AccessPolicy::CanAccessPlatform(target, scope);

        result->setErrno(SUCCESS);
        result->set("can_access", can_access ? "1" : "0");
        if (!can_access) {
            result->set("reason", "requires system admin or organization role >= reporter");
        }
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
