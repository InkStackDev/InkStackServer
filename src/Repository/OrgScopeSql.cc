#include "OrgScopeSql.hpp"

#include <vector>

void ApplyOrgScope(const chen::QueryBuilder::ptr& qb,
    const std::string& column,
    const OrgScope& scope,
    int32_t min_role)
{
    if (!qb || scope.isAll()) {
        return;
    }

    std::vector<int64_t> org_ids = scope.orgIds(min_role);
    if (org_ids.empty()) {
        // 组织 id 为正数，-1 不会命中任何记录
        qb->where(column, "=", (int64_t)-1);
        return;
    }
    qb->whereIn(column, org_ids);
}
