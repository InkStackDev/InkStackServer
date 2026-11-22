#include "GitHubConfig.hpp"

// 配置项必须在模块 .so 的静态初始化阶段注册：chen::Library::GetModule() 在 dlopen 之后
// 会立刻调用 Config::LoadFromConfDir()，此时尚未注册的 key 不会被赋值。
static chen::ConfigVar<GitHubConf>::ptr g_github_conf =
    chen::Config::Lookup("github", GitHubConf(), "GitHub API client");

GitHubConf GetGitHubConf()
{
    return g_github_conf->getValue();
}
