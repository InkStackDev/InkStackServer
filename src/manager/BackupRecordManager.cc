#include "BackupRecordManager.hpp"

#include "../Repository/RecordRepository.hpp"
#include "../Util/Util.hpp"

#include <Chen/Util/TimeUtil.hpp>

DB::BackupRecordInfo::ptr BackupRecordManager::get(int64_t id)
{
    return BackupRecordRepository::Get(id, GetDB());
}

bool BackupRecordManager::list(std::vector<DB::BackupRecordInfo::ptr>& results,
    int64_t& total,
    const std::string& search,
    const std::string& type,
    const std::string& status,
    int32_t offset,
    int32_t limit)
{
    return BackupRecordRepository::ListByPages(results, total, search, type, status, offset, limit, GetDB());
}

DB::BackupRecordInfo::ptr BackupRecordManager::create(int64_t uid,
    const std::string& type,
    const std::string& name,
    const std::string& description)
{
    auto db = GetDB();
    if (!db) {
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
    info->setCreateTime(chen::TimeUtil::GetCurrentSec());
    if (!BackupRecordRepository::Insert(info, db)) {
        return nullptr;
    }
    return info;
}

bool BackupRecordManager::remove(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto info = BackupRecordRepository::Get(id, db);
    if (!info) {
        return false;
    }
    info->setIsDeleted(1);
    return BackupRecordRepository::Update(info, db);
}
