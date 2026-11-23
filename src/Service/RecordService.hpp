/**
 * @file RecordService.hpp
 * @brief 备份/导出记录用例：创建与删除
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/BackupRecordInfo.hpp"
#include "DB/ExportRecordInfo.hpp"

#include <cstdint>
#include <string>
#include <vector>

class BackupRecordService {
public:
    static DB::BackupRecordInfo::ptr Create(int64_t uid,
        const std::string& type,
        const std::string& name,
        const std::string& description);

    static bool Remove(int64_t id);

    static DB::BackupRecordInfo::ptr Get(int64_t id);

    static bool List(std::vector<DB::BackupRecordInfo::ptr>& out,
        int64_t& total,
        const std::string& search,
        const std::string& type,
        const std::string& status,
        int32_t offset,
        int32_t limit);
};

class ExportRecordService {
public:
    static DB::ExportRecordInfo::ptr Create(int64_t uid, const std::string& type, const std::string& name);

    static bool Remove(int64_t id);

    static DB::ExportRecordInfo::ptr Get(int64_t id);

    static bool List(std::vector<DB::ExportRecordInfo::ptr>& out,
        int64_t& total,
        const std::string& search,
        const std::string& type,
        const std::string& status,
        int32_t offset,
        int32_t limit);
};
