#include "PrStatsQueryRepository.hpp"

#include "OrgScopeSql.hpp"

#include "../Domain/OrganizationTypes.hpp"
#include "DB/OrganizationRepoPrsInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>

#include <functional>
#include <string>

static void AddBaseConds(const chen::QueryBuilder::ptr& qb,
    const OrgScope& scope,
    int64_t org_id,
    time_t start,
    time_t end)
{
    qb->join("INNER", "organization_repos repos", "prs.repo_id = repos.id");
    ApplyOrgScope(qb, "repos.org_id", scope, OrgRole::REPORTER);
    qb->whereIf(org_id > 0, "repos.org_id", "=", org_id);
    if (start > 0) {
        qb->whereSQL("prs.create_time >= FROM_UNIXTIME(?)", (int64_t)start);
    }
    if (end > 0) {
        qb->whereSQL("prs.create_time < FROM_UNIXTIME(?)", (int64_t)end);
    }
}

static bool CountWithBase(const OrgScope& scope,
    int64_t org_id,
    time_t start,
    time_t end,
    chen::IDB::ptr conn,
    int64_t& out,
    const std::function<void(chen::QueryBuilder::ptr)>& extra)
{
    out = 0;
    if (!conn) {
        return false;
    }
    auto qb = DB::OrganizationRepoPrsInfoDao::newQuery("prs");
    if (!qb) {
        return false;
    }
    AddBaseConds(qb, scope, org_id, start, end);
    if (extra) {
        extra(qb);
    }
    return qb->executeCount(out, conn) == 0;
}

bool PrStatsQueryRepository::CountByDay(std::vector<std::pair<std::string, int64_t>>& rows,
    const OrgScope& scope,
    int64_t org_id,
    time_t start,
    time_t end,
    bool merged_only,
    chen::IDB::ptr conn)
{
    rows.clear();
    if (!conn) {
        return false;
    }
    auto qb = DB::OrganizationRepoPrsInfoDao::newQuery("prs");
    if (!qb) {
        return false;
    }
    qb->select("DATE_FORMAT(prs.create_time, '%Y-%m-%d') as d, COUNT(*) as cnt");
    AddBaseConds(qb, scope, org_id, start, end);
    if (merged_only) {
        // 未合入时 merged_at 可能被写成 0（1970-01-01），这里排除零值
        qb->whereSQL("(prs.state = 'merged' OR prs.merged_at > ?)", std::string("1970-01-02 00:00:00"));
    }
    qb->groupBy("d");
    qb->orderBy("d", "ASC");
    return qb->queryPairs<std::string, int64_t>(rows, conn) == 0;
}

bool PrStatsQueryRepository::CountReviewed(const OrgScope& scope,
    int64_t org_id,
    time_t start,
    time_t end,
    int64_t& out,
    chen::IDB::ptr conn)
{
    return CountWithBase(scope, org_id, start, end, conn, out, [](chen::QueryBuilder::ptr qb) {
        qb->where("prs.review_status", "!=", std::string(""));
        qb->where("prs.review_status", "!=", std::string("pending"));
    });
}

bool PrStatsQueryRepository::CountApproved(const OrgScope& scope,
    int64_t org_id,
    time_t start,
    time_t end,
    int64_t& out,
    chen::IDB::ptr conn)
{
    return CountWithBase(scope, org_id, start, end, conn, out, [](chen::QueryBuilder::ptr qb) {
        qb->where("prs.review_status", "=", std::string("approved"));
    });
}

bool PrStatsQueryRepository::CountCreated(const OrgScope& scope,
    int64_t org_id,
    time_t start,
    time_t end,
    int64_t& out,
    chen::IDB::ptr conn)
{
    return CountWithBase(scope, org_id, start, end, conn, out, nullptr);
}
