/**
 * @file AuthConfig.hpp
 * @brief 认证相关配置（会话 Cookie / WebAuthn / GitHub OAuth）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <Chen/Config/Config.hpp>

#include <cstdint>
#include <sstream>
#include <string>

/**
 * @brief 会话 Cookie 与有效期配置
 */
struct AuthConf {
    bool cookie_secure = false;
    std::string cookie_same_site = "Lax";
    /// 会话绝对有效期（秒），<=0 表示不限制
    int64_t session_absolute_ttl = 604800;

    bool operator==(const AuthConf& oth) const
    {
        return cookie_secure == oth.cookie_secure && cookie_same_site == oth.cookie_same_site
            && session_absolute_ttl == oth.session_absolute_ttl;
    }
};

/**
 * @brief WebAuthn / Passkey 注册与认证配置
 */
struct WebAuthnConf {
    std::string rp_id;
    std::string rp_name = "InkStack";
    std::string origin;
    int64_t timeout_ms = 60000;
    std::string user_verification = "preferred";
    std::string attestation = "none";
    std::string user_handle_secret;
    int64_t challenge_ttl = 300;

    bool operator==(const WebAuthnConf& other) const
    {
        return rp_id == other.rp_id && rp_name == other.rp_name && origin == other.origin
            && timeout_ms == other.timeout_ms && user_verification == other.user_verification
            && attestation == other.attestation && user_handle_secret == other.user_handle_secret
            && challenge_ttl == other.challenge_ttl;
    }
};

/**
 * @brief GitHub OAuth 登录配置
 */
struct GithubOAuthConf {
    std::string client_id;
    std::string client_secret;
    std::string callback_url;
    std::string frontend_base;

    bool operator==(const GithubOAuthConf& other) const
    {
        return client_id == other.client_id && client_secret == other.client_secret
            && callback_url == other.callback_url && frontend_base == other.frontend_base;
    }
};

namespace chen {

template <>
class LexicalCast<std::string, AuthConf> {
public:
    AuthConf operator()(const std::string& value)
    {
        AuthConf conf;
        const YAML::Node node = YAML::Load(value);
        if (!node.IsMap()) {
            return conf;
        }
        if (node["cookie_secure"].IsScalar()) {
            conf.cookie_secure = LexicalCast<std::string, bool>()(node["cookie_secure"].Scalar());
        }
        if (node["cookie_same_site"].IsScalar()) {
            conf.cookie_same_site = node["cookie_same_site"].Scalar();
        }
        if (node["session_absolute_ttl"].IsScalar()) {
            conf.session_absolute_ttl = LexicalCast<std::string, int64_t>()(node["session_absolute_ttl"].Scalar());
        }
        return conf;
    }
};

template <>
class LexicalCast<AuthConf, std::string> {
public:
    std::string operator()(const AuthConf& conf)
    {
        YAML::Node node;
        node["cookie_secure"] = conf.cookie_secure;
        node["cookie_same_site"] = conf.cookie_same_site;
        node["session_absolute_ttl"] = conf.session_absolute_ttl;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template <>
class LexicalCast<std::string, WebAuthnConf> {
public:
    WebAuthnConf operator()(const std::string& value)
    {
        WebAuthnConf conf;
        const YAML::Node node = YAML::Load(value);
        if (!node.IsMap()) {
            return conf;
        }
        if (node["rp_id"].IsScalar()) {
            conf.rp_id = node["rp_id"].Scalar();
        }
        if (node["rp_name"].IsScalar()) {
            conf.rp_name = node["rp_name"].Scalar();
        }
        if (node["origin"].IsScalar()) {
            conf.origin = node["origin"].Scalar();
        }
        if (node["timeout_ms"].IsScalar()) {
            conf.timeout_ms = LexicalCast<std::string, int64_t>()(node["timeout_ms"].Scalar());
        }
        if (node["user_verification"].IsScalar()) {
            conf.user_verification = node["user_verification"].Scalar();
        }
        if (node["attestation"].IsScalar()) {
            conf.attestation = node["attestation"].Scalar();
        }
        if (node["user_handle_secret"].IsScalar()) {
            conf.user_handle_secret = node["user_handle_secret"].Scalar();
        }
        if (node["challenge_ttl"].IsScalar()) {
            conf.challenge_ttl = LexicalCast<std::string, int64_t>()(node["challenge_ttl"].Scalar());
        }
        return conf;
    }
};

template <>
class LexicalCast<WebAuthnConf, std::string> {
public:
    std::string operator()(const WebAuthnConf& conf)
    {
        YAML::Node node;
        node["rp_id"] = conf.rp_id;
        node["rp_name"] = conf.rp_name;
        node["origin"] = conf.origin;
        node["timeout_ms"] = conf.timeout_ms;
        node["user_verification"] = conf.user_verification;
        node["attestation"] = conf.attestation;
        node["user_handle_secret"] = conf.user_handle_secret;
        node["challenge_ttl"] = conf.challenge_ttl;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template <>
class LexicalCast<std::string, GithubOAuthConf> {
public:
    GithubOAuthConf operator()(const std::string& value)
    {
        GithubOAuthConf conf;
        const YAML::Node node = YAML::Load(value);
        if (!node.IsMap()) {
            return conf;
        }
        if (node["client_id"].IsScalar()) {
            conf.client_id = node["client_id"].Scalar();
        }
        if (node["client_secret"].IsScalar()) {
            conf.client_secret = node["client_secret"].Scalar();
        }
        if (node["callback_url"].IsScalar()) {
            conf.callback_url = node["callback_url"].Scalar();
        }
        if (node["frontend_base"].IsScalar()) {
            conf.frontend_base = node["frontend_base"].Scalar();
        }
        return conf;
    }
};

template <>
class LexicalCast<GithubOAuthConf, std::string> {
public:
    std::string operator()(const GithubOAuthConf& conf)
    {
        YAML::Node node;
        node["client_id"] = conf.client_id;
        node["client_secret"] = conf.client_secret;
        node["callback_url"] = conf.callback_url;
        node["frontend_base"] = conf.frontend_base;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

} // namespace chen

/**
 * @brief 会话 Cookie 与有效期配置（auth）
 */
AuthConf GetAuthConf();

/**
 * @brief WebAuthn / Passkey 配置（webauthn）
 */
WebAuthnConf GetWebAuthnConf();

/**
 * @brief GitHub OAuth 登录配置（oauth.github）
 */
GithubOAuthConf GetGithubOAuth();
