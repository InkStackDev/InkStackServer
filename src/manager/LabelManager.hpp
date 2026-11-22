#pragma once

#include "DB/LabelInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

class LabelManager {
public:
    void add(DB::LabelInfo::ptr info);

    DB::LabelInfo::ptr get(int64_t id);

    DB::LabelInfo::ptr getByUserIdName(int64_t id, const std::string& name);

    bool listByUserId(std::vector<DB::LabelInfo::ptr>& infos, int64_t id, bool valid);
};

typedef chen::Singleton<LabelManager> LabelMgr;
