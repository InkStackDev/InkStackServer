#include "GitHubConfig.hpp"

bool GitHubConf::operator==(const GitHubConf& other) const
{
    return api_base == other.api_base && oauth_base == other.oauth_base && timeout_ms == other.timeout_ms
        && detail_timeout_ms == other.detail_timeout_ms && max_attempts == other.max_attempts
        && backoff_ms == other.backoff_ms && user_agent == other.user_agent && per_page == other.per_page;
}

// 配置项必须在模块 .so 的静态初始化阶段注册：chen::Library::GetModule() 在 dlopen 之后
// 会立刻调用 Config::LoadFromConfDir()，此时尚未注册的 key 不会被赋值。
static chen::ConfigVar<GitHubConf>::ptr g_github_conf =
    chen::Config::Lookup("github", GitHubConf(), "GitHub API client");

GitHubConf GetGitHubConf()
{
    return g_github_conf->getValue();
}
