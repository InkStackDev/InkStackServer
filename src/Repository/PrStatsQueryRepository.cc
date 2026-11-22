#include "PrStatsQueryRepository.hpp"

#include <functional>

#include <Chen/DB/QueryBuilder.hpp>

#include "DB/OrganizationRepoPrsInfo.hpp"
namespace {

void AddBaseConds(chen::QueryBuilder::ptr qb, const std::vector<int64_t>& org_ids, time_t start, time_t end)
{
    qb->join("INNER", "organization_repos repos", "prs.repo_id = repos.id");
    if (!org_ids.empty()) {
        qb->whereIn("repos.org_id", org_ids);
    }
    qb->whereSQL("prs.create_time >= FROM_UNIXTIME(?)", (int64_t)start);
    qb->whereSQL("prs.create_time < FROM_UNIXTIME(?)", (int64_t)end);
}

int64_t CountWithBase(const std::vector<int64_t>& org_ids,
    time_t start,
    time_t end,
    chen::IDB::ptr conn,
    const std::function<void(chen::QueryBuilder::ptr)>& extra)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::OrganizationRepoPrsInfoDao::newQuery("prs");
    if (!qb) {
        return 0;
    }
    AddBaseConds(qb, org_ids, start, end);
    if (extra) {
        extra(qb);
    }
    int64_t total = 0;
    if (qb->executeCount(total, conn)) {
        return 0;
    }
    return total;
}

} // namespace

bool PrStatsQueryRepository::CountByDay(std::vector<std::pair<std::string, int64_t>>& rows,
    const std::vector<int64_t>& org_ids,
    time_t start,
    time_t end,
    bool merged_only,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return false;
    }
    auto qb = DB::OrganizationRepoPrsInfoDao::newQuery("prs");
    if (!qb) {
        return false;
    }
    qb->select("DATE_FORMAT(prs.create_time, '%Y-%m-%d') as d, COUNT(*) as cnt");
    AddBaseConds(qb, org_ids, start, end);
    if (merged_only) {
        // 未合入时 merged_at 可能被写成 0（1970-01-01），这里排除零值
        qb->whereSQL("(prs.state = 'merged' OR prs.merged_at > ?)", std::string("1970-01-02 00:00:00"));
    }
    qb->groupBy("d");
    qb->orderBy("d", "ASC");
    return qb->queryPairs<std::string, int64_t>(rows, conn) == 0;
}

int64_t PrStatsQueryRepository::CountReviewed(const std::vector<int64_t>& org_ids,
    time_t start,
    time_t end,
    chen::IDB::ptr conn)
{
    return CountWithBase(org_ids, start, end, conn, [](chen::QueryBuilder::ptr qb) {
        qb->where("prs.review_status", "!=", std::string(""));
        qb->where("prs.review_status", "!=", std::string("pending"));
    });
}

int64_t PrStatsQueryRepository::CountApproved(const std::vector<int64_t>& org_ids,
    time_t start,
    time_t end,
    chen::IDB::ptr conn)
{
    return CountWithBase(org_ids, start, end, conn, [](chen::QueryBuilder::ptr qb) {
        qb->where("prs.review_status", "!=", std::string(""));
        qb->where("prs.review_status", "!=", std::string("pending"));
        qb->where("prs.review_status", "=", std::string("approved"));
    });
}

int64_t PrStatsQueryRepository::CountCreated(const std::vector<int64_t>& org_ids,
    time_t start,
    time_t end,
    chen::IDB::ptr conn)
{
    return CountWithBase(org_ids, start, end, conn, nullptr);
}
