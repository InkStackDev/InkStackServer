#include "AccessPolicy.hpp"

#include "../Domain/OrganizationTypes.hpp"

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
