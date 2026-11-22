#pragma once

#include "DB/OrganizationInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

#include "../Repository/OrganizationRepository.hpp"

class OrganizationManager {
public:
    enum Role {
        MEMBER = 1,
        REPORTER = 2,
        DEVELOPER = 3,
        DEV_LEAD = 4,
        ORG_ADMIN = 5
    };
    enum Status {
        INACTIVE = 0,
        ACTIVE = 1,
    };

    void add(DB::OrganizationInfo::ptr info);

    DB::OrganizationInfo::ptr get(int64_t id);

    DB::OrganizationInfo::ptr getByName(const std::string& name);

    int64_t listByPages(std::vector<DB::OrganizationInfo::ptr>& orgs,
        uint64_t offset,
        uint64_t limit,
        int32_t status,
        bool is_valid);

    using OrganizationStats = OrganizationQueryRepository::Stats;

    OrganizationStats getStats();
};

typedef chen::Singleton<OrganizationManager> OrganizationMgr;
