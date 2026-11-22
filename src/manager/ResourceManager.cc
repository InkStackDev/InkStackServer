#include "ResourceManager.hpp"

#include <algorithm>
#include <cctype>

#include "../Repository/ResourceRepository.hpp"
#include "../Util/Util.hpp"

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

void ResourceManager::add(DB::ResourceInfo::ptr info)
{
    ResourceRepository::Put(info);
}

DB::ResourceInfo::ptr ResourceManager::get(int64_t id)
{
    return ResourceRepository::Get(id, GetDB());
}

void ResourceManager::getByBizUid(std::vector<DB::ResourceInfo::ptr>& results,
    const std::string& biz_type,
    int64_t biz_id,
    int64_t uid)
{
    ResourceRepository::ListByBizUid(results, biz_type, biz_id, uid, GetDB());
}

DB::ResourceInfo::ptr ResourceManager::getByBizUidName(const std::string& biz_type,
    int64_t biz_id,
    int64_t uid,
    const std::string& filename)
{
    return ResourceRepository::GetByBizUidName(biz_type, biz_id, uid, filename, GetDB());
}

DB::ResourceInfo::ptr ResourceManager::getByPath(const std::string& path)
{
    return ResourceRepository::GetByPath(path, GetDB());
}
