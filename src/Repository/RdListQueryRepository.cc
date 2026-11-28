#include "RdListQueryRepository.hpp"

#include "OrgScopeSql.hpp"

#include "../Domain/OrganizationTypes.hpp"

#include <Chen/DB/QueryBuilder.hpp>

#include <string>
#include <vector>

const char* TaskColumns()
{
    return "t.id, t.org_id, t.title, t.description, t.priority, t.status, t.creator_id, t.assignee_id, "
           "t.requirement_id, t.bug_id, t.deadline, t.estimated_hours, t.is_deleted, t.create_time, t.update_time";
}

const char* RequirementColumns()
{
    return "r.id, r.org_id, r.title, r.description, r.priority, r.status, r.creator_id, r.assignee_id, "
           "r.iteration, r.category, r.source, r.deadline, r.is_deleted, r.create_time, r.update_time";
}

const char* BugColumns()
{
    return "b.id, b.org_id, b.title, b.description, b.severity, b.priority, b.status, b.creator_id, "
           "b.assignee_id, b.requirement_id, b.module, b.steps_to_reproduce, b.environment, b.is_deleted, "
           "b.create_time, b.update_time";
}

static std::string JoinInt64(const std::vector<int64_t>& values)
{
    std::string out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            out.push_back(',');
        }
        out += std::to_string(values[i]);
    }
    return out;
}

static void ApplyTaskVisibility(const chen::QueryBuilder::ptr& qb, const Viewer& viewer, const OrgScope& scope)
{
    if (scope.isAll()) {
        return;
    }
    if (!viewer.valid()) {
        qb->where("t.org_id", "=", (int64_t)-1);
        return;
    }

    std::vector<int64_t> all_orgs = scope.orgIds(OrgRole::MEMBER);
    if (all_orgs.empty()) {
        qb->where("t.org_id", "=", (int64_t)-1);
        return;
    }
    qb->whereIn("t.org_id", all_orgs);

    // 普通成员只能看到自己创建/负责的任务；报告者及以上可见组织内全部任务。
    std::vector<int64_t> reporter_orgs = scope.orgIds(OrgRole::REPORTER);
    std::string cond = "(";
    if (!reporter_orgs.empty()) {
        cond += "t.org_id IN (";
        cond += JoinInt64(reporter_orgs);
        cond += ") OR ";
    }
    cond += "? IN (t.creator_id, t.assignee_id))";
    qb->whereSQL(cond, (int64_t)viewer.user_id);
}

static void ApplyCommonFilters(const chen::QueryBuilder::ptr& qb,
    const std::string& alias,
    const RdListQueryRepository::Filter& filter)
{
    qb->whereIf(filter.status >= 0, alias + ".status", "=", (int64_t)filter.status);
    qb->whereIf(filter.priority >= 0, alias + ".priority", "=", (int64_t)filter.priority);
}

bool RdListQueryRepository::ListTasks(std::vector<DB::TaskInfo::ptr>& out,
    int64_t& total,
    const Viewer& viewer,
    const OrgScope& scope,
    int64_t org_id,
    const Filter& filter,
    chen::IDB::ptr conn)
{
    out.clear();
    total = 0;
    if (!conn) {
        return false;
    }
    auto qb = DB::TaskInfoDao::newQuery("t");
    if (!qb) {
        return false;
    }
    qb->select(TaskColumns());
    qb->where("t.is_deleted", "=", (int64_t)0);
    qb->whereIf(org_id > 0, "t.org_id", "=", org_id);
    ApplyTaskVisibility(qb, viewer, scope);
    ApplyCommonFilters(qb, "t", filter);
    qb->whereIf(filter.assignee_id > 0, "t.assignee_id", "=", filter.assignee_id);
    if (!filter.search.empty()) {
        qb->whereSQL("LOCATE(?, t.title) > 0", filter.search);
    }
    qb->orderBy("t.id", "DESC");
    return DB::TaskInfoDao::QueryByBuilderPages(out, total, qb, (int32_t)filter.offset, (int32_t)filter.limit, conn)
        == 0;
}

bool RdListQueryRepository::ListRequirements(std::vector<DB::RequirementInfo::ptr>& out,
    int64_t& total,
    const OrgScope& scope,
    int64_t org_id,
    const Filter& filter,
    chen::IDB::ptr conn)
{
    out.clear();
    total = 0;
    if (!conn) {
        return false;
    }
    auto qb = DB::RequirementInfoDao::newQuery("r");
    if (!qb) {
        return false;
    }
    qb->select(RequirementColumns());
    qb->where("r.is_deleted", "=", (int64_t)0);
    qb->whereIf(org_id > 0, "r.org_id", "=", org_id);
    ApplyOrgScope(qb, "r.org_id", scope, OrgRole::REPORTER);
    ApplyCommonFilters(qb, "r", filter);
    if (!filter.search.empty()) {
        qb->join("LEFT", "user u", "u.id = r.creator_id");
        qb->whereSQL("LOCATE(?, CONCAT_WS(' ', r.title, r.description, u.name)) > 0", filter.search);
    }
    qb->orderBy("r.id", "DESC");
    return DB::RequirementInfoDao::QueryByBuilderPages(
               out, total, qb, (int32_t)filter.offset, (int32_t)filter.limit, conn)
        == 0;
}

bool RdListQueryRepository::ListBugs(std::vector<DB::BugInfo::ptr>& out,
    int64_t& total,
    const OrgScope& scope,
    int64_t org_id,
    const Filter& filter,
    chen::IDB::ptr conn)
{
    out.clear();
    total = 0;
    if (!conn) {
        return false;
    }
    auto qb = DB::BugInfoDao::newQuery("b");
    if (!qb) {
        return false;
    }
    qb->select(BugColumns());
    qb->where("b.is_deleted", "=", (int64_t)0);
    qb->whereIf(org_id > 0, "b.org_id", "=", org_id);
    ApplyOrgScope(qb, "b.org_id", scope, OrgRole::REPORTER);
    ApplyCommonFilters(qb, "b", filter);
    qb->whereIf(filter.severity >= 0, "b.severity", "=", (int64_t)filter.severity);
    if (!filter.search.empty()) {
        qb->whereSQL("LOCATE(?, CONCAT_WS(' ', b.title, b.description)) > 0", filter.search);
    }
    qb->orderBy("b.id", "DESC");
    return DB::BugInfoDao::QueryByBuilderPages(out, total, qb, (int32_t)filter.offset, (int32_t)filter.limit, conn)
        == 0;
}
