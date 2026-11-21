#pragma once

#include "blog/data/bug_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

class BugManager {
public:
    enum Severity {
        SEVERITY_MINOR = 1,
        SEVERITY_NORMAL = 2,
        SEVERITY_MAJOR = 3,
        SEVERITY_CRITICAL = 4
    };
    enum Priority {
        PRIORITY_LOW = 1,
        PRIORITY_MEDIUM = 2,
        PRIORITY_HIGH = 3,
        PRIORITY_URGENT = 4
    };
    enum Status {
        STATUS_PENDING = 0,
        STATUS_INPROGRESS = 1,
        STATUS_FIXED = 2,
        STATUS_CLOSED = 3,
        STATUS_REOPENED = 4
    };

    data::BugInfo::ptr get(int64_t id);

    uint64_t listByPages(std::vector<data::BugInfo::ptr>& infos,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool is_valid);

    uint64_t listByOrg(std::vector<data::BugInfo::ptr>& infos,
        int64_t org_id,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool is_valid);

private:
    static data::BugInfo::ptr ParseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<BugManager> BugMgr;
