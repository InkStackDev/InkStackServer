#pragma once

#include "DB/RequirementInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

class RequirementManager {
public:
    enum Priority {
        PRIORITY_LOW = 1,
        PRIORITY_MEDIUM = 2,
        PRIORITY_HIGH = 3,
        PRIORITY_URGENT = 4
    };
    enum Status {
        STATUS_DRAFT = 0,
        STATUS_PENDING = 1,
        STATUS_INPROGRESS = 2,
        STATUS_DONE = 3,
        STATUS_CLOSED = 4
    };

    DB::RequirementInfo::ptr get(int64_t id);

    uint64_t listByPages(std::vector<DB::RequirementInfo::ptr>& infos,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool is_valid);

    uint64_t listByOrg(std::vector<DB::RequirementInfo::ptr>& infos,
        int64_t org_id,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool is_valid);
};

typedef chen::Singleton<RequirementManager> RequirementMgr;
