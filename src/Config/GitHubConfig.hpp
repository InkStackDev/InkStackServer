/**
 * @file GitHubConfig.hpp
 * @brief GitHub API 客户端配置
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <Chen/Config/Config.hpp>

#include <cstdint>
#include <sstream>
#include <string>

struct GitHubConf {
    std::string api_base = "https://api.github.com";
    std::string oauth_base = "https://github.com";
    int64_t timeout_ms = 15000;
    int64_t detail_timeout_ms = 15000;
    int32_t max_attempts = 3;
    int64_t backoff_ms = 300;
    std::string user_agent = "InkStackServer/1.0";
    int32_t per_page = 50;

    bool operator==(const GitHubConf& other) const;
};

namespace chen {

template <>
class LexicalCast<std::string, GitHubConf> {
public:
    GitHubConf operator()(const std::string& value)
    {
        GitHubConf conf;
        const YAML::Node node = YAML::Load(value);
        if (!node.IsMap()) {
            return conf;
        }
        if (node["api_base"].IsScalar()) {
            conf.api_base = node["api_base"].Scalar();
        }
        if (node["oauth_base"].IsScalar()) {
            conf.oauth_base = node["oauth_base"].Scalar();
        }
        if (node["timeout_ms"].IsScalar()) {
            conf.timeout_ms = LexicalCast<std::string, int64_t>()(node["timeout_ms"].Scalar());
        }
        if (node["detail_timeout_ms"].IsScalar()) {
            conf.detail_timeout_ms = LexicalCast<std::string, int64_t>()(node["detail_timeout_ms"].Scalar());
        }
        if (node["max_attempts"].IsScalar()) {
            conf.max_attempts = LexicalCast<std::string, int32_t>()(node["max_attempts"].Scalar());
        }
        if (node["backoff_ms"].IsScalar()) {
            conf.backoff_ms = LexicalCast<std::string, int64_t>()(node["backoff_ms"].Scalar());
        }
        if (node["user_agent"].IsScalar()) {
            conf.user_agent = node["user_agent"].Scalar();
        }
        if (node["per_page"].IsScalar()) {
            conf.per_page = LexicalCast<std::string, int32_t>()(node["per_page"].Scalar());
        }
        return conf;
    }
};

template <>
class LexicalCast<GitHubConf, std::string> {
public:
    std::string operator()(const GitHubConf& conf)
    {
        YAML::Node node;
        node["api_base"] = conf.api_base;
        node["oauth_base"] = conf.oauth_base;
        node["timeout_ms"] = conf.timeout_ms;
        node["detail_timeout_ms"] = conf.detail_timeout_ms;
        node["max_attempts"] = conf.max_attempts;
        node["backoff_ms"] = conf.backoff_ms;
        node["user_agent"] = conf.user_agent;
        node["per_page"] = conf.per_page;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

} // namespace chen

GitHubConf GetGitHubConf();
