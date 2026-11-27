/**
 * @file DashboardService.hpp
 * @brief 管理端仪表盘查询用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class DashboardService {
public:
    struct Stats {
        int64_t total_users = 0;
        int64_t total_users_change = 0;
        int64_t total_articles = 0;
        int64_t total_articles_change = 0;
        int64_t total_comments = 0;
        int64_t total_comments_change = 0;
        int64_t today_visits = 0;
        int64_t today_visits_change = 0;
        int64_t new_users_today = 0;
        int64_t new_users_today_change = 0;
    };

    struct Trend {
        int32_t num_buckets = 0;
        int64_t period_start = 0;
        int64_t bucket_sec = 0;
        std::vector<int64_t> buckets;
    };

    struct Activity {
        std::string type;
        std::string title;
        std::string title_id;
        std::map<std::string, std::string> params;
        int64_t create_time = 0;
    };

    struct RecentUser {
        std::string name;
        std::string avatar;
        int32_t role = 0;
        int32_t state = 0;
    };

    static bool GetStats(Stats& out);

    static bool GetTrend(int32_t period, Trend& out);

    static bool GetActivities(int32_t limit, std::vector<Activity>& out);

    static bool GetRecentUsers(int32_t limit, std::vector<RecentUser>& out);
};
