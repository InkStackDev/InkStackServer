#include "AssignmentRepository.hpp"

#include <memory>

#include "../Cache/EntityCache.hpp"
#include "../Manager/CacheUtil.hpp"

namespace {

struct AssignmentCodec {
    static DB::AssignmentInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::AssignmentInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setName(CacheJsonString(v, "name"));
        info->setSubjectName(CacheJsonString(v, "subject_name"));
        info->setPriority(CacheJsonInt32(v, "priority"));
        info->setStatus(CacheJsonInt32(v, "status"));
        info->setDescription(CacheJsonString(v, "description"));
        info->setMaxSize(CacheJsonInt32(v, "max_size"));
        info->setFileType(CacheJsonString(v, "file_type"));
        info->setDeadline(CacheJsonTime(v, "deadline"));
        info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
        info->setCreateTime(CacheJsonTime(v, "create_time"));
        info->setUpdateTime(CacheJsonTime(v, "update_time"));
        info->markClean();
        return info;
    }
};

const EntityCache<DB::AssignmentInfo, AssignmentCodec> g_assignment_cache{{"asg", 120}};

std::string NameKey(const std::string& subject_name, const std::string& name)
{
    return "assign:name:" + subject_name + ":" + name;
}

} // namespace

DB::AssignmentInfo::ptr AssignmentRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return g_assignment_cache.Get(id, [id, conn]() {
        return DB::AssignmentInfoDao::Query(id, conn);
    });
}

DB::AssignmentInfo::ptr AssignmentRepository::GetByName(const std::string& subject_name,
    const std::string& name,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return nullptr;
    }
    const std::string key = NameKey(subject_name, name);
    int64_t cached_id = GetCachedIdMapping(key);
    if (cached_id > 0) {
        auto info = Get(cached_id, conn);
        if (info) {
            return info;
        }
        RedisCacheDel("cache:map:" + key);
    }

    std::vector<DB::AssignmentInfo::ptr> results;
    if (DB::AssignmentInfoDao::QueryBySubjectNameName(results, subject_name, name, conn)) {
        return nullptr;
    }
    if (results.empty()) {
        return nullptr;
    }
    Put(results[0]);
    CacheIdMapping(key, results[0]->getId());
    return results[0];
}

bool AssignmentRepository::InsertOrUpdate(DB::AssignmentInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::AssignmentInfoDao::InsertOrUpdate(info, conn) == 0;
}

bool AssignmentRepository::Update(DB::AssignmentInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::AssignmentInfoDao::Update(info, conn) == 0;
}

void AssignmentRepository::Put(const DB::AssignmentInfo::ptr& info)
{
    g_assignment_cache.Put(info);
}

void AssignmentRepository::Evict(int64_t id)
{
    g_assignment_cache.Evict(id);
}

void AssignmentRepository::EvictName(const std::string& subject_name, const std::string& name)
{
    RedisCacheDel("cache:map:" + NameKey(subject_name, name));
}
