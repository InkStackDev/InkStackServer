#include "RdListService.hpp"

#include "../Access/AccessScopeResolver.hpp"
#include "../Util/Util.hpp"

#include <Chen/Log/Log.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

namespace {

bool ResolveScope(const Viewer& viewer, OrgScope& scope, chen::IDB::ptr& db)
{
    if (!viewer.valid()) {
        return false;
    }
    db = GetDB();
    if (!db) {
        return false;
    }
    if (!AccessScopeResolver::ResolveOrgs(scope, viewer, db)) {
        return false;
    }
    return true;
}

} // namespace

bool RdListService::ListTasks(const Viewer& viewer,
    int64_t org_id,
    const RdListQueryRepository::Filter& filter,
    std::vector<DB::TaskInfo::ptr>& out,
    int64_t& total)
{
    OrgScope scope;
    chen::IDB::ptr db;
    if (!ResolveScope(viewer, scope, db)) {
        return false;
    }
    if (!RdListQueryRepository::ListTasks(out, total, viewer, scope, org_id, filter, db)) {
        ERROR(logger) << "list tasks failed, org_id=" << org_id;
        return false;
    }
    return true;
}

bool RdListService::ListRequirements(const Viewer& viewer,
    int64_t org_id,
    const RdListQueryRepository::Filter& filter,
    std::vector<DB::RequirementInfo::ptr>& out,
    int64_t& total)
{
    OrgScope scope;
    chen::IDB::ptr db;
    if (!ResolveScope(viewer, scope, db)) {
        return false;
    }
    if (!RdListQueryRepository::ListRequirements(out, total, scope, org_id, filter, db)) {
        ERROR(logger) << "list requirements failed, org_id=" << org_id;
        return false;
    }
    return true;
}

bool RdListService::ListBugs(const Viewer& viewer,
    int64_t org_id,
    const RdListQueryRepository::Filter& filter,
    std::vector<DB::BugInfo::ptr>& out,
    int64_t& total)
{
    OrgScope scope;
    chen::IDB::ptr db;
    if (!ResolveScope(viewer, scope, db)) {
        return false;
    }
    if (!RdListQueryRepository::ListBugs(out, total, scope, org_id, filter, db)) {
        ERROR(logger) << "list bugs failed, org_id=" << org_id;
        return false;
    }
    return true;
}
