#pragma once

#include <memory>

#include "DB/AssignmentInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

#include "../Repository/AssignmentQueryRepository.hpp"

class AssignmentManager {
public:
    typedef std::shared_ptr<AssignmentManager> ptr;

    enum Status {
        DRAFT = 0,
        ACTIVE = 1,
        INACTIVE = 2,
    };

    void add(DB::AssignmentInfo::ptr info);

    uint64_t listByPages(std::vector<DB::AssignmentInfo::ptr>& infos,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool is_valid);

    DB::AssignmentInfo::ptr get(int64_t id);

    DB::AssignmentInfo::ptr getByName(const std::string& subject_name, const std::string& name);

    using AssignmentStats = AssignmentQueryRepository::Stats;

    AssignmentStats getStats();
};

typedef chen::Singleton<AssignmentManager> AssignmentMgr;
