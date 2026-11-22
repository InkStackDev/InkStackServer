/**
 * @file PrStatsQueryRepository.hpp
 * @brief PR 报表统计查询（与业务读写分离）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

class PrStatsQueryRepository {
public:
    /**
     * @brief 按天聚合 PR 创建数
     * @param rows 日期（YYYY-MM-DD）→ 数量
     * @param merged_only 只统计已合入的 PR
     */
    static bool CountByDay(std::vector<std::pair<std::string, int64_t>>& rows,
        const std::vector<int64_t>& org_ids,
        time_t start,
        time_t end,
        bool merged_only,
        chen::IDB::ptr conn);

    /**
     * @brief 统计已有人工评审结论的 PR 数（review_status 非空且非 pending）
     */
    static int64_t CountReviewed(const std::vector<int64_t>& org_ids, time_t start, time_t end, chen::IDB::ptr conn);

    /**
     * @brief 统计评审通过的 PR 数
     */
    static int64_t CountApproved(const std::vector<int64_t>& org_ids, time_t start, time_t end, chen::IDB::ptr conn);

    /**
     * @brief 统计时间范围内创建的 PR 数
     */
    static int64_t CountCreated(const std::vector<int64_t>& org_ids, time_t start, time_t end, chen::IDB::ptr conn);
};
