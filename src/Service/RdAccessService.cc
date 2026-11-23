#include "RdAccessService.hpp"

#include "../Access/AccessPolicy.hpp"
#include "../Access/AccessScopeResolver.hpp"
#include "../Domain/UserTypes.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Util/Util.hpp"

RdAccessService::Result RdAccessService::CheckPlatformAccess(const Viewer& viewer, int64_t target_uid, CheckResult& out)
{
    out = CheckResult{};
    if (!viewer.valid()) {
        return Result::AccessDenied;
    }
    if (target_uid != viewer.user_id && !viewer.is_platform_admin) {
        return Result::AccessDenied;
    }

    auto db = GetDB();
    if (!db) {
        return Result::DbError;
    }

    Viewer target = viewer;
    if (target_uid != viewer.user_id) {
        auto target_user = UserRepository::Get(target_uid, db);
        if (!target_user || target_user->getState() != UserState::ACTIVE) {
            out.reason = "user not found or disabled";
            return Result::Ok;
        }
        target.user_id = target_uid;
        target.system_role = target_user->getRole();
        target.is_platform_admin = (target.system_role == UserRole::ADMIN);
    }

    OrgScope scope;
    if (!AccessScopeResolver::ResolveOrgs(scope, target, db)) {
        return Result::DbError;
    }

    out.can_access = AccessPolicy::CanAccessPlatform(target, scope);
    if (!out.can_access) {
        out.reason = "requires system admin or organization role >= reporter";
    }
    return Result::Ok;
}
