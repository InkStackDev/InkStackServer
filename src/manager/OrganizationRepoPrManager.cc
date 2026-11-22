#include "OrganizationRepoPrManager.hpp"

#include "../Repository/OrganizationRepoPrRepository.hpp"
#include "../Service/OrganizationRepoPrService.hpp"
#include "../Util/Util.hpp"

void OrganizationRepoPrManager::add(DB::OrganizationRepoPrsInfo::ptr info)
{
    OrganizationRepoPrRepository::Put(info);
}

void OrganizationRepoPrManager::del(int64_t id)
{
    OrganizationRepoPrRepository::Evict(id);
}

void OrganizationRepoPrManager::onTick()
{
    OrganizationRepoPrService::SyncAll();
}

DB::OrganizationRepoPrsInfo::ptr OrganizationRepoPrManager::get(int64_t id)
{
    return OrganizationRepoPrRepository::Get(id, GetDB());
}

DB::OrganizationRepoPrsInfo::ptr OrganizationRepoPrManager::getByRepoAndPrId(int64_t repo_id, int32_t github_pr_id)
{
    return OrganizationRepoPrRepository::GetByRepoAndPrId(repo_id, github_pr_id, GetDB());
}

int64_t OrganizationRepoPrManager::listByRepoPages(std::vector<DB::OrganizationRepoPrsInfo::ptr>& prs,
    int64_t repo_id,
    const std::string& state,
    uint64_t offset,
    uint64_t limit)
{
    return OrganizationRepoPrRepository::ListByRepoPages(prs, repo_id, state, offset, limit, GetDB());
}

int64_t OrganizationRepoPrManager::getCountByRepo(int64_t repo_id)
{
    return OrganizationRepoPrRepository::CountByRepo(repo_id, GetDB());
}

int64_t OrganizationRepoPrManager::listByOrgPages(std::vector<DB::OrganizationRepoPrsInfo::ptr>& prs,
    int64_t org_id,
    const std::string& state,
    uint64_t offset,
    uint64_t limit)
{
    return OrganizationRepoPrRepository::ListByOrgPages(prs, org_id, state, offset, limit, GetDB());
}

int64_t OrganizationRepoPrManager::listByUserPages(std::vector<DB::OrganizationRepoPrsInfo::ptr>& prs,
    int64_t uid,
    const std::string& state,
    uint64_t offset,
    uint64_t limit)
{
    return OrganizationRepoPrRepository::ListByUserPages(prs, uid, state, offset, limit, GetDB());
}

void OrganizationRepoPrManager::SyncFromGitHub(int64_t repo_id,
    const std::string& repo_url,
    const std::string& token,
    int max_prs)
{
    OrganizationRepoPrService::SyncFromGitHub(repo_id, repo_url, token, max_prs);
}

OrganizationRepoPrManager::SyncPrStatus OrganizationRepoPrManager::SyncPrFromGitHub(int64_t pr_id,
    DB::OrganizationRepoPrsInfo::ptr& out)
{
    return OrganizationRepoPrService::SyncPrFromGitHub(pr_id, out);
}

DB::OrganizationReposInfo::ptr OrganizationRepoPrManager::FindRepoByOwnerAndName(const std::string& owner,
    const std::string& name)
{
    return OrganizationRepoPrService::FindRepoByOwnerAndName(owner, name);
}
