#include "ExportRecordManager.hpp"

#include "../Repository/RecordRepository.hpp"
#include "../Util/Util.hpp"

#include <Chen/Util/TimeUtil.hpp>

DB::ExportRecordInfo::ptr ExportRecordManager::get(int64_t id)
{
    return ExportRecordRepository::Get(id, GetDB());
}

bool ExportRecordManager::list(std::vector<DB::ExportRecordInfo::ptr>& results,
    int64_t& total,
    const std::string& search,
    const std::string& type,
    const std::string& status,
    int32_t offset,
    int32_t limit)
{
    return ExportRecordRepository::ListByPages(results, total, search, type, status, offset, limit, GetDB());
}

DB::ExportRecordInfo::ptr ExportRecordManager::create(int64_t uid, const std::string& type, const std::string& name)
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
        return nullptr;
    }
    return info;
}

bool ExportRecordManager::remove(int64_t id)
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
