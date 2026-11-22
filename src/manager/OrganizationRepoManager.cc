#include "OrganizationRepoManager.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/DB/Redis.hpp>
#include <Chen/Http/HttpConnection.hpp>
#include <Chen/Http/Uri.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/JsonUtil.hpp>
#include <json/json.h>

#include "../Util/Util.hpp"
#include "CacheUtil.hpp"
#include "../Config/AppConfig.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 300;
static const char* kCachePrefix = "cache:obj:org_repo:";

static std::string OrgRepoCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static DB::OrganizationReposInfo::ptr ParseOrgRepoCache(const Json::Value& v)
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

static DB::OrganizationReposInfo::ptr GetOrgRepoCache(int64_t id)
{
    std::string key = OrgRepoCacheKey(id);
    std::string data = RedisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        RedisCacheDel(key);
        return nullptr;
    }
    return ParseOrgRepoCache(v);
}

static void SetOrgRepoCache(const DB::OrganizationReposInfo::ptr& info)
{
    if (!info) {
        return;
    }
    RedisCacheSetex(OrgRepoCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

static void DelOrgRepoCache(int64_t id)
{
    RedisCacheDel(OrgRepoCacheKey(id));
}

DB::OrganizationReposInfo::ptr OrganizationRepoManager::ParseRow(chen::ISQLData::ptr rt)
{
    return DB::OrganizationReposInfoDao::ParseRow(rt);
}

void OrganizationRepoManager::add(DB::OrganizationReposInfo::ptr info)
{
    SetOrgRepoCache(info);
    invalidateCountCache(info->getOrgId());
}

void OrganizationRepoManager::del(int64_t id)
{
    auto info = GetOrgRepoCache(id);
    if (info) {
        invalidateCountCache(info->getOrgId());
    }
    DelOrgRepoCache(id);
}

void OrganizationRepoManager::onTick()
{
    std::vector<DB::OrganizationReposInfo::ptr> repos;
    getAllWithToken(repos);

    for (const auto& repo : repos) {
        if (!repo) {
            continue;
        }
        int64_t id = repo->getId();
        std::string url = repo->getUrl();
        std::string token = repo->getToken();

        SyncFromGitHub(id, url, token);
    }
}

DB::OrganizationReposInfo::ptr OrganizationRepoManager::get(int64_t id)
{
    auto v = GetOrgRepoCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = DB::OrganizationReposInfoDao::Query(id, db);
    if (v) {
        SetOrgRepoCache(v);
    }
    return v;
}

DB::OrganizationReposInfo::ptr OrganizationRepoManager::getByOrgAndName(int64_t org_id, const std::string& name)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = DB::OrganizationReposInfoDao::QueryByOrgIdName(org_id, name, db);
    if (info) {
        SetOrgRepoCache(info);
    }
    return info;
}

int64_t OrganizationRepoManager::listByOrgPages(std::vector<DB::OrganizationReposInfo::ptr>& repos,
    int64_t org_id,
    uint64_t offset,
    uint64_t limit)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    auto qb = DB::OrganizationReposInfoDao::newQuery();
    qb->where("org_id", "=", org_id);
    qb->orderBy("sort_order DESC, id", "DESC");

    int64_t total = 0;
    if (DB::OrganizationReposInfoDao::QueryByBuilderPages(repos, total, qb, (int32_t)offset, (int32_t)limit, db)) {
        ERROR(logger) << "QueryByBuilderPages failed";
        return 0;
    }
    return total;
}

int64_t OrganizationRepoManager::getCountByOrg(int64_t org_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    auto qb = DB::OrganizationReposInfoDao::newQuery();
    qb->where("org_id", "=", org_id);

    std::stringstream ck;
    ck << "org_repo:count:" << org_id;
    return ExecuteCountCached(qb, db, ck.str());
}

void OrganizationRepoManager::getAllWithToken(std::vector<DB::OrganizationReposInfo::ptr>& repos)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = DB::OrganizationReposInfoDao::newQuery();
    qb->where("token", "!=", "");
    if (DB::OrganizationReposInfoDao::QueryByBuilder(repos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
    }
}

void OrganizationRepoManager::invalidateCountCache(int64_t org_id)
{
    {
        std::stringstream key;
        key << "cache:count:org_repo:count:" << org_id;
        chen::RedisUtil::Cmd(GetRedisPoolName(), "del %s", key.str().c_str());
    }
    {
        std::stringstream key;
        key << "cache:count:org_repo:list:" << org_id;
        chen::RedisUtil::Cmd(GetRedisPoolName(), "del %s", key.str().c_str());
    }
}

static std::string ExtractGithubRepo(const std::string& url)
{
    auto uri = chen::Uri::Create(url);
    if (!uri) {
        return "";
    }
    std::string path = uri->getPath();
    if (!path.empty() && path[0] == '/') {
        path = path.substr(1);
    }
    if (path.size() > 4 && path.substr(path.size() - 4) == ".git") {
        path = path.substr(0, path.size() - 4);
    }
    return path;
}

static void MarkSyncFailed(int64_t repo_id)
{
    auto repo = OrganizationRepoMgr::GetInstance()->get(repo_id);
    if (!repo) {
        return;
    }
    auto db = GetDB();
    if (!db) {
        return;
    }
    repo->setSyncStatus("failed");
    repo->setUpdateTime(time(0));
    DB::OrganizationReposInfoDao::Update(repo, db);
}

void OrganizationRepoManager::SyncFromGitHub(int64_t repo_id, const std::string& url, const std::string& token)
{
    std::string repo_path = ExtractGithubRepo(url);
    if (repo_path.empty()) {
        ERROR(logger) << "SyncFromGitHub: invalid repo url=" << url;
        MarkSyncFailed(repo_id);
        return;
    }

    std::string api_url = "https://api.github.com/repos/" + repo_path;
    std::map<std::string, std::string> headers;
    headers["Accept"] = "application/vnd.github.v3+json";
    headers["Accept-Encoding"] = "identity";
    headers["User-Agent"] = "Blog-Server/1.0";
    if (!token.empty()) {
        headers["Authorization"] = "Bearer " + token;
    }

    INFO(logger) << "SyncFromGitHub: calling " << api_url;
    auto result = chen::http::HttpConnection::DoGet(api_url, 15000, headers);
    if (!result || result->result != 0 || !result->response || result->response->getStatus() != chen::http::HttpStatus::OK) {
        ERROR(logger) << "SyncFromGitHub: HTTP request failed"
                      << " result=" << (result ? result->result : -1)
                      << " status=" << (result && result->response ? (int)result->response->getStatus() : 0)
                      << " body=" << (result && result->response ? result->response->getBody() : "");
        MarkSyncFailed(repo_id);
        return;
    }

    Json::Value json;
    Json::Reader reader;
    if (!reader.parse(result->response->getBody(), json)) {
        ERROR(logger) << "SyncFromGitHub: parse JSON failed";
        MarkSyncFailed(repo_id);
        return;
    }

    auto repo = OrganizationRepoMgr::GetInstance()->get(repo_id);
    if (!repo) {
        ERROR(logger) << "SyncFromGitHub: repo " << repo_id << " not found";
        return;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "SyncFromGitHub: GetDB failed";
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
    // 从 API 响应设置 github_full_name（如果尚未设置或与 URL 提取一致）
    if (json.isMember("full_name") && !json["full_name"].isNull()) {
        repo->setGithubFullName(json["full_name"].asString());
    }
    repo->setSyncStatus("success");
    repo->setUpdateTime(time(0));

    if (DB::OrganizationReposInfoDao::Update(repo, db)) {
        ERROR(logger) << "SyncFromGitHub: DB update failed";
        repo->setSyncStatus("failed");
        DB::OrganizationReposInfoDao::Update(repo, db);
        return;
    }

    INFO(logger) << "SyncFromGitHub: success for repo " << repo_id;
}
