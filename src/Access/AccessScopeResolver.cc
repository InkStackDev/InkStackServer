#include "AccessScopeResolver.hpp"

#include "../Domain/OrganizationTypes.hpp"
#include "../Repository/OrganizationRepository.hpp"

#include <Chen/Log/Log.hpp>

#include <utility>
#include <vector>

static chen::Logger::ptr logger = LOG_ROOT();

bool AccessScopeResolver::ResolveOrgs(OrgScope& scope, const Viewer& viewer, chen::IDB::ptr conn)
{
    scope = OrgScope::ForGrants({});

    if (viewer.is_platform_admin) {
        scope = OrgScope::All();
        return true;
    }

    if (!viewer.valid() || !conn) {
        return false;
    }

    std::vector<std::pair<int64_t, int32_t>> rows;
    if (!OrganizationQueryRepository::ListUserOrgGrants(rows, viewer.user_id, conn)) {
        ERROR(logger) << "resolve org scope failed, user_id=" << viewer.user_id;
        return false;
    }

    std::vector<OrgGrant> grants;
    grants.reserve(rows.size());
    for (const auto& [org_id, role] : rows) {
        grants.push_back({org_id, role});
    }
    scope = OrgScope::ForGrants(std::move(grants));
    return true;
}
