#include "SystemSettingsServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Service/SystemSettingsService.hpp"
#include "../../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

SystemSettingsServlet::SystemSettingsServlet()
    : BlogLoginedServlet("SystemSettingsServlet")
{
}

static void BuildSettingsJson(Json::Value& json, DB::SystemSettingsInfo::ptr info)
{
    json["id"] = info->getId();
    json["site_name"] = info->getSiteName();
    json["site_description"] = info->getSiteDescription();
    json["site_keywords"] = info->getSiteKeywords();
    json["site_icon"] = info->getSiteIcon();
    json["site_logo"] = info->getSiteLogo();
    json["favicon"] = info->getFavicon();
    json["admin_email"] = info->getAdminEmail();
    json["timezone"] = info->getTimezone();
    json["language"] = info->getLanguage();

    json["smtp_host"] = info->getSmtpHost();
    json["smtp_port"] = info->getSmtpPort();
    json["smtp_username"] = info->getSmtpUsername();
    if (!info->getSmtpPassword().empty()) {
        json["smtp_password"] = "****";
    } else {
        json["smtp_password"] = "";
    }
    json["smtp_encryption"] = info->getSmtpEncryption();
    json["from_email"] = info->getFromEmail();
    json["from_name"] = info->getFromName();
    json["reply_to"] = info->getReplyTo();

    json["enable_registration"] = info->getEnableRegistration() ? true : false;
    json["require_email_verification"] = info->getRequireEmailVerification() ? true : false;
    json["allow_comments"] = info->getAllowComments() ? true : false;
    json["moderate_comments"] = info->getModerateComments() ? true : false;
    json["max_file_size"] = info->getMaxFileSize();
    json["allowed_file_types"] = info->getAllowedFileTypes();
    json["session_timeout"] = info->getSessionTimeout();
    json["maintenance_mode"] = info->getMaintenanceMode() ? true : false;
    json["backup_schedule"] = info->getBackupSchedule();
    json["created_at"] = info->getCreatedAt();
    json["updated_at"] = info->getUpdatedAt();
}

int32_t SystemSettingsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        if (!viewer.is_platform_admin) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        auto method = request->getMethod();

        if (method == chen::http::HttpMethod::GET) {
            auto settings = SystemSettingsServiceRegistry::GetInstance()->get();
            if (!settings) {
                result->setErrno(SETTINGS_NOT_LOADED);
                break;
            }
            Json::Value data;
            BuildSettingsJson(data, settings);
            result->setData(data);
        } else if (method == chen::http::HttpMethod::PUT) {
            auto info = SystemSettingsServiceRegistry::GetInstance()->clone();
            if (!info) {
                result->setErrno(SETTINGS_NOT_LOADED);
                break;
            }

            std::string param;

            param = request->getParam("site_name");
            if (!param.empty()) {
                info->setSiteName(param);
            }
            param = request->getParam("site_description");
            if (!param.empty()) {
                info->setSiteDescription(param);
            }
            param = request->getParam("site_keywords");
            if (!param.empty()) {
                info->setSiteKeywords(param);
            }
            param = request->getParam("site_icon");
            if (!param.empty()) {
                info->setSiteIcon(param);
            }
            param = request->getParam("site_logo");
            if (!param.empty()) {
                info->setSiteLogo(param);
            }
            param = request->getParam("favicon");
            if (!param.empty()) {
                info->setFavicon(param);
            }
            param = request->getParam("admin_email");
            if (!param.empty()) {
                info->setAdminEmail(param);
            }
            param = request->getParam("timezone");
            if (!param.empty()) {
                info->setTimezone(param);
            }
            param = request->getParam("language");
            if (!param.empty()) {
                info->setLanguage(param);
            }

            param = request->getParam("smtp_host");
            if (!param.empty()) {
                info->setSmtpHost(param);
            }
            int32_t smtpPort = request->getParamAs<int32_t>("smtp_port", 0);
            if (smtpPort > 0) {
                info->setSmtpPort(smtpPort);
            }
            param = request->getParam("smtp_username");
            if (!param.empty()) {
                info->setSmtpUsername(param);
            }
            param = request->getParam("smtp_password");
            if (!param.empty() && param != "****") {
                info->setSmtpPassword(param);
            }
            param = request->getParam("smtp_encryption");
            if (!param.empty()) {
                info->setSmtpEncryption(param);
            }
            param = request->getParam("from_email");
            if (!param.empty()) {
                info->setFromEmail(param);
            }
            param = request->getParam("from_name");
            if (!param.empty()) {
                info->setFromName(param);
            }
            param = request->getParam("reply_to");
            if (!param.empty()) {
                info->setReplyTo(param);
            }

            param = request->getParam("enable_registration");
            if (!param.empty()) {
                info->setEnableRegistration(param == "true" || param == "1" ? 1 : 0);
            }
            param = request->getParam("require_email_verification");
            if (!param.empty()) {
                info->setRequireEmailVerification(param == "true" || param == "1" ? 1 : 0);
            }
            param = request->getParam("allow_comments");
            if (!param.empty()) {
                info->setAllowComments(param == "true" || param == "1" ? 1 : 0);
            }
            param = request->getParam("moderate_comments");
            if (!param.empty()) {
                info->setModerateComments(param == "true" || param == "1" ? 1 : 0);
            }
            int32_t maxFileSize = request->getParamAs<int32_t>("max_file_size", 0);
            if (maxFileSize > 0) {
                info->setMaxFileSize(maxFileSize);
            }
            param = request->getParam("allowed_file_types");
            if (!param.empty()) {
                info->setAllowedFileTypes(param);
            }
            int32_t sessionTimeout = request->getParamAs<int32_t>("session_timeout", 0);
            if (sessionTimeout > 0) {
                info->setSessionTimeout(sessionTimeout);
            }
            param = request->getParam("maintenance_mode");
            if (!param.empty()) {
                info->setMaintenanceMode(param == "true" || param == "1" ? 1 : 0);
            }
            param = request->getParam("backup_schedule");
            if (!param.empty()) {
                info->setBackupSchedule(param);
            }

            info->setUpdatedAt(time(0));

            if (!SystemSettingsServiceRegistry::GetInstance()->update(info)) {
                result->setErrno(SETTINGS_SAVE_FAILED);
                break;
            }
            Json::Value data;
            BuildSettingsJson(data, info);
            result->setData(data);
        } else {
            result->setErrno(METHOD_NOT_ALLOWED);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
