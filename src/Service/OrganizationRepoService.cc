#include "OrganizationRepoService.hpp"

#include <map>
#include <memory>

#include <Chen/Http/HttpConnection.hpp>
#include <Chen/Http/Uri.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/JsonUtil.hpp>
#include <Chen/Util/TimeUtil.hpp>
#include <json/json.h>

#include "../Repository/OrganizationRepoRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

namespace {

/// 从仓库 URL 提取 owner/repo
std::string ExtractGithubRepo(const std::string& url)
{
    auto uri = chen::Uri::Create(url);
    if (uri) {
        std::string path = uri->getPath();
        if (!path.empty() && path[0] == '/') {
            path = path.substr(1);
        }
        if (path.size() > 4 && path.substr(path.size() - 4) == ".git") {
            path = path.substr(0, path.size() - 4);
        }
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
    std::string path = url.substr(pos + marker.size());
    if (path.size() > 4 && path.substr(path.size() - 4) == ".git") {
        path = path.substr(0, path.size() - 4);
    }
    return path;
}

void MarkFailed(int64_t repo_id)
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

} // namespace

OrganizationRepoService::Result OrganizationRepoService::Add(const OrganizationRepoInput& input,
    DB::OrganizationReposInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return Result::DbError;
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

bool OrganizationRepoService::Remove(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto info = OrganizationRepoRepository::Get(id, db);
    if (!info) {
        return false;
    }
    if (!OrganizationRepoRepository::DeleteById(id, db)) {
        ERROR(logger) << "OrganizationRepoService::Remove failed, id=" << id;
        return false;
    }
    OrganizationRepoRepository::Evict(id);
    OrganizationRepoRepository::InvalidateOrgCount(info->getOrgId());
    return true;
}

OrganizationRepoService::Result OrganizationRepoService::UpdateToken(int64_t id,
    const std::string& token,
    DB::OrganizationReposInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return Result::DbError;
    }
    auto info = OrganizationRepoRepository::Get(id, db);
    if (!info) {
        return Result::NotFound;
    }
    info->setToken(token);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!OrganizationRepoRepository::Update(info, db)) {
        ERROR(logger) << "OrganizationRepoService::UpdateToken failed, id=" << id;
        return Result::DbError;
    }
    OrganizationRepoRepository::Put(info);
    out = info;
    return Result::Ok;
}

OrganizationRepoService::Result OrganizationRepoService::MarkSyncing(int64_t id, DB::OrganizationReposInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return Result::DbError;
    }
    auto info = OrganizationRepoRepository::Get(id, db);
    if (!info) {
        return Result::NotFound;
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

void OrganizationRepoService::SyncFromGitHub(int64_t repo_id)
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

    std::string repo_path = ExtractGithubRepo(repo->getUrl());
    if (repo_path.empty()) {
        ERROR(logger) << "SyncFromGitHub: invalid repo url=" << repo->getUrl();
        MarkFailed(repo_id);
        return;
    }

    std::string api_url = "https://api.github.com/repos/" + repo_path;
    std::map<std::string, std::string> headers;
    headers["Accept"] = "application/vnd.github.v3+json";
    headers["Accept-Encoding"] = "identity";
    headers["User-Agent"] = "Blog-Server/1.0";
    const std::string& token = repo->getToken();
    if (!token.empty()) {
        headers["Authorization"] = "Bearer " + token;
    }

    INFO(logger) << "SyncFromGitHub: calling " << api_url;
    auto result = chen::http::HttpConnection::DoGet(api_url, 15000, headers);
    if (!result || result->result != 0 || !result->response
        || result->response->getStatus() != chen::http::HttpStatus::OK) {
        ERROR(logger) << "SyncFromGitHub: HTTP request failed"
                      << " result=" << (result ? result->result : -1)
                      << " status=" << (result && result->response ? (int)result->response->getStatus() : 0)
                      << " body=" << (result && result->response ? result->response->getBody() : "");
        MarkFailed(repo_id);
        return;
    }

    Json::Value json;
    Json::Reader reader;
    if (!reader.parse(result->response->getBody(), json)) {
        ERROR(logger) << "SyncFromGitHub: parse JSON failed";
        MarkFailed(repo_id);
        return;
    }

    if (json.isMember("stargazers_count")) {
        repo->setStarsCount(json["stargazers_count"].asInt());
    }
    if (json.isMember("language") && !json["language"].isNull()) {
        repo->setLanguage(json["language"].asString());
    }
    if (json.isMember("description") && !json["description"].isNull()) {
        repo->setDescription(json["description"].asString());
    }
    if (json.isMember("full_name") && !json["full_name"].isNull()) {
        repo->setGithubFullName(json["full_name"].asString());
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
