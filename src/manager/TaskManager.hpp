#pragma once

#include "DB/TaskInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

class TaskManager {
public:
    enum Priority {
        PRIORITY_LOW = 1,
        PRIORITY_MEDIUM = 2,
        PRIORITY_HIGH = 3,
        PRIORITY_URGENT = 4
    };
    enum Status {
        STATUS_TODO = 0,
        STATUS_INPROGRESS = 1,
        STATUS_DONE = 2,
        STATUS_CLOSED = 3
    };

    DB::TaskInfo::ptr get(int64_t id);

    uint64_t listByPages(std::vector<DB::TaskInfo::ptr>& infos,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool is_valid);

    uint64_t listByOrg(std::vector<DB::TaskInfo::ptr>& infos,
        int64_t org_id,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool is_valid);

private:
    static DB::TaskInfo::ptr ParseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<TaskManager> TaskMgr;
