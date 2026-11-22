#pragma once

#include "DB/AssignmentUserRelInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

class AssignmentUserRelManager {
public:
    enum Status {
        SUBMITTED = 1,
        GRADED = 2,
        LATE = 3
    };

    DB::AssignmentUserRelInfo::ptr get(int64_t id);

    DB::AssignmentUserRelInfo::ptr getByAssignAndUser(int64_t assign_id, int64_t user_id);
};

typedef chen::Singleton<AssignmentUserRelManager> AssignmentUserRelMgr;
