#include "ResourceService.hpp"

#include "../Repository/ResourceRepository.hpp"
#include "../Util/Util.hpp"

bool ResourceService::GetByPath(const std::string& path, DB::ResourceInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return false;
    }
    out = ResourceRepository::GetByPath(path, db);
    return out != nullptr;
}

bool ResourceService::ListByBizUid(const std::string& biz_type,
    int64_t biz_id,
    int64_t uid,
    std::vector<DB::ResourceInfo::ptr>& out)
{
    out.clear();
    auto db = GetDB();
    if (!db) {
        return false;
    }
    ResourceRepository::ListByBizUid(out, biz_type, biz_id, uid, db);
    return true;
}
