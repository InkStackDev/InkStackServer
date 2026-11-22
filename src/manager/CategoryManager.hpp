#pragma once

#include "DB/CategoryInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

class CategoryManager {
public:
    void listAll(std::vector<DB::CategoryInfo::ptr>& infos, bool is_valid = false);

    bool listByUserId(std::vector<DB::CategoryInfo::ptr>& infos, int64_t user_id, bool is_valid = false);

    void add(DB::CategoryInfo::ptr info);

    DB::CategoryInfo::ptr get(int64_t id);

    DB::CategoryInfo::ptr getByUserIdName(int64_t user_id, const std::string& name);
};

typedef chen::Singleton<CategoryManager> CategoryMgr;
