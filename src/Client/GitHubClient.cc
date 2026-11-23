#include "GitHubClient.hpp"

#include "../Config/GitHubConfig.hpp"

#include <Chen/Http/HttpClient.hpp>
#include <Chen/Http/HttpConnection.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/JsonUtil.hpp>
#include <Chen/Util/StringUtil.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include <json/json.h>

#include <map>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

static chen::Logger::ptr logger = LOG_ROOT();

namespace {

std::map<std::string, std::string> BuildHeaders(const std::string& token, const GitHubConf& conf)
{
    std::map<std::string, std::string> headers;
    headers["Accept"] = "application/vnd.github.v3+json";
    headers["Accept-Encoding"] = "identity";
    headers["User-Agent"] = conf.user_agent;
    if (!token.empty()) {
        headers["Authorization"] = "Bearer " + token;
    }
    return headers;
}

bool IsRateLimited(const chen::http::HttpResponse::ptr& response)
{
    if (!response) {
        return false;
    }
    int status = (int)response->getStatus();
    if (status == 429) {
        return true;
    }
    if (status == 403) {
        return response->getHeader("x-ratelimit-remaining") == "0" || !response->getHeader("retry-after").empty();
    }
    return false;
}

bool DoRequestWithRetry(chen::http::HttpMethod method,
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::string& request_body,
    int64_t timeout_ms,
    ClientError& error,
    std::string& body,
    std::string& link_header)
{
    GitHubConf conf = GetGitHubConf();
    int attempts = conf.max_attempts > 0 ? conf.max_attempts : 1;

    error = ClientError::Unknown;
    body.clear();
    link_header.clear();

    for (int attempt = 0; attempt < attempts; ++attempt) {
        chen::http::HttpResult::ptr result;
        try {
            result = chen::http::HttpClient::request(method, url, (uint64_t)timeout_ms, headers, request_body).get();
        } catch (...) {
            result = nullptr;
        }
        if (!result) {
            result = method == chen::http::HttpMethod::POST
                ? chen::http::HttpConnection::DoPost(url, (uint64_t)timeout_ms, headers, request_body)
                : chen::http::HttpConnection::DoGet(url, (uint64_t)timeout_ms, headers, request_body);
        }
        if (!result) {
            error = ClientError::ConnectFailed;
            if (attempt + 1 < attempts) {
                WARN(logger) << "github request retry " << (attempt + 1) << "/" << attempts << " url=" << url;
                int64_t backoff = conf.backoff_ms * (attempt + 1);
                if (backoff > 5000) {
                    backoff = 5000;
                }
                usleep((useconds_t)(backoff * 1000));
                continue;
            }
            return false;
        }

        if (result->result == (int)chen::http::HttpResult::Error::CONNECT_FAIL
            || result->result == (int)chen::http::HttpResult::Error::TIMEOUT) {
            error = result->result == (int)chen::http::HttpResult::Error::TIMEOUT ? ClientError::Timeout
                                                                                  : ClientError::ConnectFailed;
            if (attempt + 1 < attempts) {
                WARN(logger) << "github request retry " << (attempt + 1) << "/" << attempts << " url=" << url;
                int64_t backoff = conf.backoff_ms * (attempt + 1);
                if (backoff > 5000) {
                    backoff = 5000;
                }
                usleep((useconds_t)(backoff * 1000));
                continue;
            }
            return false;
        }
        if (result->result != 0 || !result->response) {
            error = ClientError::Unknown;
            return false;
        }

        auto response = result->response;
        int status = (int)response->getStatus();
        if (status == 200) {
            body = response->getBody();
            link_header = response->getHeader("link");
            error = ClientError::Ok;
            return true;
        }
        if (status == 401 || status == 403) {
            error = IsRateLimited(response) ? ClientError::RateLimited : ClientError::Unauthorized;
            return false;
        }
        if (status == 404) {
            error = ClientError::NotFound;
            return false;
        }
        if (status == 429) {
            error = ClientError::RateLimited;
            return false;
        }
        if (status >= 500) {
            error = ClientError::ServerError;
            if (attempt + 1 < attempts) {
                WARN(logger) << "github request retry " << (attempt + 1) << "/" << attempts << " status=" << status
                             << " url=" << url;
                int64_t backoff = conf.backoff_ms * (attempt + 1);
                if (backoff > 5000) {
                    backoff = 5000;
                }
                usleep((useconds_t)(backoff * 1000));
                continue;
            }
            return false;
        }

        error = ClientError::Unknown;
        return false;
    }

    return false;
}

bool DoGetWithRetry(const std::string& url,
    const std::string& token,
    int64_t timeout_ms,
    ClientError& error,
    std::string& body,
    std::string& link_header)
{
    GitHubConf conf = GetGitHubConf();
    auto headers = BuildHeaders(token, conf);
    return DoRequestWithRetry(chen::http::HttpMethod::GET, url, headers, "", timeout_ms, error, body, link_header);
}

std::string GetNextPageUrl(const std::string& link_header)
{
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
        auto rel_pos = link_header.find("rel=\"", end);
        if (rel_pos != std::string::npos) {
            auto quote_end = link_header.find('"', rel_pos + 5);
            std::string rel = link_header.substr(rel_pos + 5, quote_end - rel_pos - 5);
            if (rel == "next") {
                return url;
            }
        }
        pos = end + 1;
    }
    return "";
}

std::string JsonStr(const Json::Value& value, const char* key)
{
    if (!value.isMember(key) || !value[key].isString()) {
        return "";
    }
    return value[key].asString();
}

int64_t JsonTime(const Json::Value& value, const char* key)
{
    if (!value.isMember(key) || !value[key].isString()) {
        return 0;
    }
    return chen::TimeUtil::Str2Time(value[key].asCString());
}

void ParsePullRequest(const Json::Value& value, GitHubPullRequest& pr)
{
    if (!value.isObject()) {
        return;
    }
    if (value.isMember("number") && value["number"].isInt()) {
        pr.number = value["number"].asInt();
    }
    pr.title = JsonStr(value, "title");
    pr.description = JsonStr(value, "body");
    pr.state = JsonStr(value, "state");
    if (value.isMember("user") && value["user"].isObject()) {
        pr.author = JsonStr(value["user"], "login");
    }
    if (value.isMember("head") && value["head"].isObject()) {
        pr.head_branch = JsonStr(value["head"], "ref");
        pr.commit_sha = JsonStr(value["head"], "sha");
    }
    if (value.isMember("base") && value["base"].isObject()) {
        pr.base_branch = JsonStr(value["base"], "ref");
    }
    if (value.isMember("merged") && value["merged"].isBool()) {
        pr.merged = value["merged"].asBool();
    }
    pr.created_at = JsonTime(value, "created_at");
    pr.closed_at = JsonTime(value, "closed_at");
    pr.merged_at = JsonTime(value, "merged_at");
    if (value.isMember("additions") && value["additions"].isInt()) {
        pr.additions = value["additions"].asInt();
    }
    if (value.isMember("deletions") && value["deletions"].isInt()) {
        pr.deletions = value["deletions"].asInt();
    }
    if (value.isMember("changed_files") && value["changed_files"].isInt()) {
        pr.changed_files = value["changed_files"].asInt();
    }
}

} // namespace

ClientResult<GitHubRepoInfo> GitHubClient::GetRepo(const std::string& token, const std::string& repo_path)
{
    ClientResult<GitHubRepoInfo> result;
    if (repo_path.empty()) {
        result.error = ClientError::MalformedResponse;
        result.detail = "empty repo path";
        return result;
    }

    GitHubConf conf = GetGitHubConf();
    std::string url = conf.api_base + "/repos/" + repo_path;
    std::string body;
    std::string link_header;
    if (!DoGetWithRetry(url, token, conf.timeout_ms, result.error, body, link_header)) {
        result.detail = "request failed";
        return result;
    }

    Json::Value json;
    if (!chen::JsonUtil::FromString(json, body) || !json.isObject()) {
        result.error = ClientError::MalformedResponse;
        result.detail = "invalid json";
        return result;
    }

    if (json.isMember("stargazers_count") && json["stargazers_count"].isInt()) {
        result.value.stars = json["stargazers_count"].asInt();
    }
    if (json.isMember("language") && json["language"].isString()) {
        result.value.language = json["language"].asString();
    }
    if (json.isMember("description") && json["description"].isString()) {
        result.value.description = json["description"].asString();
    }
    if (json.isMember("full_name") && json["full_name"].isString()) {
        result.value.full_name = json["full_name"].asString();
    }

    result.error = ClientError::Ok;
    return result;
}

ClientResult<std::vector<GitHubPullRequest>> GitHubClient::ListPullRequests(const std::string& token,
    const std::string& repo_path,
    int32_t per_page,
    int32_t max_pages)
{
    ClientResult<std::vector<GitHubPullRequest>> result;
    if (repo_path.empty()) {
        result.error = ClientError::MalformedResponse;
        result.detail = "empty repo path";
        return result;
    }

    GitHubConf conf = GetGitHubConf();
    if (per_page <= 0) {
        per_page = conf.per_page > 0 ? conf.per_page : 50;
    }
    if (per_page > 100) {
        per_page = 100;
    }
    if (max_pages <= 0) {
        max_pages = 10;
    }

    std::string url = conf.api_base + "/repos/" + repo_path + "/pulls?state=all&per_page=" + std::to_string(per_page);
    int page = 0;
    while (!url.empty() && page < max_pages) {
        ++page;
        std::string body;
        std::string link_header;
        if (!DoGetWithRetry(url, token, conf.timeout_ms, result.error, body, link_header)) {
            result.detail = "list pull requests failed";
            return result;
        }

        Json::Value arr;
        if (!chen::JsonUtil::FromString(arr, body) || !arr.isArray()) {
            result.error = ClientError::MalformedResponse;
            result.detail = "invalid pull request list";
            return result;
        }
        for (const auto& item : arr) {
            GitHubPullRequest pr;
            ParsePullRequest(item, pr);
            if (pr.number > 0) {
                result.value.push_back(std::move(pr));
            }
        }
        url = GetNextPageUrl(link_header);
    }

    result.error = ClientError::Ok;
    return result;
}

ClientResult<GitHubPullRequest> GitHubClient::GetPullRequest(const std::string& token,
    const std::string& repo_path,
    int32_t number)
{
    ClientResult<GitHubPullRequest> result;
    if (repo_path.empty() || number <= 0) {
        result.error = ClientError::MalformedResponse;
        result.detail = "invalid pull request";
        return result;
    }

    GitHubConf conf = GetGitHubConf();
    std::string url = conf.api_base + "/repos/" + repo_path + "/pulls/" + std::to_string(number);
    std::string body;
    std::string link_header;
    if (!DoGetWithRetry(url, token, conf.detail_timeout_ms, result.error, body, link_header)) {
        result.detail = "get pull request failed";
        return result;
    }

    Json::Value json;
    if (!chen::JsonUtil::FromString(json, body) || !json.isObject()) {
        result.error = ClientError::MalformedResponse;
        result.detail = "invalid pull request json";
        return result;
    }
    ParsePullRequest(json, result.value);
    result.error = ClientError::Ok;
    return result;
}

ClientResult<GitHubReviewSummary> GitHubClient::GetReviewSummary(const std::string& token,
    const std::string& repo_path,
    int32_t number)
{
    ClientResult<GitHubReviewSummary> result;
    if (repo_path.empty() || number <= 0) {
        result.error = ClientError::MalformedResponse;
        result.detail = "invalid pull request";
        return result;
    }

    GitHubConf conf = GetGitHubConf();
    std::string url = conf.api_base + "/repos/" + repo_path + "/pulls/" + std::to_string(number) + "/reviews";
    std::string body;
    std::string link_header;
    if (!DoGetWithRetry(url, token, conf.detail_timeout_ms, result.error, body, link_header)) {
        result.detail = "get reviews failed";
        return result;
    }

    Json::Value arr;
    if (!chen::JsonUtil::FromString(arr, body) || !arr.isArray()) {
        result.error = ClientError::MalformedResponse;
        result.detail = "invalid reviews json";
        return result;
    }

    bool has_approved = false;
    bool has_changes = false;
    Json::Value reviewers(Json::arrayValue);
    for (const auto& item : arr) {
        std::string state = JsonStr(item, "state");
        if (state == "APPROVED") {
            has_approved = true;
        } else if (state == "CHANGES_REQUESTED") {
            has_changes = true;
        }
        Json::Value reviewer;
        if (item.isMember("user") && item["user"].isObject()) {
            reviewer["reviewer"] = JsonStr(item["user"], "login");
        } else {
            reviewer["reviewer"] = "";
        }
        reviewer["status"] = state;
        reviewers.append(reviewer);
    }

    if (has_approved) {
        result.value.status = "approved";
    } else if (has_changes) {
        result.value.status = "changes_requested";
    } else {
        result.value.status = "pending";
    }
    result.value.reviewers = chen::JsonUtil::ToString(reviewers);
    result.error = ClientError::Ok;
    return result;
}

ClientResult<GitHubOAuthToken> GitHubClient::ExchangeOAuthCode(const std::string& client_id,
    const std::string& client_secret,
    const std::string& code,
    const std::string& redirect_uri)
{
    ClientResult<GitHubOAuthToken> result;
    if (client_id.empty() || client_secret.empty() || code.empty()) {
        result.error = ClientError::MalformedResponse;
        result.detail = "missing oauth params";
        return result;
    }

    GitHubConf conf = GetGitHubConf();
    std::map<std::string, std::string> headers;
    headers["Accept"] = "application/json";
    headers["Content-Type"] = "application/x-www-form-urlencoded";
    headers["User-Agent"] = conf.user_agent;

    std::string body = "client_id=" + chen::StringUtil::UrlEncode(client_id, false) + "&client_secret="
        + chen::StringUtil::UrlEncode(client_secret, false) + "&code=" + chen::StringUtil::UrlEncode(code, false)
        + "&redirect_uri=" + chen::StringUtil::UrlEncode(redirect_uri, false);

    std::string response_body;
    std::string link_header;
    std::string url = conf.oauth_base + "/login/oauth/access_token";
    if (!DoRequestWithRetry(chen::http::HttpMethod::POST, url, headers, body, conf.detail_timeout_ms, result.error,
            response_body, link_header)) {
        result.detail = "oauth token request failed";
        return result;
    }

    Json::Value json;
    if (!chen::JsonUtil::FromString(json, response_body) || !json.isObject()) {
        result.error = ClientError::MalformedResponse;
        result.detail = "invalid oauth token json";
        return result;
    }
    if (!json.isMember("access_token") || !json["access_token"].isString()) {
        result.error = ClientError::Unauthorized;
        result.detail = "missing access_token";
        return result;
    }

    result.value.access_token = json["access_token"].asString();
    result.value.token_type = JsonStr(json, "token_type");
    result.value.scope = JsonStr(json, "scope");
    if (result.value.access_token.empty()) {
        result.error = ClientError::Unauthorized;
        result.detail = "empty access_token";
        return result;
    }
    result.error = ClientError::Ok;
    return result;
}

ClientResult<GitHubOAuthUser> GitHubClient::GetOAuthUser(const std::string& access_token)
{
    ClientResult<GitHubOAuthUser> result;
    if (access_token.empty()) {
        result.error = ClientError::Unauthorized;
        result.detail = "empty access_token";
        return result;
    }

    GitHubConf conf = GetGitHubConf();
    std::map<std::string, std::string> headers;
    headers["Accept"] = "application/vnd.github+json";
    headers["User-Agent"] = conf.user_agent;
    headers["Authorization"] = "Bearer " + access_token;

    std::string body;
    std::string link_header;
    std::string url = conf.api_base + "/user";
    if (!DoRequestWithRetry(
            chen::http::HttpMethod::GET, url, headers, "", conf.timeout_ms, result.error, body, link_header)) {
        result.detail = "get oauth user failed";
        return result;
    }

    Json::Value json;
    if (!chen::JsonUtil::FromString(json, body) || !json.isObject()) {
        result.error = ClientError::MalformedResponse;
        result.detail = "invalid oauth user json";
        return result;
    }

    if (json.isMember("id")) {
        const auto& id = json["id"];
        if (id.isInt64()) {
            result.value.open_id = std::to_string(id.asInt64());
        } else if (id.isUInt64()) {
            result.value.open_id = std::to_string(id.asUInt64());
        } else if (id.isInt()) {
            result.value.open_id = std::to_string(id.asInt());
        } else if (id.isUInt()) {
            result.value.open_id = std::to_string(id.asUInt());
        } else if (id.isString()) {
            result.value.open_id = id.asString();
        }
    }
    if (result.value.open_id.empty()) {
        result.error = ClientError::MalformedResponse;
        result.detail = "missing oauth user id";
        return result;
    }

    result.value.login = JsonStr(json, "login");
    result.value.name = JsonStr(json, "name");
    result.value.email = JsonStr(json, "email");
    result.value.avatar = JsonStr(json, "avatar_url");
    result.error = ClientError::Ok;
    return result;
}

ClientResult<std::string> GitHubClient::GetPrimaryEmail(const std::string& access_token)
{
    ClientResult<std::string> result;
    if (access_token.empty()) {
        result.error = ClientError::Unauthorized;
        result.detail = "empty access_token";
        return result;
    }

    GitHubConf conf = GetGitHubConf();
    std::map<std::string, std::string> headers;
    headers["Accept"] = "application/vnd.github+json";
    headers["User-Agent"] = conf.user_agent;
    headers["Authorization"] = "Bearer " + access_token;

    std::string body;
    std::string link_header;
    std::string url = conf.api_base + "/user/emails";
    if (!DoRequestWithRetry(
            chen::http::HttpMethod::GET, url, headers, "", conf.timeout_ms, result.error, body, link_header)) {
        result.detail = "get oauth emails failed";
        return result;
    }

    Json::Value arr;
    if (!chen::JsonUtil::FromString(arr, body) || !arr.isArray()) {
        result.error = ClientError::MalformedResponse;
        result.detail = "invalid oauth emails json";
        return result;
    }

    std::string fallback;
    for (const auto& item : arr) {
        std::string email = JsonStr(item, "email");
        if (email.empty()) {
            continue;
        }
        bool verified = item.isMember("verified") && item["verified"].isBool() && item["verified"].asBool();
        bool primary = item.isMember("primary") && item["primary"].isBool() && item["primary"].asBool();
        if (primary && verified) {
            result.value = email;
            result.error = ClientError::Ok;
            return result;
        }
        if (verified && fallback.empty()) {
            fallback = email;
        }
    }

    result.value = fallback;
    result.error = ClientError::Ok;
    return result;
}
