#include "RdStatsQueryRepository.hpp"

#include "OrgScopeSql.hpp"

#include "../Domain/OrganizationTypes.hpp"
#include "../Domain/RdTypes.hpp"

#include <Chen/DB/QueryBuilder.hpp>

const char* TableName(RdStatsQueryRepository::Entity entity)
{
    switch (entity) {
    case RdStatsQueryRepository::Entity::Requirement:
        return "requirement";
    case RdStatsQueryRepository::Entity::Bug:
        return "bug";
    case RdStatsQueryRepository::Entity::Task:
        return "task";
    }
    return "requirement";
}

static chen::QueryBuilder::ptr CreateBase(const OrgScope& scope,
    int64_t org_id,
    RdStatsQueryRepository::Entity entity,
    time_t start,
    time_t end)
{
    auto qb = chen::QueryBuilder::Create(TableName(entity));
    if (!qb) {
        return nullptr;
    }
    qb->where("is_deleted", "=", (int64_t)0);
    ApplyOrgScope(qb, "org_id", scope, OrgRole::REPORTER);
    qb->whereIf(org_id > 0, "org_id", "=", org_id);
    if (start > 0) {
        qb->whereSQL("create_time >= FROM_UNIXTIME(?)", (int64_t)start);
    }
    if (end > 0) {
        qb->whereSQL("create_time < FROM_UNIXTIME(?)", (int64_t)end);
    }
    return qb;
}

static void AddMinStatus(const chen::QueryBuilder::ptr& qb, int32_t min_status)
{
    if (min_status >= 0) {
        qb->where("status", ">=", (int64_t)min_status);
    }
}

static void AddOpenCond(const chen::QueryBuilder::ptr& qb, RdStatsQueryRepository::Entity entity)
{
    switch (entity) {
    case RdStatsQueryRepository::Entity::Requirement:
        qb->whereNotIn("status", std::vector<int64_t>{RequirementStatus::DONE, RequirementStatus::CLOSED});
        break;
    case RdStatsQueryRepository::Entity::Bug:
        qb->whereNotIn("status", std::vector<int64_t>{BugStatus::FIXED, BugStatus::CLOSED});
        break;
    case RdStatsQueryRepository::Entity::Task:
        qb->whereNotIn("status", std::vector<int64_t>{TaskStatus::DONE, TaskStatus::CLOSED});
        break;
    }
}

bool RdStatsQueryRepository::CountCreated(const OrgScope& scope,
    int64_t org_id,
    Entity entity,
    time_t start,
    time_t end,
    int64_t& out,
    chen::IDB::ptr conn)
{
    out = 0;
    if (!conn) {
        return false;
    }
    auto qb = CreateBase(scope, org_id, entity, start, end);
    if (!qb) {
        return false;
    }
    return qb->executeCount(out, conn) == 0;
}

bool RdStatsQueryRepository::CountByMinStatus(const OrgScope& scope,
    int64_t org_id,
    Entity entity,
    int32_t min_status,
    time_t start,
    time_t end,
    int64_t& out,
    chen::IDB::ptr conn)
{
    out = 0;
    if (!conn) {
        return false;
    }
    auto qb = CreateBase(scope, org_id, entity, start, end);
    if (!qb) {
        return false;
    }
    AddMinStatus(qb, min_status);
    return qb->executeCount(out, conn) == 0;
}

bool RdStatsQueryRepository::AvgCycleDays(const OrgScope& scope,
    int64_t org_id,
    Entity entity,
    int32_t min_status,
    time_t start,
    time_t end,
    double& out,
    chen::IDB::ptr conn)
{
    out = 0.0;
    if (!conn) {
        return false;
    }
    auto qb = CreateBase(scope, org_id, entity, start, end);
    if (!qb) {
        return false;
    }
    AddMinStatus(qb, min_status);
    qb->select("AVG(TIMESTAMPDIFF(SECOND, create_time, update_time)) / 86400.0");
    return qb->queryScalarDouble(out, conn) == 0;
}

bool RdStatsQueryRepository::ListCreatedByDay(const OrgScope& scope,
    int64_t org_id,
    Entity entity,
    int32_t min_status,
    time_t start,
    time_t end,
    std::vector<std::pair<std::string, int64_t>>& out,
    chen::IDB::ptr conn)
{
    out.clear();
    if (!conn) {
        return false;
    }
    auto qb = CreateBase(scope, org_id, entity, start, end);
    if (!qb) {
        return false;
    }
    AddMinStatus(qb, min_status);
    qb->select("DATE_FORMAT(create_time, '%Y-%m-%d') as d, COUNT(*) as cnt");
    qb->groupBy("d");
    qb->orderBy("d", "ASC");
    return qb->queryPairs<std::string, int64_t>(out, conn) == 0;
}

bool RdStatsQueryRepository::CountCreatedBefore(const OrgScope& scope,
    int64_t org_id,
    Entity entity,
    int64_t before,
    int64_t& out,
    chen::IDB::ptr conn)
{
    out = 0;
    if (!conn) {
        return false;
    }
    auto qb = CreateBase(scope, org_id, entity, 0, 0);
    if (!qb) {
        return false;
    }
    qb->whereSQL("create_time < FROM_UNIXTIME(?)", before);
    return qb->executeCount(out, conn) == 0;
}

bool RdStatsQueryRepository::GetStatusStats(const OrgScope& scope,
    int64_t org_id,
    Entity entity,
    int64_t now,
    StatusStats& out,
    chen::IDB::ptr conn)
{
    out = StatusStats{};
    if (!conn) {
        return false;
    }

    {
        auto qb = CreateBase(scope, org_id, entity, 0, 0);
        if (!qb || qb->executeCount(out.total, conn) != 0) {
            return false;
        }
    }

    {
        auto qb = CreateBase(scope, org_id, entity, 0, 0);
        if (!qb) {
            return false;
        }
        AddOpenCond(qb, entity);
        if (qb->executeCount(out.open, conn) != 0) {
            return false;
        }
    }

    if (entity == Entity::Task) {
        auto qb = CreateBase(scope, org_id, entity, 0, 0);
        if (!qb) {
            return false;
        }
        AddOpenCond(qb, entity);
        qb->where("deadline", ">", (int64_t)0);
        qb->whereSQL("deadline < FROM_UNIXTIME(?)", now);
        if (qb->executeCount(out.overdue, conn) != 0) {
            return false;
        }
    }

    return true;
}
