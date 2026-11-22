/**
 * @file RdAnalyticsService.hpp
 * @brief 研发数据分析用例（趋势、概览、PR 分布）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "../Domain/AccessTypes.hpp"

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

/**
 * @brief 研发分析用例
 * @details 入口只传 Viewer 与原始查询参数；组织范围在 Service 内解析，
 *          统计查询全部走 QueryRepository，入口不再接触 SQL 与成员关系。
 */
class RdAnalyticsService {
public:
    enum class Result {
        Ok,
        AccessDenied,
        NoAccessibleOrg,
        InvalidRange,
        InvalidParam,
        DbError
    };

    struct TrendQuery {
        int64_t org_id = 0;
        int64_t period_days = 30;
        std::string start_date;
        std::string end_date;
        std::string granularity = "day";
    };

    struct TrendPoint {
        std::string date;
        int64_t requirements = 0;
        int64_t bugs = 0;
        int64_t tasks = 0;
        int64_t completed = 0;
        int64_t reopened = 0;
        int64_t review_pass_rate = 0;
        double cycle_time = 0.0;
    };

    struct TrendResult {
        int64_t completed_total = 0;
        int64_t bug_total = 0;
        int64_t review_pass_rate = 0;
        double avg_cycle_time = 0.0;
        int64_t task_delta = 0;
        double cycle_delta = 0.0;
        int64_t new_requirements = 0;
        int64_t new_tasks = 0;
        int64_t pr_count = 0;
        std::vector<TrendPoint> series;
    };

    static Result GetTrend(const Viewer& viewer, const TrendQuery& query, TrendResult& out);

    struct Overview {
        int64_t total_requirements = 0;
        int64_t open_requirements = 0;
        int64_t total_bugs = 0;
        int64_t unresolved_bugs = 0;
        int64_t total_tasks = 0;
        int64_t overdue_tasks = 0;
    };

    static Result GetOverview(const Viewer& viewer, int64_t org_id, Overview& out);

    struct PrDistributionQuery {
        int64_t org_id = 0;
        std::string range;
        std::string granularity;
    };

    struct PrPoint {
        std::string date;
        std::string end_date;
        int64_t total = 0;
        int64_t merged = 0;
    };

    struct PrDistribution {
        int64_t total = 0;
        int64_t merged = 0;
        double merge_rate = 0.0;
        std::vector<PrPoint> points;
    };

    static Result GetPrDistribution(const Viewer& viewer, const PrDistributionQuery& query, PrDistribution& out);
};
