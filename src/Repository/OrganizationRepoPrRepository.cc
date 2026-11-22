#include "OrganizationRepoPrRepository.hpp"

#include <memory>

#include "../Cache/EntityCache.hpp"

namespace {

constexpr int32_t kStatusApproved = 1;

struct OrgRepoPrCodec {
    static DB::OrganizationRepoPrsInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::OrganizationRepoPrsInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setRepoId(CacheJsonInt64(v, "repo_id"));
        info->setGithubPrId(CacheJsonInt32(v, "github_pr_id"));
        info->setTitle(CacheJsonString(v, "title"));
        info->setDescription(CacheJsonString(v, "description"));
        info->setState(CacheJsonString(v, "state"));
        info->setPriority(CacheJsonString(v, "priority"));
        info->setAuthor(CacheJsonString(v, "author"));
        info->setHeadBranch(CacheJsonString(v, "head_branch"));
        info->setBaseBranch(CacheJsonString(v, "base_branch"));
        info->setCommitSha(CacheJsonString(v, "commit_sha"));
        info->setChangedFiles(CacheJsonInt32(v, "changed_files"));
        info->setAdditions(CacheJsonInt32(v, "additions"));
        info->setDeletions(CacheJsonInt32(v, "deletions"));
        info->setReviewers(CacheJsonString(v, "reviewers"));
        info->setReviewStatus(CacheJsonString(v, "review_status"));
        info->setClosedAt(CacheJsonTime(v, "closed_at"));
        info->setMergedAt(CacheJsonTime(v, "merged_at"));
        info->setCreateTime(CacheJsonTime(v, "create_time"));
        info->setUpdateTime(CacheJsonTime(v, "update_time"));
        info->markClean();
        return info;
    }
};

const EntityCache<DB::OrganizationRepoPrsInfo, OrgRepoPrCodec> g_org_pr_cache{{"org_pr", 300}};

} // namespace

DB::OrganizationRepoPrsInfo::ptr OrganizationRepoPrRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return g_org_pr_cache.Get(id, [id, conn]() {
        return DB::OrganizationRepoPrsInfoDao::Query(id, conn);
    });
}

DB::OrganizationRepoPrsInfo::ptr OrganizationRepoPrRepository::GetByRepoAndPrId(int64_t repo_id,
    int32_t github_pr_id,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return nullptr;
    }
    auto info = DB::OrganizationRepoPrsInfoDao::QueryByRepoIdGithubPrId(repo_id, github_pr_id, conn);
    if (info) {
        Put(info);
    }
    return info;
}

int64_t OrganizationRepoPrRepository::ListByRepoPages(std::vector<DB::OrganizationRepoPrsInfo::ptr>& prs,
    int64_t repo_id,
    const std::string& state,
    uint64_t offset,
    uint64_t limit,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::OrganizationRepoPrsInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("repo_id", "=", repo_id);
    qb->whereIf(!state.empty(), "state", "=", state);
    qb->orderBy("create_time", "DESC");

    int64_t total = 0;
    if (DB::OrganizationRepoPrsInfoDao::QueryByBuilderPages(prs, total, qb, (int32_t)offset, (int32_t)limit, conn)) {
        return 0;
    }
    return total;
}

int64_t OrganizationRepoPrRepository::CountByRepo(int64_t repo_id, chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::OrganizationRepoPrsInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("repo_id", "=", repo_id);
    int64_t total = 0;
    if (qb->executeCount(total, conn)) {
        return 0;
    }
    return total;
}

int64_t OrganizationRepoPrRepository::ListByOrgPages(std::vector<DB::OrganizationRepoPrsInfo::ptr>& prs,
    int64_t org_id,
    const std::string& state,
    uint64_t offset,
    uint64_t limit,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::OrganizationRepoPrsInfoDao::newQuery("prs");
    if (!qb) {
        return 0;
    }
    qb->select("prs.*");
    qb->join("organization_repos repos", "prs.repo_id = repos.id");
    qb->where("repos.org_id", "=", org_id);
    qb->whereIf(!state.empty(), "prs.state", "=", state);
    qb->orderBy("prs.create_time", "DESC");

    int64_t total = 0;
    if (DB::OrganizationRepoPrsInfoDao::QueryByBuilderPages(prs, total, qb, (int32_t)offset, (int32_t)limit, conn)) {
        return 0;
    }
    return total;
}

int64_t OrganizationRepoPrRepository::ListByUserPages(std::vector<DB::OrganizationRepoPrsInfo::ptr>& prs,
    int64_t uid,
    const std::string& state,
    uint64_t offset,
    uint64_t limit,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::OrganizationRepoPrsInfoDao::newQuery("prs");
    if (!qb) {
        return 0;
    }
    qb->select("prs.*");
    qb->join("organization_repos repos", "prs.repo_id = repos.id");
    qb->join("INNER", "organization_user_rel rel", "repos.org_id = rel.org_id");
    qb->where("rel.user_id", "=", uid);
    qb->where("rel.status", "=", (int64_t)kStatusApproved);
    qb->where("rel.is_deleted", "=", (int64_t)0);
    qb->whereIf(!state.empty(), "prs.state", "=", state);
    qb->orderBy("prs.create_time", "DESC");

    int64_t total = 0;
    if (DB::OrganizationRepoPrsInfoDao::QueryByBuilderPages(prs, total, qb, (int32_t)offset, (int32_t)limit, conn)) {
        return 0;
    }
    return total;
}

bool OrganizationRepoPrRepository::Insert(DB::OrganizationRepoPrsInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::OrganizationRepoPrsInfoDao::Insert(info, conn) == 0;
}

bool OrganizationRepoPrRepository::InsertOrUpdate(DB::OrganizationRepoPrsInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::OrganizationRepoPrsInfoDao::InsertOrUpdate(info, conn) == 0;
}

bool OrganizationRepoPrRepository::DeleteById(int64_t id, chen::IDB::ptr conn)
{
    if (!conn) {
        return false;
    }
    return DB::OrganizationRepoPrsInfoDao::DeleteById(id, conn) == 0;
}

bool OrganizationRepoPrRepository::Update(DB::OrganizationRepoPrsInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::OrganizationRepoPrsInfoDao::Update(info, conn) == 0;
}

void OrganizationRepoPrRepository::Put(const DB::OrganizationRepoPrsInfo::ptr& info)
{
    g_org_pr_cache.Put(info);
}

void OrganizationRepoPrRepository::Evict(int64_t id)
{
    g_org_pr_cache.Evict(id);
}
