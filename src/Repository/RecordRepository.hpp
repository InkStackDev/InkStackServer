/**
 * @file RecordRepository.hpp
 * @brief 备份/导出记录读写
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/BackupRecordInfo.hpp"
#include "DB/ExportRecordInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <string>
#include <vector>

class BackupRecordRepository {
public:
    static DB::BackupRecordInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static bool ListByPages(std::vector<DB::BackupRecordInfo::ptr>& results,
        int64_t& total,
        const std::string& search,
        const std::string& type,
        const std::string& status,
        int32_t offset,
        int32_t limit,
        chen::IDB::ptr conn);

    static bool Insert(DB::BackupRecordInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::BackupRecordInfo::ptr info, chen::IDB::ptr conn);

    static bool DeleteById(int64_t id, chen::IDB::ptr conn);

    /**
     * @brief 读取在 deadline 之前创建且已软删除的记录（物理清理用）
     */
    static void ListDeletedBefore(std::vector<DB::BackupRecordInfo::ptr>& results,
        int64_t deadline,
        chen::IDB::ptr conn);
};

class ExportRecordRepository {
public:
    static DB::ExportRecordInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static bool ListByPages(std::vector<DB::ExportRecordInfo::ptr>& results,
        int64_t& total,
        const std::string& search,
        const std::string& type,
        const std::string& status,
        int32_t offset,
        int32_t limit,
        chen::IDB::ptr conn);

    static bool Insert(DB::ExportRecordInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::ExportRecordInfo::ptr info, chen::IDB::ptr conn);

    static bool DeleteById(int64_t id, chen::IDB::ptr conn);

    static void ListDeletedBefore(std::vector<DB::ExportRecordInfo::ptr>& results,
        int64_t deadline,
        chen::IDB::ptr conn);
};
