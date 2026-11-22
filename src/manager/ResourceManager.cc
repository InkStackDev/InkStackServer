#include "ResourceManager.hpp"

#include "../Util/Util.hpp"
#include "CacheUtil.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/JsonUtil.hpp>
#include <Chen/Util/Util.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 300;
static const char* kCachePrefix = "cache:obj:res:";

static std::string ResourceCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static DB::ResourceInfo::ptr ParseResourceCache(const Json::Value& v)
{
    auto info = std::make_shared<DB::ResourceInfo>();
    info->setId(CacheJsonInt64(v, "id"));
    info->setName(CacheJsonString(v, "name"));
    info->setPath(CacheJsonString(v, "path"));
    info->setType(CacheJsonInt32(v, "type"));
    info->setSize(CacheJsonInt64(v, "size"));
    info->setHash(CacheJsonString(v, "hash"));
    info->setOwnerId(CacheJsonInt64(v, "owner_id"));
    info->setBizType(CacheJsonString(v, "biz_type"));
    info->setBizId(CacheJsonInt64(v, "biz_id"));
    info->setStatus(CacheJsonInt32(v, "status"));
    info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
    info->setCreateTime(CacheJsonTime(v, "create_time"));
    info->setUpdateTime(CacheJsonTime(v, "update_time"));
    info->markClean();
    return info;
}

static DB::ResourceInfo::ptr GetResourceCache(int64_t id)
{
    std::string key = ResourceCacheKey(id);
    std::string data = RedisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        RedisCacheDel(key);
        return nullptr;
    }
    return ParseResourceCache(v);
}

static void SetResourceCache(const DB::ResourceInfo::ptr& info)
{
    if (!info) {
        return;
    }
    RedisCacheSetex(ResourceCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

ResourceManager::ResourceType ResourceManager::GetResourceType(const std::string& filename)
{
    auto pos = filename.rfind('.');
    if (pos == std::string::npos) {
        return TYPE_OTHER;
    }
    std::string ext = filename.substr(pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "bmp" || ext == "gif" || ext == "webp") {
        return TYPE_IMAGE;
    } else if (ext == "mp4" || ext == "avi" || ext == "mov" || ext == "wmv" || ext == "flv" || ext == "mkv") {
        return TYPE_VIDEO;
    } else if (
        ext == "pdf" || ext == "doc" || ext == "docx" || ext == "xls" || ext == "xlsx" || ext == "ppt" || ext == "pptx" || ext == "txt"
        || ext == "md") {
        return TYPE_DOCUMENT;
    } else if (ext == "zip" || ext == "rar" || ext == "7z" || ext == "tar" || ext == "gz") {
        return TYPE_COMPRESSED;
    } else if (ext == "mp3" || ext == "wav" || ext == "aac" || ext == "flac" || ext == "ogg" || ext == "m4a") {
        return TYPE_AUDIO;
    } else {
        return TYPE_OTHER;
    }
}

DB::ResourceInfo::ptr ResourceManager::ParseRow(chen::ISQLData::ptr rt)
{
    return DB::ResourceInfoDao::ParseRow(rt);
}

void ResourceManager::add(DB::ResourceInfo::ptr info)
{
    SetResourceCache(info);
}

DB::ResourceInfo::ptr ResourceManager::get(int64_t id)
{
    auto v = GetResourceCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = DB::ResourceInfoDao::Query(id, db);
    if (v) {
        SetResourceCache(v);
    }
    return v;
}

void ResourceManager::getByBizUid(std::vector<DB::ResourceInfo::ptr>& results,
    const std::string& biz_type,
    int64_t biz_id,
    int64_t uid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = DB::ResourceInfoDao::newQuery();
    qb->where("biz_type", "=", biz_type);
    qb->where("biz_id", "=", biz_id);
    qb->where("owner_id", "=", uid);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    if (DB::ResourceInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
}

DB::ResourceInfo::ptr ResourceManager::getByBizUidName(const std::string& biz_type,
    int64_t biz_id,
    int64_t uid,
    const std::string& filename)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto qb = DB::ResourceInfoDao::newQuery();
    qb->where("biz_type", "=", biz_type);
    qb->where("biz_id", "=", biz_id);
    qb->where("owner_id", "=", uid);
    qb->where("name", "=", filename);
    qb->limit(1);
    std::vector<DB::ResourceInfo::ptr> results;
    if (DB::ResourceInfoDao::QueryByBuilder(results, qb, db) || results.empty()) {
        return nullptr;
    }
    auto info = results[0];
    SetResourceCache(info);
    return info;
}

DB::ResourceInfo::ptr ResourceManager::getByPath(const std::string& path)
{
    std::string ck = "res:path:" + path;
    int64_t cachedId = GetCachedIdMapping(ck);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto qb = DB::ResourceInfoDao::newQuery();
    qb->where("path", "=", path);
    qb->limit(1);
    std::vector<DB::ResourceInfo::ptr> results;
    if (DB::ResourceInfoDao::QueryByBuilder(results, qb, db) || results.empty()) {
        return nullptr;
    }
    auto info = results[0];
    SetResourceCache(info);
    CacheIdMapping(ck, info->getId());
    return info;
}
