#pragma once

#include "DB/BackupRecordInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

class BackupRecordManager {
public:
    DB::BackupRecordInfo::ptr get(int64_t id);

    bool list(std::vector<DB::BackupRecordInfo::ptr>& results,
        int64_t& total,
        const std::string& search,
        const std::string& type,
        const std::string& status,
        int32_t offset,
        int32_t limit);

    DB::BackupRecordInfo::ptr create(int64_t uid,
        const std::string& type,
        const std::string& name,
        const std::string& description);

    bool remove(int64_t id);
};

typedef chen::Singleton<BackupRecordManager> BackupRecordMgr;
