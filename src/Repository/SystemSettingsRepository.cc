#include "SystemSettingsRepository.hpp"

DB::SystemSettingsInfo::ptr SystemSettingsRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return DB::SystemSettingsInfoDao::Query(id, conn);
}

bool SystemSettingsRepository::Insert(DB::SystemSettingsInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::SystemSettingsInfoDao::Insert(info, conn) == 0;
}

bool SystemSettingsRepository::InsertOrUpdate(DB::SystemSettingsInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::SystemSettingsInfoDao::InsertOrUpdate(info, conn) == 0;
}
