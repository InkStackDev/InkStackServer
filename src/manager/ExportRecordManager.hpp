#pragma once

#include "DB/ExportRecordInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

class ExportRecordManager {
public:
    DB::ExportRecordInfo::ptr get(int64_t id);

    bool list(std::vector<DB::ExportRecordInfo::ptr>& results,
        int64_t& total,
        const std::string& search,
        const std::string& type,
        const std::string& status,
        int32_t offset,
        int32_t limit);

    DB::ExportRecordInfo::ptr create(int64_t uid, const std::string& type, const std::string& name);

    bool remove(int64_t id);
};

typedef chen::Singleton<ExportRecordManager> ExportRecordMgr;
