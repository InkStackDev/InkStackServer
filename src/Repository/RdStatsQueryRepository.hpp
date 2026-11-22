/**
 * @file RdStatsQueryRepository.hpp
 * @brief 研发（需求/缺陷/任务）统计查询
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "../Domain/AccessTypes.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief 研发实体统计查询
 * @details 所有方法都要求显式传入 OrgScope，空范围返回 0 行，不会退化成全组织。
 */
class RdStatsQueryRepository {
public:
    enum class Entity {
        Requirement,
        Bug,
        Task
    };

    struct StatusStats {
        int64_t total = 0;
        int64_t open = 0;
        int64_t overdue = 0;
    };

    static bool CountCreated(const OrgScope& scope,
        int64_t org_id,
        Entity entity,
        time_t start,
        time_t end,
        int64_t& out,
        chen::IDB::ptr conn);

    static bool CountByMinStatus(const OrgScope& scope,
        int64_t org_id,
        Entity entity,
        int32_t min_status,
        time_t start,
        time_t end,
        int64_t& out,
        chen::IDB::ptr conn);

    /**
     * @brief 平均流转周期（天）
     */
    static bool AvgCycleDays(const OrgScope& scope,
        int64_t org_id,
        Entity entity,
        int32_t min_status,
        time_t start,
        time_t end,
        double& out,
        chen::IDB::ptr conn);

    /**
     * @brief 按创建日期聚合数量（date 为 YYYY-MM-DD）
     */
    static bool ListCreatedByDay(const OrgScope& scope,
        int64_t org_id,
        Entity entity,
        int32_t min_status,
        time_t start,
        time_t end,
        std::vector<std::pair<std::string, int64_t>>& out,
        chen::IDB::ptr conn);

    static bool CountCreatedBefore(const OrgScope& scope,
        int64_t org_id,
        Entity entity,
        int64_t before,
        int64_t& out,
        chen::IDB::ptr conn);

    /**
     * @brief 全量状态统计（不限制时间范围）
     * @param now 用于判断任务是否逾期
     */
    static bool GetStatusStats(const OrgScope& scope,
        int64_t org_id,
        Entity entity,
        int64_t now,
        StatusStats& out,
        chen::IDB::ptr conn);
};
