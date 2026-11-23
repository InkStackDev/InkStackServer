#include "AssignmentQueryRepository.hpp"

namespace {
constexpr int32_t kStatusDraft = 0;
constexpr int32_t kStatusActive = 1;
constexpr int32_t kStatusInactive = 2;
} // namespace

uint64_t AssignmentQueryRepository::ListByPages(std::vector<DB::AssignmentInfo::ptr>& infos,
    uint64_t offset,
    uint64_t size,
    int32_t status,
    bool is_valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::AssignmentInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (DB::AssignmentInfoDao::QueryByBuilderPages(infos, total, qb, (int32_t)offset, (int32_t)size, conn)) {
        return 0;
    }
    return (uint64_t)total;
}

AssignmentQueryRepository::Stats AssignmentQueryRepository::GetStats(chen::IDB::ptr conn)
{
    Stats stats;
    if (!conn) {
        return stats;
    }

    auto qb = DB::AssignmentInfoDao::newQuery();
    if (!qb) {
        return stats;
    }
    qb->select("status, COUNT(*) AS cnt");
    qb->where("is_deleted", "=", (int64_t)0);
    qb->groupBy("status");

    std::vector<std::pair<int32_t, int64_t>> rows;
    if (qb->queryPairs<int32_t, int64_t>(rows, conn)) {
        return stats;
    }
    for (auto& [status, count] : rows) {
        stats.total += count;
        if (status == kStatusDraft) {
            stats.draft = count;
        } else if (status == kStatusActive) {
            stats.active = count;
        } else if (status == kStatusInactive) {
            stats.closed = count;
        }
    }
    return stats;
}
