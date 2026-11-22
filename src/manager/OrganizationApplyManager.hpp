#pragma once

#include "DB/OrganizationApplyInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

class OrganizationApplyManager {
public:
    enum Status {
        PENDING = 0,
        APPROVED = 1,
        REJECTED = 2
    };

    DB::OrganizationApplyInfo::ptr get(int64_t id);

    int64_t listByPages(std::vector<DB::OrganizationApplyInfo::ptr>& results,
        uint64_t offset,
        uint64_t limit,
        int32_t status,
        bool is_valid);

    int64_t listByUserId(std::vector<DB::OrganizationApplyInfo::ptr>& results,
        int64_t user_id,
        uint64_t offset,
        uint64_t limit,
        bool is_valid);
};

typedef chen::Singleton<OrganizationApplyManager> OrganizationApplyMgr;
