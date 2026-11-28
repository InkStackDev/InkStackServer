/**
 * @file DailyRecommendService.hpp
 * @brief 每日推荐用例：快照生成、个性化重排与结果缓存
 * @author Christins
 * @date 2026-11-28
 * @copyright Apache 2.0
 */
#pragma once

#include <string>
#include <vector>

class DailyRecommendService {
public:
    /**
     * @brief 推荐条目（接口返回与缓存共用）
     */
    struct Item {
        int64_t article_id = 0;
        int64_t user_id = 0;
        std::string author;
        std::string author_avatar;
        std::string title;
        std::string summary;
        int64_t publish_time = 0;
        int64_t views = 0;
        int64_t praise = 0;
        int64_t favorites = 0;
        int64_t score = 0;
        int32_t rank_no = 0;
        std::string reason;
    };

    /**
     * @brief 今天的推荐日期（YYYY-MM-DD）
     */
    static std::string Today();

    /**
     * @brief 幂等生成指定日期的推荐快照
     * @return 当天快照已存在或生成成功返回 true
     */
    static bool EnsureSnapshot(const std::string& recommend_date);

    /**
     * @brief 读取每日推荐；user_id 为 0 或未开启个性化时返回全局榜
     * @param items 输出条目，按名次升序
     * @param user_id 当前登录用户，0 表示匿名
     */
    static bool ListDaily(std::vector<Item>& items, int64_t user_id);

private:
    static bool BuildSnapshot(const std::string& recommend_date);
    static bool LoadSnapshot(const std::string& recommend_date, std::vector<Item>& rows);
    static void Personalize(int64_t user_id, std::vector<Item>& rows);
    static bool FillDetails(std::vector<Item>& items);
    static bool LoadCache(const std::string& key, std::vector<Item>& items);
    static void SaveCache(const std::string& key, const std::vector<Item>& items, int32_t ttl_sec);
};
