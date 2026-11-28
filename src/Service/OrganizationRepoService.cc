#include "OrganizationRepoService.hpp"

#include <memory>

#include <Chen/Http/Uri.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Access/AccessPolicy.hpp"
#include "../Access/AccessScopeResolver.hpp"
#include "../Client/GitHubClient.hpp"
#include "../Domain/OrganizationTypes.hpp"
#include "../Repository/OrganizationRepoRepository.hpp"
#include "../Repository/OrganizationRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

static std::string NormalizeRepoPath(std::string path)
{
    while (!path.empty() && path.back() == '/') {
        path.pop_back();
    }
    if (path.size() > 4 && path.substr(path.size() - 4) == ".git") {
        path = path.substr(0, path.size() - 4);
    }
    while (!path.empty() && path.back() == '/') {
        path.pop_back();
    }
    return path;
}

/// 从仓库 URL 提取 owner/repo
static std::string ExtractGithubRepo(const std::string& url)
{
    auto uri = chen::Uri::Create(url);
    if (uri) {
        std::string path = uri->getPath();
        if (!path.empty() && path[0] == '/') {
            path = path.substr(1);
        }
        path = NormalizeRepoPath(path);
        if (!path.empty()) {
            return path;
        }
    }

    // 兼容 git@github.com:owner/repo.git 这类 SSH 地址
    const std::string marker = "github.com:";
    auto pos = url.find(marker);
    if (pos == std::string::npos) {
        return "";
    }
    return NormalizeRepoPath(url.substr(pos + marker.size()));
}

static bool ValidRepoName(const std::string& name)
{
    return !name.empty() && name.size() <= 128;
}

static bool ValidRepoUrl(const std::string& url)
{
    return !url.empty() && url.size() <= 512 && url.find("github.com") != std::string::npos
        && !ExtractGithubRepo(url).empty();
}

static bool ValidRepoLanguage(const std::string& language)
{
    return language.size() <= 64;
}

static bool ValidRepoDescription(const std::string& description)
{
    return description.size() <= 1024;
}

static void MarkFailed(int64_t repo_id)
{
    auto db = GetDB();
    if (!db) {
        return;
    }
    auto repo = OrganizationRepoRepository::Get(repo_id, db);
    if (!repo) {
        return;
    }
    repo->setSyncStatus("failed");
    repo->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (OrganizationRepoRepository::Update(repo, db)) {
        OrganizationRepoRepository::Put(repo);
    }
}

OrganizationRepoService::Result OrganizationRepoService::Add(const Viewer& viewer,
    const OrganizationRepoInput& input,
    DB::OrganizationReposInfo::ptr& out)
{
    out.reset();
    if (!viewer.valid()) {
        return Result::AccessDenied;
    }
    if (!ValidRepoName(input.name) || !ValidRepoUrl(input.url) || !ValidRepoLanguage(input.language)
        || !ValidRepoDescription(input.description)) {
        return Result::InvalidParam;
    }
    auto db = GetDB();
    if (!db) {
        return Result::DbError;
    }
    OrgScope scope;
    if (!AccessScopeResolver::ResolveOrgs(scope, viewer, db)) {
        return Result::DbError;
    }
    if (!viewer.is_platform_admin && !scope.allows(input.org_id, OrgRole::MEMBER)) {
        return Result::NotMember;
    }
    if (!AccessPolicy::CanManageOrgRepos(viewer, scope, input.org_id)) {
        return Result::AccessDenied;
    }
    auto org = OrganizationRepository::Get(input.org_id, db);
    if (!org) {
        return Result::OrgNotFound;
    }
    auto existing = OrganizationRepoRepository::GetByOrgAndName(input.org_id, input.name, db);
    if (existing) {
        return Result::NameExists;
    }

    auto info = std::make_shared<DB::OrganizationReposInfo>();
    info->setOrgId(input.org_id);
    info->setName(input.name);
    info->setUrl(input.url);
    info->setLanguage(input.language);
    info->setDescription(input.description);
    info->setToken(input.token);
    info->setStarsCount(0);
    info->setSortOrder(0);
    info->setSyncStatus("idle");
    info->setPrSyncStatus("idle");
    info->setPrSyncedAt(0);
    info->setGithubFullName(ExtractGithubRepo(input.url));
    int64_t now = chen::TimeUtil::GetCurrentSec();
    info->setCreateTime(now);
    info->setUpdateTime(now);

    if (!OrganizationRepoRepository::Insert(info, db)) {
        ERROR(logger) << "OrganizationRepoService::Add failed, name=" << input.name;
        return Result::DbError;
    }
    OrganizationRepoRepository::Put(info);
    OrganizationRepoRepository::InvalidateOrgCount(input.org_id);
    out = info;
    return Result::Ok;
}

OrganizationRepoService::Result OrganizationRepoService::Update(const Viewer& viewer,
    const OrganizationRepoUpdateInput& input,
    DB::OrganizationReposInfo::ptr& out)
{
    out.reset();
    if (!viewer.valid()) {
        return Result::AccessDenied;
    }
    if (input.id <= 0 || !ValidRepoUrl(input.url)) {
        return Result::InvalidParam;
    }
    auto db = GetDB();
    if (!db) {
        return Result::DbError;
    }
    auto info = OrganizationRepoRepository::Get(input.id, db);
    if (!info) {
        return Result::NotFound;
    }
    OrgScope scope;
    if (!AccessScopeResolver::ResolveOrgs(scope, viewer, db)) {
        return Result::DbError;
    }
    if (!viewer.is_platform_admin && !scope.allows(info->getOrgId(), OrgRole::MEMBER)) {
        return Result::NotMember;
    }
    if (!AccessPolicy::CanManageOrgRepos(viewer, scope, info->getOrgId())) {
        return Result::AccessDenied;
    }

    if (input.url != info->getUrl()) {
        info->setUrl(input.url);
        info->setGithubFullName(ExtractGithubRepo(input.url));
        info->setSyncStatus("idle");
        info->setPrSyncStatus("idle");
        info->setPrSyncedAt(0);
    }
    if (input.token) {
        info->setToken(*input.token);
    }
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());

    if (!OrganizationRepoRepository::Update(info, db)) {
        ERROR(logger) << "OrganizationRepoService::Update failed, id=" << input.id;
        return Result::DbError;
    }
    OrganizationRepoRepository::Put(info);
    out = info;
    return Result::Ok;
}

OrganizationRepoService::Result OrganizationRepoService::Remove(const Viewer& viewer, int64_t id, int64_t org_id)
{
    if (!viewer.valid()) {
        return Result::AccessDenied;
    }
    auto db = GetDB();
    if (!db) {
        return Result::DbError;
    }
    auto info = OrganizationRepoRepository::Get(id, db);
    if (!info || (org_id > 0 && info->getOrgId() != org_id)) {
        return Result::NotFound;
    }
    OrgScope scope;
    if (!AccessScopeResolver::ResolveOrgs(scope, viewer, db)) {
        return Result::DbError;
    }
    if (!viewer.is_platform_admin && !scope.allows(info->getOrgId(), OrgRole::MEMBER)) {
        return Result::NotMember;
    }
    if (!AccessPolicy::CanManageOrgRepos(viewer, scope, info->getOrgId())) {
        return Result::AccessDenied;
    }
    if (!OrganizationRepoRepository::DeleteById(id, db)) {
        ERROR(logger) << "OrganizationRepoService::Remove failed, id=" << id;
        return Result::DbError;
    }
    OrganizationRepoRepository::Evict(id);
    OrganizationRepoRepository::InvalidateOrgCount(info->getOrgId());
    return Result::Ok;
}

OrganizationRepoService::Result OrganizationRepoService::MarkSyncing(const Viewer& viewer,
    int64_t id,
    DB::OrganizationReposInfo::ptr& out)
{
    out.reset();
    if (!viewer.valid()) {
        return Result::AccessDenied;
    }
    auto db = GetDB();
    if (!db) {
        return Result::DbError;
    }
    auto info = OrganizationRepoRepository::Get(id, db);
    if (!info) {
        return Result::NotFound;
    }
    OrgScope scope;
    if (!AccessScopeResolver::ResolveOrgs(scope, viewer, db)) {
        return Result::DbError;
    }
    if (!viewer.is_platform_admin && !scope.allows(info->getOrgId(), OrgRole::MEMBER)) {
        return Result::NotMember;
    }
    if (!AccessPolicy::CanManageOrgRepos(viewer, scope, info->getOrgId())) {
        return Result::AccessDenied;
    }
    info->setSyncStatus("syncing");
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!OrganizationRepoRepository::Update(info, db)) {
        ERROR(logger) << "OrganizationRepoService::MarkSyncing failed, id=" << id;
        return Result::DbError;
    }
    OrganizationRepoRepository::Put(info);
    out = info;
    return Result::Ok;
}

OrganizationRepoService::Result OrganizationRepoService::List(const Viewer& viewer,
    int64_t org_id,
    uint64_t offset,
    uint64_t limit,
    std::vector<DB::OrganizationReposInfo::ptr>& out,
    int64_t& total)
{
    out.clear();
    total = 0;
    if (!viewer.valid()) {
        return Result::AccessDenied;
    }
    auto db = GetDB();
    if (!db) {
        return Result::DbError;
    }
    OrgScope scope;
    if (!AccessScopeResolver::ResolveOrgs(scope, viewer, db)) {
        return Result::DbError;
    }
    if (!viewer.is_platform_admin && !scope.allows(org_id, OrgRole::MEMBER)) {
        return Result::NotMember;
    }
    auto org = OrganizationRepository::Get(org_id, db);
    if (!org) {
        return Result::OrgNotFound;
    }
    total = OrganizationRepoRepository::ListByOrgPages(out, org_id, offset, limit, db);
    return Result::Ok;
}

OrganizationRepoService::Result OrganizationRepoService::CountByOrg(int64_t org_id, int64_t& total)
{
    total = 0;
    auto db = GetDB();
    if (!db) {
        return Result::DbError;
    }
    auto org = OrganizationRepository::Get(org_id, db);
    if (!org) {
        return Result::OrgNotFound;
    }
    total = OrganizationRepoRepository::CountByOrg(org_id, db);
    return Result::Ok;
}

OrganizationRepoService::Result OrganizationRepoService::CheckOrgAccess(const Viewer& viewer,
    int64_t org_id,
    bool manage)
{
    if (!viewer.valid()) {
        return Result::AccessDenied;
    }
    auto db = GetDB();
    if (!db) {
        return Result::DbError;
    }
    OrgScope scope;
    if (!AccessScopeResolver::ResolveOrgs(scope, viewer, db)) {
        return Result::DbError;
    }
    if (!viewer.is_platform_admin && !scope.allows(org_id, OrgRole::MEMBER)) {
        return Result::NotMember;
    }
    if (manage && !AccessPolicy::CanManageOrgRepos(viewer, scope, org_id)) {
        return Result::AccessDenied;
    }
    auto org = OrganizationRepository::Get(org_id, db);
    if (!org) {
        return Result::OrgNotFound;
    }
    return Result::Ok;
}

OrganizationRepoService::Result OrganizationRepoService::CheckRepoAccess(const Viewer& viewer,
    int64_t repo_id,
    bool manage,
    DB::OrganizationReposInfo::ptr& out)
{
    out.reset();
    if (!viewer.valid()) {
        return Result::AccessDenied;
    }
    auto db = GetDB();
    if (!db) {
        return Result::DbError;
    }
    auto repo = OrganizationRepoRepository::Get(repo_id, db);
    if (!repo) {
        return Result::NotFound;
    }
    OrgScope scope;
    if (!AccessScopeResolver::ResolveOrgs(scope, viewer, db)) {
        return Result::DbError;
    }
    if (!viewer.is_platform_admin && !scope.allows(repo->getOrgId(), OrgRole::MEMBER)) {
        return Result::NotMember;
    }
    if (manage && !AccessPolicy::CanManageOrgRepos(viewer, scope, repo->getOrgId())) {
        return Result::AccessDenied;
    }
    out = repo;
    return Result::Ok;
}

void OrganizationRepoService::SyncFromGitHub(int64_t repo_id)
{
    std::string token;
    std::string repo_path;
    {
        auto db = GetDB();
        if (!db) {
            ERROR(logger) << "SyncFromGitHub: GetDB failed";
            return;
        }
        auto repo = OrganizationRepoRepository::Get(repo_id, db);
        if (!repo) {
            ERROR(logger) << "SyncFromGitHub: repo " << repo_id << " not found";
            return;
        }
        token = repo->getToken();
        repo_path = ExtractGithubRepo(repo->getUrl());
        if (repo_path.empty()) {
            ERROR(logger) << "SyncFromGitHub: invalid repo url=" << repo->getUrl();
            MarkFailed(repo_id);
            return;
        }
    }

    INFO(logger) << "SyncFromGitHub: calling github repo=" << repo_path;
    auto gh = GitHubClient::GetRepo(token, repo_path);
    if (!gh.ok()) {
        ERROR(logger) << "SyncFromGitHub: github request failed repo_id=" << repo_id << " error=" << (int)gh.error
                      << " detail=" << gh.detail;
        MarkFailed(repo_id);
        return;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "SyncFromGitHub: GetDB failed after request";
        return;
    }
    auto repo = OrganizationRepoRepository::Get(repo_id, db);
    if (!repo) {
        ERROR(logger) << "SyncFromGitHub: repo " << repo_id << " disappeared";
        return;
    }

    repo->setStarsCount(gh.value.stars);
    if (!gh.value.language.empty()) {
        repo->setLanguage(gh.value.language);
    }
    if (!gh.value.description.empty()) {
        repo->setDescription(gh.value.description);
    }
    if (!gh.value.full_name.empty()) {
        repo->setGithubFullName(gh.value.full_name);
    }
    repo->setSyncStatus("success");
    repo->setUpdateTime(chen::TimeUtil::GetCurrentSec());

    if (!OrganizationRepoRepository::Update(repo, db)) {
        ERROR(logger) << "SyncFromGitHub: DB update failed";
        repo->setSyncStatus("failed");
        OrganizationRepoRepository::Update(repo, db);
        return;
    }
    OrganizationRepoRepository::Put(repo);
    INFO(logger) << "SyncFromGitHub: success for repo " << repo_id;
}

void OrganizationRepoService::SyncAll()
{
    std::vector<DB::OrganizationReposInfo::ptr> repos;
    OrganizationRepoRepository::ListWithToken(repos, GetDB());
    for (const auto& repo : repos) {
        if (!repo) {
            continue;
        }
        SyncFromGitHub(repo->getId());
    }
}
