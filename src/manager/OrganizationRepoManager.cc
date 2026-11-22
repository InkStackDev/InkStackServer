#include "OrganizationRepoManager.hpp"

#include "../Repository/OrganizationRepoRepository.hpp"
#include "../Service/OrganizationRepoService.hpp"
#include "../Util/Util.hpp"

void OrganizationRepoManager::add(DB::OrganizationReposInfo::ptr info)
{
    if (!info) {
        return;
    }
    OrganizationRepoRepository::Put(info);
    OrganizationRepoRepository::InvalidateOrgCount(info->getOrgId());
}

void OrganizationRepoManager::del(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        OrganizationRepoRepository::Evict(id);
        return;
    }
    auto info = OrganizationRepoRepository::Get(id, db);
    if (info) {
        OrganizationRepoRepository::InvalidateOrgCount(info->getOrgId());
    }
    OrganizationRepoRepository::Evict(id);
}

void OrganizationRepoManager::onTick()
{
    OrganizationRepoService::SyncAll();
}

DB::OrganizationReposInfo::ptr OrganizationRepoManager::get(int64_t id)
{
    return OrganizationRepoRepository::Get(id, GetDB());
}

DB::OrganizationReposInfo::ptr OrganizationRepoManager::getByOrgAndName(int64_t org_id, const std::string& name)
{
    return OrganizationRepoRepository::GetByOrgAndName(org_id, name, GetDB());
}

int64_t OrganizationRepoManager::listByOrgPages(std::vector<DB::OrganizationReposInfo::ptr>& repos,
    int64_t org_id,
    uint64_t offset,
    uint64_t limit)
{
    return OrganizationRepoRepository::ListByOrgPages(repos, org_id, offset, limit, GetDB());
}

int64_t OrganizationRepoManager::getCountByOrg(int64_t org_id)
{
    return OrganizationRepoRepository::CountByOrg(org_id, GetDB());
}

void OrganizationRepoManager::getAllWithToken(std::vector<DB::OrganizationReposInfo::ptr>& repos)
{
    OrganizationRepoRepository::ListWithToken(repos, GetDB());
}

void OrganizationRepoManager::SyncFromGitHub(int64_t repo_id, const std::string& url, const std::string& token)
{
    // url/token 参数保留用于兼容旧调用；实际同步会读取仓库当前配置
    (void)url;
    (void)token;
    OrganizationRepoService::SyncFromGitHub(repo_id);
}
