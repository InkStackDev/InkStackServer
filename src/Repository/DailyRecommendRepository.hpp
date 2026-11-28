/**
 * @file DailyRecommendRepository.hpp
 * @brief 每日推荐快照读写
 * @author Christins
 * @date 2026-11-28
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/ArticleDailyRecommendInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <string>
#include <vector>

class DailyRecommendRepository {
public:
    /**
     * @brief 判断某天是否已生成快照
     */
    static bool HasDate(const std::string& recommend_date, chen::IDB::ptr conn);

    /**
     * @brief 按名次升序读取某天的快照
     */
    static bool ListByDate(const std::string& recommend_date,
        std::vector<DB::ArticleDailyRecommendInfo::ptr>& rows,
        chen::IDB::ptr conn);

    /**
     * @brief 覆盖写入某天的快照（先删除当天旧数据再逐条插入）
     */
    static bool ReplaceDate(const std::string& recommend_date,
        const std::vector<DB::ArticleDailyRecommendInfo::ptr>& rows,
        chen::IDB::ptr conn);
};
