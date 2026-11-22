#include "OrganizationRepoRepository.hpp"

#include <memory>
#include <sstream>

#include <Chen/DB/Redis.hpp>

#include "../Cache/EntityCache.hpp"
#include "../Config/AppConfig.hpp"
#include "../Manager/CacheUtil.hpp"
#include "../Util/Util.hpp"

namespace {

struct OrgRepoCodec {
    static DB::OrganizationReposInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::OrganizationReposInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setOrgId(CacheJsonInt64(v, "org_id"));
        info->setName(CacheJsonString(v, "name"));
        info->setDescription(CacheJsonString(v, "description"));
        info->setUrl(CacheJsonString(v, "url"));
        info->setLanguage(CacheJsonString(v, "language"));
        info->setStarsCount(CacheJsonInt32(v, "stars_count"));
        info->setSortOrder(CacheJsonInt32(v, "sort_order"));
        info->setCreateTime(CacheJsonTime(v, "create_time"));
        info->setUpdateTime(CacheJsonTime(v, "update_time"));
        info->setToken(CacheJsonString(v, "token"));
        info->setSyncStatus(CacheJsonString(v, "sync_status"));
        info->setPrSyncStatus(CacheJsonString(v, "pr_sync_status"));
        info->setPrSyncedAt(CacheJsonInt64(v, "pr_synced_at"));
        info->setGithubFullName(CacheJsonString(v, "github_full_name"));
        info->markClean();
        return info;
    }
};

const EntityCache<DB::OrganizationReposInfo, OrgRepoCodec> g_org_repo_cache{{"org_repo", 300}};

} // namespace

DB::OrganizationReposInfo::ptr OrganizationRepoRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return g_org_repo_cache.Get(id, [id, conn]() {
        return DB::OrganizationReposInfoDao::Query(id, conn);
    });
}

DB::OrganizationReposInfo::ptr OrganizationRepoRepository::GetByOrgAndName(int64_t org_id,
    const std::string& name,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return nullptr;
    }
    auto info = DB::OrganizationReposInfoDao::QueryByOrgIdName(org_id, name, conn);
    if (info) {
        Put(info);
    }
    return info;
}

int64_t OrganizationRepoRepository::ListByOrgPages(std::vector<DB::OrganizationReposInfo::ptr>& repos,
    int64_t org_id,
    uint64_t offset,
    uint64_t limit,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::OrganizationReposInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("org_id", "=", org_id);
    qb->orderBy("sort_order DESC, id", "DESC");

    int64_t total = 0;
    if (DB::OrganizationReposInfoDao::QueryByBuilderPages(repos, total, qb, (int32_t)offset, (int32_t)limit, conn)) {
        return 0;
    }
    return total;
}

int64_t OrganizationRepoRepository::CountByOrg(int64_t org_id, chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::OrganizationReposInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("org_id", "=", org_id);
    std::stringstream key;
    key << "org_repo:count:" << org_id;
    return ExecuteCountCached(qb, conn, key.str());
}

void OrganizationRepoRepository::ListWithToken(std::vector<DB::OrganizationReposInfo::ptr>& repos, chen::IDB::ptr conn)
{
    if (!conn) {
        return;
    }
    auto qb = DB::OrganizationReposInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->where("token", "!=", "");
    DB::OrganizationReposInfoDao::QueryByBuilder(repos, qb, conn);
}

bool OrganizationRepoRepository::Insert(DB::OrganizationReposInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::OrganizationReposInfoDao::Insert(info, conn) == 0;
}

bool OrganizationRepoRepository::Update(DB::OrganizationReposInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::OrganizationReposInfoDao::Update(info, conn) == 0;
}

bool OrganizationRepoRepository::DeleteById(int64_t id, chen::IDB::ptr conn)
{
    if (!conn) {
        return false;
    }
    return DB::OrganizationReposInfoDao::DeleteById(id, conn) == 0;
}

void OrganizationRepoRepository::Put(const DB::OrganizationReposInfo::ptr& info)
{
    g_org_repo_cache.Put(info);
}

void OrganizationRepoRepository::Evict(int64_t id)
{
    g_org_repo_cache.Evict(id);
}

void OrganizationRepoRepository::InvalidateOrgCount(int64_t org_id)
{
    if (org_id <= 0) {
        return;
    }
    std::stringstream count_key;
    count_key << "cache:count:org_repo:count:" << org_id;
    chen::RedisUtil::Cmd(GetRedisPoolName(), "del %s", count_key.str().c_str());

    std::stringstream list_key;
    list_key << "cache:count:org_repo:list:" << org_id;
    chen::RedisUtil::Cmd(GetRedisPoolName(), "del %s", list_key.str().c_str());
}

int64_t OrganizationRepoRepository::FindIdByGithubFullName(const std::string& full_name, chen::IDB::ptr conn)
{
    if (!conn || full_name.empty()) {
        return 0;
    }
    auto qb = DB::OrganizationReposInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->select("id");
    qb->where("github_full_name", "=", full_name);
    qb->limit(1);
    int64_t repo_id = 0;
    if (qb->queryScalarInt64(repo_id, conn)) {
        return 0;
    }
    return repo_id;
}
