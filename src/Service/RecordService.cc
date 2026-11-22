#include "RecordService.hpp"

#include <memory>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Repository/RecordRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

DB::BackupRecordInfo::ptr BackupRecordService::Create(int64_t uid,
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
        ERROR(logger) << "create backup record failed, name=" << name;
        return nullptr;
    }
    return info;
}

bool BackupRecordService::Remove(int64_t id)
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

DB::ExportRecordInfo::ptr ExportRecordService::Create(int64_t uid, const std::string& type, const std::string& name)
{
    auto db = GetDB();
    if (!db) {
        return nullptr;
    }
    auto info = std::make_shared<DB::ExportRecordInfo>();
    info->setCreatedBy(uid);
    info->setType(type);
    info->setName(name);
    info->setStatus("processing");
    info->setIsDeleted(0);
    info->setSize(0);
    info->setRecordCount(0);
    info->setCreateTime(chen::TimeUtil::GetCurrentSec());
    if (!ExportRecordRepository::Insert(info, db)) {
        ERROR(logger) << "create export record failed, name=" << name;
        return nullptr;
    }
    return info;
}

bool ExportRecordService::Remove(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto info = ExportRecordRepository::Get(id, db);
    if (!info) {
        return false;
    }
    info->setIsDeleted(1);
    return ExportRecordRepository::Update(info, db);
}
