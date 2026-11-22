#pragma once

#include "DB/OrganizationUserRelInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

#include "../Repository/OrganizationUserRelRepository.hpp"

class OrganizationUserRelManager {
public:
    enum Status {
        PENDING = 0,
        APPROVED = 1,
        REJECTED = 2,
        EXITED = 3
    };

    void add(DB::OrganizationUserRelInfo::ptr info);

    DB::OrganizationUserRelInfo::ptr get(int64_t id);

    DB::OrganizationUserRelInfo::ptr getByOrgAndUser(int64_t o_id, int64_t u_id);

    int64_t getByPages(std::vector<DB::OrganizationUserRelInfo::ptr>& results,
        int64_t o_id,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool is_valid);

    int64_t getOrgByUserId(std::vector<DB::OrganizationUserRelInfo::ptr>& results,
        int64_t u_id,
        int32_t status,
        bool is_valid);

    int64_t getMemberCount(int64_t o_id, int32_t status, bool is_valid);

    using Stats = OrganizationUserRelRepository::Stats;

    Stats getStats(int64_t org_id);
};

typedef chen::Singleton<OrganizationUserRelManager> OrganizationUserRelMgr;
