#include "UserSendCodeServlet.hpp"

#include <map>

#include <Chen/Config/Config.hpp>
#include <Chen/DB/Redis.hpp>
#include <Chen/Log/Log.hpp>

#include "../../Event/EventDefine.hpp"
#include "../../I18n/I18n.hpp"
#include "../../I18n/MessageCatalog.hpp"
#include "../../Service/SystemSettingsService.hpp"
#include "../../Util/Util.hpp"
#include "../../Config/AppConfig.hpp"
#include "../../Service/UserService.hpp"
#include "../../Domain/UserTypes.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

UserSendCodeServlet::UserSendCodeServlet()
    : BlogServlet("UserSendCodeServlet")
{
}

int32_t UserSendCodeServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, email, "email");
        DEFINE_AND_CHECK_STRING(result, type, "type");
        DEFINE_AND_CHECK_STRING(result, agent, "agent");

        if (email.empty() && agent.empty()) {
            result->setErrno(PARAM_MISSING);
            break;
        }

        if (!IsEmail(email)) {
            result->setErrno(USER_INVALID_EMAIL);
            break;
        }

        if (type == "1" && UserService::GetByEmail(email)) {
            result->setErrno(USER_EMAIL_EXISTS);
            break;
        }
        if (type == "5") {
            auto recovery_user = UserService::GetByEmail(email);
            if (!recovery_user) {
                result->setErrno(USER_EMAIL_NOT_REGISTER);
                break;
            }
            if (recovery_user->getState() != UserState::ACTIVE || recovery_user->getIsDeleted()) {
                result->setErrno(ACCOUNT_INVALID);
                break;
            }
        }

        // IP 限流：每小时每 IP 最多 5 次
        {
            std::string ip = request->getHeader("X-Real-IP");
            if (ip.empty()) {
                ip = session->getRemoteAddressString();
                auto pos = ip.find(':');
                if (pos != std::string::npos) {
                    ip = ip.substr(0, pos);
                }
            }
            auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "INCR code_limit:ip:%s", ip.c_str());
            if (rpy && rpy->integer == 1) {
                chen::RedisUtil::Cmd(GetRedisPoolName(), "EXPIRE code_limit:ip:%s 3600", ip.c_str());
            }
            if (rpy && rpy->integer > 5) {
                result->setErrno(SEND_CODE_FREQUENT);
                break;
            }
        }

        // 检查 SMTP 配置（在写 DB 之前校验，避免产生无效验证码）
        auto sys_settings = SystemSettingsServiceRegistry::GetInstance()->get();
        if (!sys_settings || sys_settings->getSmtpHost().empty() || sys_settings->getSmtpPort() <= 0 || sys_settings->getSmtpPort() > 65535) {
            result->setErrno(SMTP_NOT_CONFIGURED);
            break;
        }
        bool smtp_use_ssl = false;
        if (!ResolveSmtpTls(sys_settings->getSmtpEncryption(), smtp_use_ssl)) {
            ERROR(logger) << "unsupported smtp encryption: " << sys_settings->getSmtpEncryption();
            result->setErrno(
                SMTP_ENCRYPTION_UNSUPPORTED, "SMTP STARTTLS is not supported, use port 465 with SSL encryption");
            break;
        }

        // 生成验证码并写入 Redis，10分钟过期
        std::string code = chen::RandomUtil::RandString(6);
        auto rpy = chen::RedisUtil::Cmd(
            GetRedisPoolName(), "SETEX email:verify:%s:%s 600 %s", type.c_str(), email.c_str(), code.c_str());
        if (!rpy) {
            result->setErrno(REDIS_OPERATION_FAILED);
            break;
        }

        INFO(logger) << "email=" << email << " type=" << type << " code=" << code;

        std::string site_name = sys_settings->getSiteName();

        // 异步发送邮件，不阻塞请求响应
        // type: 1-注册, 3-重置密码, 5-账号恢复
        bool is_register = (type == "1");
        bool is_recovery = (type == "5");
        const std::string lang = I18n::ResolveLanguage(request);
        const std::string kind = is_register ? "register" : (is_recovery ? "recovery" : "reset_password");
        std::map<std::string, std::string> params;
        params["site"] = site_name;
        std::string title =
            MessageCatalogRegistry::GetInstance()->render(lang, "email.verification.subject." + kind, params);
        std::string intro =
            MessageCatalogRegistry::GetInstance()->render(lang, "email.verification.intro." + kind, params);
        std::string greeting = MessageCatalogRegistry::GetInstance()->render(lang, "email.verification.greeting");
        std::string validity = MessageCatalogRegistry::GetInstance()->render(lang, "email.verification.validity");
        std::string footer = MessageCatalogRegistry::GetInstance()->render(lang, "email.verification.footer");
        std::string code_html = std::string() + "<div style=\"max-width:480px;margin:0 auto;padding:32px 24px;"
            + "font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
            + "background:#ffffff;border-radius:12px;box-shadow:0 2px 12px rgba(0,0,0,0.08)\">"
            + "<div style=\"text-align:center;padding-bottom:24px;border-bottom:1px solid #f0f0f0\">"
            + "<h1 style=\"margin:0;font-size:22px;color:#1a1a1a\">" + site_name + "</h1>" + "</div>"
            + "<div style=\"padding:24px 0\">" + "<p style=\"margin:0 0 8px;font-size:15px;color:#555\">" + greeting
            + "</p>" + "<p style=\"margin:0 0 24px;font-size:15px;color:#555;line-height:1.6\">" + intro + "</p>"
            + "<div style=\"background:#f7f8fa;border-radius:8px;padding:20px;text-align:center;margin-bottom:24px\">"
            + "<span style=\"font-size:32px;font-weight:700;letter-spacing:6px;color:#1a1a1a;font-family:'Courier New',monospace\">" + code + "</span>" + "</div>" + "<p style=\"margin:0;font-size:13px;color:#999\">" + validity + "</p>"
            + "</div>" + "<div style=\"padding-top:16px;border-top:1px solid #f0f0f0;text-align:center\">"
            + "<p style=\"margin:0;font-size:12px;color:#bbb\">" + footer + "</p>" + "</div>" + "</div>";
        auto mail = chen::EMail::Create(
            sys_settings->getSmtpUsername(), sys_settings->getSmtpPassword(), title, code_html, {email}, {}, {sys_settings->getFromEmail()});
        std::string smtp_host = sys_settings->getSmtpHost();
        int32_t smtp_port = sys_settings->getSmtpPort();

        // 异步触发事件，发送邮件
        {
            EventUserSendCodeData data = {};
            data.email = mail;
            data.smtp_host = smtp_host;
            data.port = static_cast<uint16_t>(smtp_port);
            data.use_ssl = smtp_use_ssl;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_USER_SEND_CODE, std::move(data));
        }

        result->setErrno(SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
