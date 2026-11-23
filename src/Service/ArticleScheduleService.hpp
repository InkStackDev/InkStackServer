/**
 * @file ArticleScheduleService.hpp
 * @brief 文章定时发布与发布日历缓存
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <vector>

class ArticleScheduleService {
public:
    static void Schedule(int64_t article_id, int64_t publish_time);

    static void Unschedule(int64_t article_id);

    /**
     * @brief 发布所有到期文章（由后台任务定时调用）
     */
    static void PublishDue();

    /**
     * @brief 查询某月有已发布文章的日期
     */
    static void GetCalendarDays(int64_t user_id, int32_t year, int32_t month, std::vector<int32_t>& days);

    /**
     * @brief 清除发布时间对应月份的日历缓存
     */
    static void ClearCalendarCache(int64_t user_id, int64_t publish_time);
};
