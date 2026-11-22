/**
 * @file ArticleCounterService.hpp
 * @brief 文章浏览/点赞/收藏计数与刷盘
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <map>
#include <string>

class ArticleCounterService {
public:
    static bool IncViews(uint64_t id, const std::string& cookie_id, uint64_t user_id);

    static bool IncPraise(uint64_t id, const std::string& cookie_id, uint64_t user_id);

    static bool IncFavorites(uint64_t id, const std::string& cookie_id, uint64_t user_id);

    static bool DecPraise(uint64_t id, const std::string& cookie_id, uint64_t user_id);

    static bool DecFavorites(uint64_t id, const std::string& cookie_id, uint64_t user_id);

    static void IncPraiseCount(int64_t id);

    static void DecPraiseCount(int64_t id);

    static bool ListUserFav(int64_t id, std::map<int64_t, int64_t>& articles);

    static bool ListUserPra(int64_t id, std::map<int64_t, int64_t>& articles);

    static bool ListArticleFav(int64_t id, std::map<int64_t, int64_t>& users);

    static bool ListArticlePra(int64_t id, std::map<int64_t, int64_t>& users);

    static int64_t GetTodayViews();

    static int64_t GetTotalViews();

    static int64_t GetTotalVisitors();

    /**
     * @brief 将内存脏计数刷新到 DB
     */
    static void FlushDirty();

    /**
     * @brief 启动时从 DB 重算总浏览数
     */
    static void SyncStatsFromDB();
};
