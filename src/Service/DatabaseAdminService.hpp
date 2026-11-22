/**
 * @file DatabaseAdminService.hpp
 * @brief 数据库管理端统计与清理用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>

class DatabaseAdminService {
public:
    struct Stats {
        int64_t users = 0;
        int64_t articles = 0;
        int64_t comments = 0;
        int64_t categories = 0;
        int64_t labels = 0;
        int64_t backups = 0;
    };

    static bool GetStats(Stats& out);

    static bool Cleanup(int64_t deadline, int64_t& backup_count, int64_t& export_count, int64_t& freed_bytes);
};
