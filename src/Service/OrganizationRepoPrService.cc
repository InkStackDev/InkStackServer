#include "OrganizationRepoPrService.hpp"

#include <ctime>
#include <map>

#include <Chen/Http/HttpConnection.hpp>
#include <Chen/Http/Uri.hpp>
#include <Chen/IOManager/Worker.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/JsonUtil.hpp>
#include <Chen/Util/TimeUtil.hpp>
#include <json/json.h>
#include <json/reader.h>

#include "../Job/RedisLease.hpp"
#include "../Repository/OrganizationRepoPrRepository.hpp"
#include "../Repository/OrganizationRepoRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

void OrganizationRepoPrService::SyncAll()
{
    std::vector<DB::OrganizationReposInfo::ptr> repos;
    OrganizationRepoRepository::ListWithToken(repos, GetDB());

    constexpr int kMaxPrCount = 15;
    for (const auto& repo : repos) {
        if (!repo) {
            continue;
        }
        SyncFromGitHub(repo->getId(), repo->getUrl(), repo->getToken(), kMaxPrCount);
    }
}

/// 从 GitHub URL 提取 owner/repo
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

/// 从 Link header 中提取 rel="next" 的 URL
static std::string GetNextPageUrl(const std::string& link_header)
{
    // 格式: <https://...>; rel="next", <https://...>; rel="last"
    size_t pos = 0;
    while (pos < link_header.size()) {
        auto start = link_header.find('<', pos);
        if (start == std::string::npos) {
            break;
        }
        auto end = link_header.find('>', start);
        if (end == std::string::npos) {
            break;
        }
        std::string url = link_header.substr(start + 1, end - start - 1);
        auto relPos = link_header.find("rel=\"", end);
        if (relPos != std::string::npos) {
            auto quoteEnd = link_header.find('"', relPos + 5);
            std::string rel = link_header.substr(relPos + 5, quoteEnd - relPos - 5);
            if (rel == "next") {
                return url;
            }
        }
        pos = end + 1;
    }
    return "";
}

struct SyncPrItem {
    int number;
    std::string title;
    std::string description;
    std::string state;
    std::string author;
    std::string head_branch;
    std::string base_branch;
    std::string commit_sha;
    bool merged;
    int64_t created_at;
    int64_t closed_at;
    int64_t merged_at;
    std::string review_status;
    std::string reviewers;
};

/// 解析 GitHub reviews 数组，计算整体审查状态与 reviewers JSON
static void ParseReviews(const Json::Value& arr, std::string& review_status, std::string& reviewers_json)
{
    bool has_approved = false;
    bool has_changes = false;
    Json::Value reviewerArr(Json::arrayValue);
    for (auto& rv : arr) {
        std::string state = rv["state"].asString();
        if (state == "APPROVED") {
            has_approved = true;
        } else if (state == "CHANGES_REQUESTED") {
            has_changes = true;
        }
        Json::Value reviewer;
        reviewer["reviewer"] = rv["user"]["login"].asString();
        reviewer["status"] = state;
        reviewerArr.append(reviewer);
    }
    if (has_approved) {
        review_status = "approved";
    } else if (has_changes) {
        review_status = "changes_requested";
    } else {
        review_status = "pending";
    }
    reviewers_json = chen::JsonUtil::ToString(reviewerArr);
}

/// 发起 GitHub GET 请求，仅对连接失败/超时重试，返回原始响应（含非 2xx 状态码）
static chen::http::HttpResult::ptr GithubGetRaw(const std::string& url,
    const std::map<std::string, std::string>& headers,
    int max_retries)
{
    chen::http::HttpResult::ptr result;
    for (int retry = 0; retry <= max_retries; ++retry) {
        result = chen::http::HttpConnection::DoGet(url, 30000, headers);
        bool transient = (!result || result->result == (int)chen::http::HttpResult::Error::CONNECT_FAIL
            || result->result == (int)chen::http::HttpResult::Error::TIMEOUT);
        if (!transient) {
            return result;
        }
        if (retry < max_retries) {
            WARN(logger) << "githubGetRaw: retry " << (retry + 1) << "/" << max_retries << " for " << url;
        }
    }
    return result;
}

/// 判断 GitHub 响应是否因限流被拒绝
static bool IsGithubRateLimited(const chen::http::HttpResponse::ptr& response)
{
    if (!response) {
        return false;
    }
    auto status = response->getStatus();
    if (status == chen::http::HttpStatus::TOO_MANY_REQUESTS) {
        return true;
    }
    if (status == chen::http::HttpStatus::FORBIDDEN) {
        if (response->getHeader("x-ratelimit-remaining") == "0") {
            return true;
        }
        if (!response->getHeader("retry-after").empty()) {
            return true;
        }
    }
    return false;
}

static int SyncPage(const Json::Value& arr,
    int64_t repo_id,
    const std::string& repo_path,
    const std::map<std::string, std::string>& headers,
    chen::IDB::ptr db)
{
    // 先解析列表数据
    std::vector<SyncPrItem> items;
    for (auto& item : arr) {
        SyncPrItem si;
        si.number = item["number"].asInt();
        si.title = item["title"].asString();
        si.description = item["body"].asString();
        si.state = item["state"].asString();
        si.author = item["user"]["login"].asString();
        si.head_branch = item["head"]["ref"].asString();
        si.base_branch = item["base"]["ref"].asString();
        si.commit_sha = item["head"]["sha"].asString();
        si.merged = item["merged"].asBool();
        si.created_at = 0;
        si.closed_at = 0;
        si.review_status = "pending";
        si.reviewers = "[]";
        si.merged_at = 0;
        if (item.isMember("created_at") && !item["created_at"].isNull()) {
            si.created_at = chen::TimeUtil::Str2Time(item["created_at"].asCString());
        }
        if (item.isMember("closed_at") && !item["closed_at"].isNull()) {
            si.closed_at = chen::TimeUtil::Str2Time(item["closed_at"].asCString());
        }
        if (item.isMember("merged_at") && !item["merged_at"].isNull()) {
            si.merged_at = chen::TimeUtil::Str2Time(item["merged_at"].asCString());
        }
        items.push_back(si);
    }

    // 并行拉详情 + 审查数据
    std::map<int, int> additions, deletions, changed_files;
    std::map<int, std::string> review_statuses, reviewers_json;
    {
        auto wg = chen::WorkerGroup::Create(5);
        for (auto& si : items) {
            int num = si.number;
            wg->schedule([repo_path, &headers, num, &additions, &deletions, &changed_files, &review_statuses,
                             &reviewers_json]() {
                // 1. 拉详情补全 additions/deletions/changed_files
                {
                    std::string detail_url =
                        "https://api.github.com/repos/" + repo_path + "/pulls/" + std::to_string(num);
                    chen::http::HttpResult::ptr detail;
                    for (int retry = 0; retry <= 3; ++retry) {
                        detail = chen::http::HttpConnection::DoGet(detail_url, 15000, headers);
                        if (detail && detail->result == 0 && detail->response
                            && detail->response->getStatus() == chen::http::HttpStatus::OK) {
                            break;
                        }
                    }
                    if (detail && detail->result == 0 && detail->response
                        && detail->response->getStatus() == chen::http::HttpStatus::OK) {
                        Json::Value detailJson;
                        Json::Reader reader;
                        if (reader.parse(detail->response->getBody(), detailJson)) {
                            if (detailJson.isMember("additions")) {
                                additions[num] = detailJson["additions"].asInt();
                            }
                            if (detailJson.isMember("deletions")) {
                                deletions[num] = detailJson["deletions"].asInt();
                            }
                            if (detailJson.isMember("changed_files")) {
                                changed_files[num] = detailJson["changed_files"].asInt();
                            }
                        }
                    }
                }
                // 2. 拉审查数据
                {
                    std::string reviews_url =
                        "https://api.github.com/repos/" + repo_path + "/pulls/" + std::to_string(num) + "/reviews";
                    chen::http::HttpResult::ptr reviews;
                    for (int retry = 0; retry <= 3; ++retry) {
                        reviews = chen::http::HttpConnection::DoGet(reviews_url, 15000, headers);
                        if (reviews && reviews->result == 0 && reviews->response
                            && reviews->response->getStatus() == chen::http::HttpStatus::OK) {
                            break;
                        }
                    }
                    if (reviews && reviews->result == 0 && reviews->response
                        && reviews->response->getStatus() == chen::http::HttpStatus::OK) {
                        Json::Value arr;
                        Json::Reader reader;
                        if (reader.parse(reviews->response->getBody(), arr) && arr.isArray()) {
                            std::string status;
                            std::string reviewers;
                            ParseReviews(arr, status, reviewers);
                            review_statuses[num] = status;
                            reviewers_json[num] = reviewers;
                        }
                    }
                }
            });
        }
        wg->waitAll();
    }

    // 写入 DB
    int synced = 0;
    for (auto& si : items) {
        auto existing = OrganizationRepoPrRepository::GetByRepoAndPrId(repo_id, si.number, db);
        DB::OrganizationRepoPrsInfo::ptr info;
        if (existing) {
            info = existing;
        } else {
            info.reset(new DB::OrganizationRepoPrsInfo);
            info->setRepoId(repo_id);
            info->setGithubPrId(si.number);
        }
        if (si.created_at) {
            info->setCreateTime(si.created_at);
        }

        info->setTitle(si.title);
        info->setDescription(si.description);
        if (si.merged || si.merged_at > 0) {
            info->setState("merged");
        } else {
            info->setState(si.state);
        }
        info->setAuthor(si.author);
        info->setHeadBranch(si.head_branch);
        info->setBaseBranch(si.base_branch);
        info->setCommitSha(si.commit_sha);

        auto it_a = additions.find(si.number);
        if (it_a != additions.end()) {
            info->setAdditions(it_a->second);
        }
        auto it_d = deletions.find(si.number);
        if (it_d != deletions.end()) {
            info->setDeletions(it_d->second);
        }
        auto it_c = changed_files.find(si.number);
        if (it_c != changed_files.end()) {
            info->setChangedFiles(it_c->second);
        }

        {
            auto it = review_statuses.find(si.number);
            info->setReviewStatus(it != review_statuses.end() ? it->second : "pending");
        }
        {
            auto it = reviewers_json.find(si.number);
            info->setReviewers(it != reviewers_json.end() ? it->second : "[]");
        }
        info->setClosedAt(si.closed_at);
        info->setMergedAt(si.merged_at);
        info->setUpdateTime(time(0));

        if (existing) {
            if (!OrganizationRepoPrRepository::Update(info, db)) {
                ERROR(logger) << "SyncPrFromGitHub: Update failed for PR #" << info->getGithubPrId()
                              << ", errstr=" << db->getErrStr() << ", errno=" << db->getErrno();
                continue;
            }
            OrganizationRepoPrRepository::Put(info);
        } else {
            if (!OrganizationRepoPrRepository::Insert(info, db)) {
                ERROR(logger) << "SyncPrFromGitHub: Insert failed for PR #" << info->getGithubPrId()
                              << ", errstr=" << db->getErrStr() << ", errno=" << db->getErrno();
                continue;
            }
            OrganizationRepoPrRepository::Put(info);
        }
        ++synced;
    }
    return synced;
}

void OrganizationRepoPrService::SyncFromGitHub(int64_t repo_id,
    const std::string& repo_url,
    const std::string& token,
    int max_prs)
{
    std::string repo_path = ExtractGithubRepo(repo_url);
    if (repo_path.empty()) {
        ERROR(logger) << "SyncPrFromGitHub: invalid repo url=" << repo_url;
        return;
    }

    std::map<std::string, std::string> headers;
    headers["Accept"] = "application/vnd.github.v3+json";
    headers["Accept-Encoding"] = "identity";
    headers["User-Agent"] = "Blog-Server/1.0";
    if (!token.empty()) {
        headers["Authorization"] = "Bearer " + token;
    }

    {
        auto repo = OrganizationRepoRepository::Get(repo_id, GetDB());
        if (repo) {
            auto db2 = GetDB();
            if (db2) {
                repo->setPrSyncStatus("syncing");
                repo->setGithubFullName(repo_path);
                if (OrganizationRepoRepository::Update(repo, db2)) {
                    OrganizationRepoRepository::Put(repo);
                }
            }
        }
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "SyncPrFromGitHub: GetDB failed";
        return;
    }

    const int kMaxPages = 10;
    int page_no = 0;
    int total_synced = 0;
    int per_page = max_prs > 0 ? max_prs : 50;
    std::string api_url =
        "https://api.github.com/repos/" + repo_path + "/pulls?state=all&per_page=" + std::to_string(per_page);

    auto doGetWithRetry = [&](const std::string& url, int max_retries) -> chen::http::HttpResult::ptr {
        for (int retry = 0; retry <= max_retries; ++retry) {
            auto r = chen::http::HttpConnection::DoGet(url, 30000, headers);
            if (r && r->result == 0 && r->response && r->response->getStatus() == chen::http::HttpStatus::OK) {
                return r;
            }
            if (retry < max_retries) {
                WARN(logger) << "SyncPrFromGitHub: retry " << (retry + 1) << "/" << max_retries << " for " << url;
            }
        }
        return nullptr;
    };

    while (!api_url.empty() && page_no < kMaxPages) {
        ++page_no;
        INFO(logger) << "SyncPrFromGitHub: page " << page_no << " " << api_url;

        auto result = doGetWithRetry(api_url, 3);
        if (!result) {
            ERROR(logger) << "SyncPrFromGitHub: HTTP request failed on page " << page_no;
            break;
        }

        Json::Value arr;
        Json::Reader reader;
        if (!reader.parse(result->response->getBody(), arr) || !arr.isArray()) {
            ERROR(logger) << "SyncPrFromGitHub: parse JSON failed on page " << page_no;
            break;
        }

        total_synced += SyncPage(arr, repo_id, repo_path, headers, db);

        // 限制条数时只取第一页，不再翻页
        if (max_prs > 0) {
            break;
        }

        // 检查下一页
        std::string link = result->response->getHeader("link");
        if (link.empty()) {
            break;
        }
        api_url = GetNextPageUrl(link);
    }

    // 更新仓库的 PR 同步状态
    {
        auto repo = OrganizationRepoRepository::Get(repo_id, GetDB());
        if (repo) {
            auto repoDb = GetDB();
            if (repoDb) {
                repo->setPrSyncStatus(total_synced >= 0 ? "success" : "failed");
                repo->setPrSyncedAt(time(0));
                if (OrganizationRepoRepository::Update(repo, repoDb)) {
                    OrganizationRepoRepository::Put(repo);
                }
            }
        }
    }

    INFO(logger) << "SyncPrFromGitHub: synced " << total_synced << " PRs across " << page_no << " pages for repo "
                 << repo_id;
}

OrganizationRepoPrService::SyncStatus OrganizationRepoPrService::SyncPrFromGitHub(int64_t pr_id,
    DB::OrganizationRepoPrsInfo::ptr& out)
{
    auto pr = OrganizationRepoPrRepository::Get(pr_id, GetDB());
    if (!pr) {
        return SyncStatus::PR_NOT_FOUND;
    }

    auto repo = OrganizationRepoRepository::Get(pr->getRepoId(), GetDB());
    if (!repo) {
        return SyncStatus::REPO_NOT_FOUND;
    }

    std::string token = repo->getToken();
    if (token.empty()) {
        return SyncStatus::NO_TOKEN;
    }

    std::string repo_path = ExtractGithubRepo(repo->getUrl());
    if (repo_path.empty()) {
        ERROR(logger) << "SyncPrFromGitHub: invalid repo url=" << repo->getUrl();
        return SyncStatus::HTTP_ERROR;
    }

    int32_t number = pr->getGithubPrId();
    if (number <= 0) {
        ERROR(logger) << "SyncPrFromGitHub: invalid github_pr_id for pr_id=" << pr_id;
        return SyncStatus::HTTP_ERROR;
    }

    // 同一 PR 加锁，重复触发直接返回 BUSY
    RedisLease lease("sync:pr:" + std::to_string(pr_id), 300);
    if (!lease.TryAcquire()) {
        return SyncStatus::BUSY;
    }

    std::map<std::string, std::string> headers;
    headers["Accept"] = "application/vnd.github.v3+json";
    headers["Accept-Encoding"] = "identity";
    headers["User-Agent"] = "Blog-Server/1.0";
    headers["Authorization"] = "Bearer " + token;

    // 1. 拉取 PR 详情
    std::string detail_url = "https://api.github.com/repos/" + repo_path + "/pulls/" + std::to_string(number);
    auto detail = GithubGetRaw(detail_url, headers, 3);
    if (!detail || detail->result != 0 || !detail->response) {
        ERROR(logger) << "SyncPrFromGitHub: detail request failed pr_id=" << pr_id << " url=" << detail_url;
        return SyncStatus::HTTP_ERROR;
    }
    auto detail_status = detail->response->getStatus();
    if (detail_status == chen::http::HttpStatus::NOT_FOUND) {
        return SyncStatus::GITHUB_NOT_FOUND;
    }
    if (IsGithubRateLimited(detail->response)) {
        return SyncStatus::RATE_LIMITED;
    }
    if (detail_status != chen::http::HttpStatus::OK) {
        ERROR(logger) << "SyncPrFromGitHub: detail unexpected status=" << (int)detail_status << " pr_id=" << pr_id;
        return SyncStatus::HTTP_ERROR;
    }

    Json::Value detail_json;
    Json::Reader detail_reader;
    if (!detail_reader.parse(detail->response->getBody(), detail_json) || !detail_json.isObject()) {
        ERROR(logger) << "SyncPrFromGitHub: parse detail failed pr_id=" << pr_id;
        return SyncStatus::HTTP_ERROR;
    }

    // 2. 回写主字段
    int64_t created_at = 0;
    if (detail_json.isMember("created_at") && !detail_json["created_at"].isNull()) {
        created_at = chen::TimeUtil::Str2Time(detail_json["created_at"].asCString());
    }
    if (created_at) {
        pr->setCreateTime(created_at);
    }
    pr->setGithubPrId(detail_json["number"].asInt());
    pr->setTitle(detail_json["title"].asString());
    pr->setDescription(detail_json["body"].asString());
    pr->setAuthor(detail_json["user"]["login"].asString());
    pr->setHeadBranch(detail_json["head"]["ref"].asString());
    pr->setBaseBranch(detail_json["base"]["ref"].asString());
    pr->setCommitSha(detail_json["head"]["sha"].asString());
    pr->setChangedFiles(detail_json["changed_files"].asInt());
    pr->setAdditions(detail_json["additions"].asInt());
    pr->setDeletions(detail_json["deletions"].asInt());

    bool merged = detail_json["merged"].asBool();
    int64_t merged_at = 0;
    int64_t closed_at = 0;
    if (detail_json.isMember("merged_at") && !detail_json["merged_at"].isNull()) {
        merged_at = chen::TimeUtil::Str2Time(detail_json["merged_at"].asCString());
    }
    if (detail_json.isMember("closed_at") && !detail_json["closed_at"].isNull()) {
        closed_at = chen::TimeUtil::Str2Time(detail_json["closed_at"].asCString());
    }
    if (merged || merged_at > 0) {
        pr->setState("merged");
    } else {
        pr->setState(detail_json["state"].asString());
    }
    pr->setClosedAt(closed_at);
    pr->setMergedAt(merged_at);

    // 3. 拉取 reviews；失败时保留原有评审字段，整体仍视为成功
    std::string reviews_url = detail_url + "/reviews";
    auto reviews = GithubGetRaw(reviews_url, headers, 3);
    if (reviews && reviews->result == 0 && reviews->response) {
        auto reviews_status = reviews->response->getStatus();
        if (reviews_status == chen::http::HttpStatus::OK) {
            Json::Value arr;
            Json::Reader reviews_reader;
            if (reviews_reader.parse(reviews->response->getBody(), arr) && arr.isArray()) {
                std::string review_status;
                std::string reviewers_json;
                ParseReviews(arr, review_status, reviewers_json);
                pr->setReviewStatus(review_status);
                pr->setReviewers(reviewers_json);
            }
        } else if (IsGithubRateLimited(reviews->response)) {
            WARN(logger) << "SyncPrFromGitHub: reviews rate limited pr_id=" << pr_id;
        }
    }

    pr->setUpdateTime(time(0));

    // 4. 写回 DB
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "SyncPrFromGitHub: GetDB failed pr_id=" << pr_id;
        return SyncStatus::DB_ERROR;
    }
    if (!OrganizationRepoPrRepository::Update(pr, db)) {
        ERROR(logger) << "SyncPrFromGitHub: Update failed for PR #" << pr->getGithubPrId()
                      << ", errstr=" << db->getErrStr() << ", errno=" << db->getErrno();
        return SyncStatus::DB_ERROR;
    }
    OrganizationRepoPrRepository::Put(pr);
    out = pr;

    INFO(logger) << "SyncPrFromGitHub: synced pr_id=" << pr_id << " PR #" << pr->getGithubPrId()
                 << " state=" << pr->getState();
    return SyncStatus::OK;
}

DB::OrganizationReposInfo::ptr OrganizationRepoPrService::FindRepoByOwnerAndName(const std::string& owner,
    const std::string& name)
{
    if (owner.empty() || name.empty()) {
        return nullptr;
    }
    std::string full_name = owner + "/" + name;

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "findRepoByOwnerAndName: GetDB failed for " << full_name;
        return nullptr;
    }

    int64_t repoId = OrganizationRepoRepository::FindIdByGithubFullName(full_name, db);
    if (!repoId) {
        DEBUG(logger) << "findRepoByOwnerAndName: no repo found for " << full_name;
        return nullptr;
    }
    return OrganizationRepoRepository::Get(repoId, db);
}
