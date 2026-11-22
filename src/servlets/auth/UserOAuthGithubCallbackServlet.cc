#include "UserOAuthGithubCallbackServlet.hpp"

#include <regex>

#include <Chen/DB/Redis.hpp>
#include <Chen/Log/Log.hpp>

#include "../../Auth/AuthService.hpp"
#include "../../Client/GitHubClient.hpp"
#include "../../Config/AppConfig.hpp"
#include "../../Config/AuthConfig.hpp"
#include "../../Domain/UserTypes.hpp"
#include "../../Service/UserOAuthService.hpp"
#include "../../Service/UserService.hpp"
#include "../../Util/Util.hpp"
#include "DB/UserInfo.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

/// Redis state key 前缀（与入口 servlet 保持一致）
static const char* kOAuthStatePrefix = "oauth:github:state:";
/// GitHub 平台标识
static const char* kProviderGithub = "github";

UserOAuthGithubCallbackServlet::UserOAuthGithubCallbackServlet()
    : BlogServlet("UserOAuthGithubCallbackServlet")
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

/// 重定向到前端登录页并携带错误码
static void RedirectError(chen::http::HttpResponse::ptr response, const std::string& frontend_base, int32_t code)
{
    if (code == 0) {
        code = GITHUB_OAUTH_FAILED;
    }
    response->setRedirect(frontend_base + "/auth?oauth_error=" + std::to_string(code));
}

int32_t UserOAuthGithubCallbackServlet::handle(chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    const GithubOAuthConf oauth = GetGithubOAuth();
    std::string client_id = oauth.client_id;
    std::string client_secret = oauth.client_secret;
    std::string callback_url = oauth.callback_url;
    std::string frontend_base = oauth.frontend_base;

    if (client_id.empty() || client_secret.empty() || callback_url.empty() || frontend_base.empty()) {
        result->setErrno(GITHUB_OAUTH_FAILED, "GitHub OAuth 未配置");
        response->setBody(result->toJsonString());
        return 0;
    }
    while (frontend_base.size() > 1 && frontend_base.back() == '/') {
        frontend_base.pop_back();
    }

    std::string code = request->getParam("code");
    std::string state = request->getParam("state");
    std::string redirect = "/index";
    int32_t error = 0;
    bool success = false;

    do {
        if (code.empty() || state.empty()) {
            error = GITHUB_STATE_INVALID;
            break;
        }

        // 校验并一次性消费 state
        std::string stateKey = std::string(kOAuthStatePrefix) + state;
        auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get %s", stateKey.c_str());
        if (!rpy || rpy->type != REDIS_REPLY_STRING || !rpy->str) {
            error = GITHUB_STATE_INVALID;
            break;
        }
        redirect = NormalizeRedirect(std::string(rpy->str, rpy->len));
        chen::RedisUtil::Cmd(GetRedisPoolName(), "del %s", stateKey.c_str());

        // 换取 GitHub 身份
        auto token = GitHubClient::ExchangeOAuthCode(client_id, client_secret, code, callback_url);
        if (!token.ok()) {
            ERROR(logger) << "github oauth token failed, error=" << (int)token.error << " detail=" << token.detail;
            error = GITHUB_OAUTH_FAILED;
            break;
        }

        auto gh_user = GitHubClient::GetOAuthUser(token.value.access_token);
        if (!gh_user.ok()) {
            ERROR(logger) << "github oauth user failed, error=" << (int)gh_user.error << " detail=" << gh_user.detail;
            error = GITHUB_OAUTH_FAILED;
            break;
        }
        GitHubOAuthUser profile = gh_user.value;
        if (profile.email.empty()) {
            auto email = GitHubClient::GetPrimaryEmail(token.value.access_token);
            if (email.ok()) {
                profile.email = email.value;
            }
        }

        // 查询或创建本地用户
        DB::UserInfo::ptr user;
        auto binding = UserOAuthService::GetByOpenId(kProviderGithub, profile.open_id);
        if (binding) {
            user = UserService::Get(binding->getUserId());
            if (!user) {
                error = USER_NOT_FOUND;
                break;
            }
        }

        if (!user) {
            UserService::OAuthUserInput input;
            input.open_id = profile.open_id;
            input.login = profile.login;
            input.name = profile.name;
            input.email = profile.email;
            input.avatar = profile.avatar;

            auto rt = UserService::FindOrCreateOAuthUser(input, user);
            if (rt != UserService::FindOrCreateOAuthResult::Ok) {
                error =
                    (rt == UserService::FindOrCreateOAuthResult::InternalError) ? INTERNAL_ERROR : DB_OPERATION_FAILED;
                break;
            }
            UserOAuthService::Bind(user->getId(), kProviderGithub, profile.open_id, profile.login);
        }

        if (!user) {
            error = USER_NOT_FOUND;
            break;
        }
        if (user->getState() != UserState::ACTIVE || user->getIsDeleted()) {
            error = ACCOUNT_INVALID;
            break;
        }

        // 签发登录态（OAuth 用户跳过 2FA）
        IssueOptions options;
        int32_t ret = AuthService::IssueSession(request, response, session, user, options);
        if (ret != 0) {
            error = ret;
            break;
        }
        INFO(logger) << "github oauth login user=" << user->getId() << " login=" << profile.login;

        success = true;
    } while (0);

    if (!success) {
        RedirectError(response, frontend_base, error);
        return 0;
    }

    std::string sep = redirect.find('?') == std::string::npos ? "?" : "&";
    response->setRedirect(frontend_base + redirect + sep + "oauth=1");
    return 0;
}
