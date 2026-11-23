#include "FeedbackRepository.hpp"

DB::UserFeedbackInfo::ptr FeedbackRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    auto qb = DB::UserFeedbackInfoDao::newQuery();
    if (!qb) {
        return nullptr;
    }
    qb->where("id", "=", id);
    qb->where("is_deleted", "=", (int64_t)0);
    std::vector<DB::UserFeedbackInfo::ptr> results;
    if (DB::UserFeedbackInfoDao::QueryByBuilder(results, qb, conn) || results.empty()) {
        return nullptr;
    }
    return results[0];
}

bool FeedbackRepository::ListByPages(std::vector<DB::UserFeedbackInfo::ptr>& infos,
    const std::string& type,
    int32_t page,
    int32_t page_size,
    int64_t& total,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return false;
    }
    auto qb = DB::UserFeedbackInfoDao::newQuery();
    if (!qb) {
        return false;
    }
    qb->where("is_deleted", "=", (int64_t)0);
    qb->whereIf(!type.empty(), "type", "=", type);
    qb->orderBy("id", "DESC");

    int32_t offset = (page > 0 ? page - 1 : 0) * page_size;
    return DB::UserFeedbackInfoDao::QueryByBuilderPages(infos, total, qb, offset, page_size, conn) == 0;
}

bool FeedbackRepository::Insert(DB::UserFeedbackInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::UserFeedbackInfoDao::Insert(info, conn) == 0;
}

bool FeedbackRepository::Update(DB::UserFeedbackInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::UserFeedbackInfoDao::Update(info, conn) == 0;
}
