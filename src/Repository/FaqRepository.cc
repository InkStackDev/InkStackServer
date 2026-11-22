#include "FaqRepository.hpp"

DB::HelpFaqsInfo::ptr FaqRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return DB::HelpFaqsInfoDao::Query(id, conn);
}

void FaqRepository::ListValid(std::vector<DB::HelpFaqsInfo::ptr>& infos, chen::IDB::ptr conn)
{
    if (!conn) {
        return;
    }
    auto qb = DB::HelpFaqsInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("sort_order", "ASC");
    qb->orderBy("id", "ASC");
    DB::HelpFaqsInfoDao::QueryByBuilder(infos, qb, conn);
}

bool FaqRepository::Insert(DB::HelpFaqsInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::HelpFaqsInfoDao::Insert(info, conn) == 0;
}

bool FaqRepository::Update(DB::HelpFaqsInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::HelpFaqsInfoDao::Update(info, conn) == 0;
}
