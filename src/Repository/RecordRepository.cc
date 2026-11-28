#include "RecordRepository.hpp"

template <typename Dao, typename Info>
static bool ListRecordsByPages(std::vector<typename Info::ptr>& results,
    int64_t& total,
    const std::string& search,
    const std::string& type,
    const std::string& status,
    int32_t offset,
    int32_t limit,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return false;
    }
    auto qb = Dao::newQuery();
    if (!qb) {
        return false;
    }
    qb->where("is_deleted", "=", (int64_t)0);
    if (!search.empty()) {
        qb->where("name", "LIKE", "%" + search + "%");
    }
    qb->whereIf(!type.empty(), "type", "=", type);
    qb->whereIf(!status.empty(), "status", "=", status);
    qb->orderBy("id", "DESC");
    return Dao::QueryByBuilderPages(results, total, qb, offset, limit, conn) == 0;
}

template <typename Dao, typename Info>
static void ListDeletedBeforeImpl(std::vector<typename Info::ptr>& results, int64_t deadline, chen::IDB::ptr conn)
{
    if (!conn) {
        return;
    }
    auto qb = Dao::newQuery();
    if (!qb) {
        return;
    }
    qb->where("is_deleted", "=", (int64_t)1);
    qb->where("create_time", "<", deadline);
    Dao::QueryByBuilder(results, qb, conn);
}

DB::BackupRecordInfo::ptr BackupRecordRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return DB::BackupRecordInfoDao::Query(id, conn);
}

bool BackupRecordRepository::ListByPages(std::vector<DB::BackupRecordInfo::ptr>& results,
    int64_t& total,
    const std::string& search,
    const std::string& type,
    const std::string& status,
    int32_t offset,
    int32_t limit,
    chen::IDB::ptr conn)
{
    return ListRecordsByPages<DB::BackupRecordInfoDao, DB::BackupRecordInfo>(
        results, total, search, type, status, offset, limit, conn);
}

bool BackupRecordRepository::Insert(DB::BackupRecordInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::BackupRecordInfoDao::Insert(info, conn) == 0;
}

bool BackupRecordRepository::Update(DB::BackupRecordInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::BackupRecordInfoDao::Update(info, conn) == 0;
}

bool BackupRecordRepository::DeleteById(int64_t id, chen::IDB::ptr conn)
{
    if (!conn) {
        return false;
    }
    return DB::BackupRecordInfoDao::DeleteById(id, conn) == 0;
}

void BackupRecordRepository::ListDeletedBefore(std::vector<DB::BackupRecordInfo::ptr>& results,
    int64_t deadline,
    chen::IDB::ptr conn)
{
    ListDeletedBeforeImpl<DB::BackupRecordInfoDao, DB::BackupRecordInfo>(results, deadline, conn);
}

DB::ExportRecordInfo::ptr ExportRecordRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return DB::ExportRecordInfoDao::Query(id, conn);
}

bool ExportRecordRepository::ListByPages(std::vector<DB::ExportRecordInfo::ptr>& results,
    int64_t& total,
    const std::string& search,
    const std::string& type,
    const std::string& status,
    int32_t offset,
    int32_t limit,
    chen::IDB::ptr conn)
{
    return ListRecordsByPages<DB::ExportRecordInfoDao, DB::ExportRecordInfo>(
        results, total, search, type, status, offset, limit, conn);
}

bool ExportRecordRepository::Insert(DB::ExportRecordInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::ExportRecordInfoDao::Insert(info, conn) == 0;
}

bool ExportRecordRepository::Update(DB::ExportRecordInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::ExportRecordInfoDao::Update(info, conn) == 0;
}

bool ExportRecordRepository::DeleteById(int64_t id, chen::IDB::ptr conn)
{
    if (!conn) {
        return false;
    }
    return DB::ExportRecordInfoDao::DeleteById(id, conn) == 0;
}

void ExportRecordRepository::ListDeletedBefore(std::vector<DB::ExportRecordInfo::ptr>& results,
    int64_t deadline,
    chen::IDB::ptr conn)
{
    ListDeletedBeforeImpl<DB::ExportRecordInfoDao, DB::ExportRecordInfo>(results, deadline, conn);
}
