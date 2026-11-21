#include "UserOAuthGithubServlet.hpp"

#include <regex>

#include <Chen/Config/Config.hpp>
#include <Chen/DB/Redis.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/StringUtil.hpp>

#include "../../Manager/UserManager.hpp"
#include "../../Util/Util.hpp"
#include "../../Config/AppConfig.hpp"
#include "../../Config/AuthConfig.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

/// state 有效期（秒）
static const int kOAuthStateTTL = 600;
/// Redis state key 前缀
static const char* kOAuthStatePrefix = "oauth:github:state:";

UserOAuthGithubServlet::UserOAuthGithubServlet()
    : BlogServlet("UserOAuthGithubServlet")
{
}

/// 仅允许站内相对路径作为登录后回跳地址，防止开放重定向
static std::string NormalizeRedirect(const std::string& raw)
{
    static const std::regex pattern(R"(^/[A-Za-z0-9_\-./?=&%]*$)");
    if (!std::regex_match(raw, pattern)) {
        return "/index";
    }
    if (raw.rfind("//", 0) == 0) {
        return "/index";
    }
    return raw;
}

int32_t UserOAuthGithubServlet::handle(chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        const GithubOAuthConf oauth = GetGithubOAuth();
        std::string client_id = oauth.client_id;
        std::string callback_url = oauth.callback_url;
        if (client_id.empty() || callback_url.empty()) {
            result->setErrno(errcode::GITHUB_OAUTH_FAILED, "GitHub OAuth 未配置");
            break;
        }

        std::string redirect = NormalizeRedirect(request->getParam("redirect", "/index"));

        std::string state = UserManager::GenerateToken();
        std::string redisKey = std::string(kOAuthStatePrefix) + state;
        chen::RedisUtil::Cmd(GetRedisPoolName(), "setex %s %d %s", redisKey.c_str(), kOAuthStateTTL, redirect.c_str());

        std::string authorize_url = "https://github.com/login/oauth/authorize?client_id=" + client_id
            + "&redirect_uri=" + chen::StringUtil::UrlEncode(callback_url, false)
            + "&scope=" + chen::StringUtil::UrlEncode("read:user user:email", false) + "&state=" + state;
        response->setRedirect(authorize_url);
    } while (0);
    if (result->getErrno() != 0) {
        response->setBody(result->toJsonString());
    }
    return 0;
}
