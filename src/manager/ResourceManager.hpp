#pragma once

#include "DB/ResourceInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

class ResourceManager {
public:
    enum ResourceType {
        TYPE_IMAGE = 1,
        TYPE_VIDEO = 2,
        TYPE_DOCUMENT = 3,
        TYPE_COMPRESSED = 4,
        TYPE_AUDIO = 5,
        TYPE_OTHER = 6,
    };

    enum Status {
        NORMAL = 1,
        DELETED = 2,
    };

    ResourceType GetResourceType(const std::string& filename);

    void add(DB::ResourceInfo::ptr info);

    DB::ResourceInfo::ptr get(int64_t id);

    void getByBizUid(std::vector<DB::ResourceInfo::ptr>& results,
        const std::string& biz_type,
        int64_t biz_id,
        int64_t uid);

    DB::ResourceInfo::ptr getByBizUidName(const std::string& biz_type,
        int64_t biz_id,
        int64_t uid,
        const std::string& filename);

    DB::ResourceInfo::ptr getByPath(const std::string& path);

private:
    static DB::ResourceInfo::ptr ParseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<ResourceManager> ResourceMgr;
