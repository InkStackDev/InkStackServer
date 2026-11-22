#pragma once

#include "DB/OrganizationReposInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

class OrganizationRepoManager {
public:
    void add(DB::OrganizationReposInfo::ptr info);

    void del(int64_t id);

    void onTick();

    DB::OrganizationReposInfo::ptr get(int64_t id);

    DB::OrganizationReposInfo::ptr getByOrgAndName(int64_t org_id, const std::string& name);

    int64_t listByOrgPages(std::vector<DB::OrganizationReposInfo::ptr>& repos,
        int64_t org_id,
        uint64_t offset,
        uint64_t limit);

    int64_t getCountByOrg(int64_t org_id);

    /// 获取所有含有 token 的仓库信息
    void getAllWithToken(std::vector<DB::OrganizationReposInfo::ptr>& repos);

    /// 从 GitHub API 异步同步仓库数据（stars_count、language、description）
    static void SyncFromGitHub(int64_t repo_id, const std::string& url, const std::string& token);
};

typedef chen::Singleton<OrganizationRepoManager> OrganizationRepoMgr;
