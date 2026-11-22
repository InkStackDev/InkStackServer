/**
 * @file CategoryService.hpp
 * @brief 分类用例：创建/更新与级联删除
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "../Domain/AccessTypes.hpp"

#include "DB/CategoryInfo.hpp"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

struct CategoryInput {
    int64_t id = 0;
    int64_t owner_id = 0;
    std::string name;
    std::string color;
    std::string description;
    std::string url;
    std::string icon;
    int64_t parent_id = 0;
    int32_t status = 1;
};

class CategoryService {
public:
    enum class Result {
        Ok,
        NotFound,
        AccessDenied,
        InvalidParent,
        NameExists,
        DbError
    };

    static Result CreateOrUpdate(const CategoryInput& input,
        int64_t requester_uid,
        bool admin_route,
        DB::CategoryInfo::ptr& out);

    /**
     * @brief 按 ID 级联删除分类及其子分类
     * @param scope 调用方可见的分类集合
     */
    static bool DeleteMany(const std::vector<DB::CategoryInfo::ptr>& scope,
        const std::set<int64_t>& root_ids,
        std::vector<DB::CategoryInfo::ptr>& deleted);

    enum class DeleteScopeResult {
        Ok,
        NoCategories,
        DbError
    };

    static DeleteScopeResult DeleteForUser(bool admin_route,
        int64_t requester_uid,
        int64_t owner_uid,
        const std::set<int64_t>& root_ids,
        std::vector<DB::CategoryInfo::ptr>& deleted);
};
