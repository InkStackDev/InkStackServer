#include "BackupRecordManager.hpp"

#include "../Util/Util.hpp"

#include <Chen/Log/Log.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

DB::BackupRecordInfo::ptr BackupRecordManager::ParseRow(chen::ISQLData::ptr rt)
{
    return DB::BackupRecordInfoDao::ParseRow(rt);
}

DB::BackupRecordInfo::ptr BackupRecordManager::get(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    return DB::BackupRecordInfoDao::Query(id, db);
}

bool BackupRecordManager::list(std::vector<DB::BackupRecordInfo::ptr>& results,
    int64_t& total,
    const std::string& search,
    const std::string& type,
    const std::string& status,
    int32_t offset,
    int32_t limit)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto qb = DB::BackupRecordInfoDao::newQuery();
    qb->where("is_deleted", "=", (int64_t)0);
    if (!search.empty()) {
        qb->where("name", "LIKE", "%" + search + "%");
    }
    if (!type.empty()) {
        qb->where("type", "=", type);
    }
    if (!status.empty()) {
        qb->where("status", "=", status);
    }
    qb->orderBy("id", "DESC");
    if (DB::BackupRecordInfoDao::QueryByBuilderPages(results, total, qb, offset, limit, db)) {
        ERROR(logger) << "backup_record QueryByBuilderPages failed";
        return false;
    }
    return true;
}

DB::BackupRecordInfo::ptr BackupRecordManager::create(int64_t uid,
    const std::string& type,
    const std::string& name,
    const std::string& description)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = std::make_shared<DB::BackupRecordInfo>();
    info->setCreatedBy(uid);
    info->setType(type);
    info->setName(name);
    info->setDescription(description);
    info->setStatus("processing");
    info->setIsDeleted(0);
    info->setSize(0);
    info->setFileCount(0);
    info->setCreateTime(time(0));
    if (DB::BackupRecordInfoDao::Insert(info, db)) {
        ERROR(logger) << "Insert backup_record failed";
        return nullptr;
    }
    return info;
}

bool BackupRecordManager::remove(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto info = get(id);
    if (!info) {
        return false;
    }
    info->setIsDeleted(1);
    if (DB::BackupRecordInfoDao::Update(info, db)) {
        ERROR(logger) << "Delete backup_record failed";
        return false;
    }
    return true;
}
