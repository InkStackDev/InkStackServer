#include "AccessPolicy.hpp"

#include "../Domain/OrganizationTypes.hpp"

bool AccessPolicy::CanAccessPlatform(const Viewer& viewer, const OrgScope& scope)
{
    if (viewer.is_platform_admin) {
        return true;
    }
    return !scope.orgIds(OrgRole::REPORTER).empty();
}

bool AccessPolicy::CanAdminOrganization(const Viewer& viewer)
{
    return viewer.is_platform_admin;
}

bool AccessPolicy::CanManageOrgMembers(const Viewer& viewer, const OrgScope& scope, int64_t org_id)
{
    if (viewer.is_platform_admin) {
        return true;
    }
    return scope.allows(org_id, OrgRole::DEV_LEAD);
}

bool AccessPolicy::CanManageOrgRepos(const Viewer& viewer, const OrgScope& scope, int64_t org_id)
{
    return viewer.is_platform_admin || scope.allows(org_id, OrgRole::DEV_LEAD);
}

bool AccessPolicy::CanSwitchOrgRole(const Viewer& viewer, const OrgScope& scope, int64_t org_id, int32_t target_role)
{
    if (viewer.is_platform_admin) {
        return true;
    }
    if (scope.allows(org_id, OrgRole::ORG_ADMIN)) {
        return true;
    }
    if (!scope.allows(org_id, OrgRole::DEV_LEAD)) {
        return false;
    }
    return target_role == OrgRole::MEMBER || target_role == OrgRole::REPORTER || target_role == OrgRole::DEVELOPER;
}

bool AccessPolicy::CanViewRequirement(const Viewer& viewer, const OrgScope& scope, int64_t org_id)
{
    return viewer.is_platform_admin || scope.allows(org_id, OrgRole::REPORTER);
}

bool AccessPolicy::CanViewBug(const Viewer& viewer, const OrgScope& scope, int64_t org_id)
{
    return viewer.is_platform_admin || scope.allows(org_id, OrgRole::REPORTER);
}

bool AccessPolicy::CanViewTask(const Viewer& viewer,
    const OrgScope& scope,
    int64_t org_id,
    int64_t creator_id,
    int64_t assignee_id)
{
    if (viewer.is_platform_admin) {
        return true;
    }
    if (!scope.allows(org_id, OrgRole::MEMBER)) {
        return false;
    }
    if (scope.allows(org_id, OrgRole::REPORTER)) {
        return true;
    }
    return viewer.user_id == creator_id || viewer.user_id == assignee_id;
}

bool AccessPolicy::CanCreateRdItem(const Viewer& viewer, const OrgScope& scope, int64_t org_id)
{
    return viewer.is_platform_admin || scope.allows(org_id, OrgRole::REPORTER);
}

bool AccessPolicy::CanEditRdItem(const Viewer& viewer, const OrgScope& scope, int64_t org_id)
{
    return viewer.is_platform_admin || scope.allows(org_id, OrgRole::REPORTER);
}

bool AccessPolicy::CanDeleteRdItem(const Viewer& viewer, const OrgScope& scope, int64_t org_id, int64_t creator_id)
{
    if (viewer.is_platform_admin) {
        return true;
    }
    if (!scope.allows(org_id, OrgRole::MEMBER)) {
        return false;
    }
    if (viewer.user_id == creator_id) {
        return true;
    }
    return scope.allows(org_id, OrgRole::DEV_LEAD);
}
