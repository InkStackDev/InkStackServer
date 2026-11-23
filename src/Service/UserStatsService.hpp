/**
 * @file UserStatsService.hpp
 * @brief 用户统计与成就用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

class UserStatsService {
public:
    struct Stats {
        int64_t total_articles = 0;
        int64_t published_articles = 0;
        int64_t private_articles = 0;
        int64_t total_views = 0;
        int64_t total_likes = 0;
        int64_t total_comments = 0;
        int64_t total_tags = 0;
        int64_t total_organizations = 0;
    };

    struct Badge {
        int64_t id = 0;
        std::string name;
        std::string description;
        std::string icon;
        std::string color;
        bool unlocked = false;
        int64_t current = 0;
        int64_t target = 0;
    };

    struct Achievements {
        int64_t total_contributions = 0;
        int64_t published_articles = 0;
        int64_t total_likes = 0;
        int64_t unlocked_badges = 0;
        int64_t total_badges = 0;
        std::vector<std::vector<int32_t>> heatmap;
        std::vector<Badge> badges;
    };

    static bool GetStats(int64_t uid, Stats& out);

    static bool GetAchievements(int64_t uid, Achievements& out);
};
