#include "BadgeRepository.hpp"

int64_t BadgeRepository::CountAll(chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::BadgeInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    int64_t total = 0;
    if (qb->executeCount(total, conn)) {
        return 0;
    }
    return total;
}

void BadgeRepository::ListValid(std::vector<DB::BadgeInfo::ptr>& badges, chen::IDB::ptr conn)
{
    if (!conn) {
        return;
    }
    auto qb = DB::BadgeInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("sort_order", "ASC");
    qb->orderBy("id", "ASC");
    DB::BadgeInfoDao::QueryByBuilder(badges, qb, conn);
}

bool BadgeRepository::Insert(DB::BadgeInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::BadgeInfoDao::Insert(info, conn) == 0;
}
