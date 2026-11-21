/**
 * @file AuthService.hpp
 * @brief 认证服务 — 统一处理会话签发、解析与撤销
 * @author Christins
 * @date 2026-10-23
 * @copyright Apache 2.0
 */
#pragma once

#include <sstream>
#include <string>

#include <Chen/Config/Config.hpp>
#include <Chen/Http/Http.hpp>
#include <Chen/Http/HttpSession.hpp>

#include "db/UserInfo.hpp"

#include "AuthContext.hpp"
#include "../Config/AuthConfig.hpp"

/**
 * @brief 会话签发选项
 */
struct IssueOptions {
    /// 是否更新用户最近登录时间
    bool update_login_time = true;
};

class AuthService {
public:
    /**
     * @brief 解析当前请求的登录态
     * @param request HTTP 请求
     * @param response 用于清理失效 Cookie，可为空
     * @param touch 是否刷新会话活跃时间
     */
    static AuthContext Resolve(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response = nullptr, bool touch = true);

    /**
     * @brief 登录成功后签发会话
     * @return 0 成功，否则返回业务错误码
     */
    static int32_t IssueSession(chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        db::UserInfo::ptr user,
        const IssueOptions& options = IssueOptions());

    /**
     * @brief 撤销当前会话（登出）
     */
    static void RevokeSession(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response, const AuthContext& ctx);

    /**
     * @brief 撤销用户全部会话（改密、封禁、删除等场景）
     */
    static void RevokeAllSessions(int64_t uid);

    /**
     * @brief WebSocket 握手认证：仅使用握手 Cookie 中的服务端会话
     */
    static bool ResolveWs(chen::http::HttpRequest::ptr request, int64_t& uid, db::UserInfo::ptr& user);

    /**
     * @brief 写入 SESSIONID Cookie（HttpOnly / SameSite）
     */
    static void SetSessionCookie(chen::http::HttpResponse::ptr response, const std::string& session_id, int64_t max_age);

    /**
     * @brief 清除 SESSIONID Cookie
     */
    static void ClearSessionCookie(chen::http::HttpResponse::ptr response);

    /**
     * @brief 将会话映射到框架 SessionData，兼容 2FA 临时密钥等旧逻辑
     */
    static void MirrorSessionData(const std::string& session_id, int64_t uid);
};
