#include "AuthConfig.hpp"

// 配置项必须在模块 .so 的静态初始化阶段注册：chen::Library::GetModule() 在 dlopen 之后
// 会立刻调用 Config::LoadFromConfDir()，此时尚未注册的 key 不会被赋值。
static chen::ConfigVar<AuthConf>::ptr g_auth_conf = chen::Config::Lookup("auth", AuthConf(), "Auth Configuration");

static chen::ConfigVar<WebAuthnConf>::ptr g_webauthn_conf =
    chen::Config::Lookup("webauthn", WebAuthnConf(), "WebAuthn configuration");

static chen::ConfigVar<GithubOAuthConf>::ptr g_github_oauth_conf =
    chen::Config::Lookup("oauth.github", GithubOAuthConf(), "GitHub OAuth configuration");

AuthConf GetAuthConf()
{
    return g_auth_conf->getValue();
}

WebAuthnConf GetWebAuthnConf()
{
    return g_webauthn_conf->getValue();
}

GithubOAuthConf GetGithubOAuth()
{
    return g_github_oauth_conf->getValue();
}
