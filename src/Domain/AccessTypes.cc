#include "AccessTypes.hpp"

#include <utility>

bool Viewer::valid() const
{
    return user_id > 0;
}

OrgScope OrgScope::ForGrants(std::vector<OrgGrant> grants)
{
    OrgScope scope;
    scope.grants_ = std::move(grants);
    return scope;
}

bool OrgScope::isAll() const
{
    return all_;
}

bool OrgScope::empty() const
{
    return !all_ && grants_.empty();
}

bool OrgScope::allows(int64_t org_id, int32_t min_role) const
{
    if (all_) {
        return true;
    }
    for (const auto& grant : grants_) {
        if (grant.org_id == org_id) {
            return grant.role >= min_role;
        }
    }
    return false;
}

std::vector<int64_t> OrgScope::orgIds(int32_t min_role) const
{
    std::vector<int64_t> ids;
    if (all_) {
        return ids;
    }
    ids.reserve(grants_.size());
    for (const auto& grant : grants_) {
        if (grant.role >= min_role) {
            ids.push_back(grant.org_id);
        }
    }
    return ids;
}

OrgScope OrgScope::All()
{
    OrgScope scope;
    scope.all_ = true;
    return scope;
}
