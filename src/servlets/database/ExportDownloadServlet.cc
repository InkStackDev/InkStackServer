#include "ExportDownloadServlet.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/Util.hpp>

#include "../../Manager/ExportRecordManager.hpp"
#include "../../Manager/UserManager.hpp"

#include <fstream>

#include "../../Config/AppConfig.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

ExportDownloadServlet::ExportDownloadServlet()
    : BlogLoginedServlet("ExportDownloadServlet")
{
}

int32_t ExportDownloadServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    bool success = false;
    do {
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        if (current_user->getRole() != UserManager::Role::ADMIN) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        std::string id_str = request->getParam("id");
        if (id_str.empty()) {
            result->setErrno(PARAM_MISSING, "param id is required");
            break;
        }
        int64_t id = chen::TypeUtil::Atoi(id_str);
        if (!id) {
            result->setErrno(PARAM_INVALID, "invalid id");
            break;
        }

        auto info = ExportRecordMgr::GetInstance()->get(id);
        if (!info) {
            result->setErrno(DB_OPERATION_FAILED, "export record not found");
            break;
        }

        std::string filepath = info->getFilePath();
        if (filepath.empty()) {
            result->setErrno(FILE_NOT_FOUND);
            break;
        }

        if (filepath[0] != '/') {
            filepath = GetServerDataPath() + "/" + filepath;
        }

        std::ifstream ifs(filepath, std::ios::binary);
        if (!ifs) {
            result->setErrno(FILE_NOT_FOUND);
            break;
        }

        ifs.seekg(0, std::ios::end);
        int64_t fileSize = ifs.tellg();
        ifs.seekg(0, std::ios::beg);

        std::string format = info->getFormat();
        if (format.empty())
            format = "json";
        response->setHeader("Content-Disposition", "attachment; filename=\"" + info->getName() + "." + format + "\"");
        response->setHeader("Content-Type", "application/octet-stream");
        response->setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->setHeader("Pragma", "no-cache");
        response->setHeader("Expires", "0");

        std::string file_content;
        file_content.reserve(fileSize);
        const std::streamsize BUFFER_SIZE = 64 * 1024;
        std::vector<char> buffer(BUFFER_SIZE);
        while (ifs.read(buffer.data(), BUFFER_SIZE) || ifs.gcount() > 0) {
            std::streamsize bytesRead = ifs.gcount();
            file_content.append(buffer.data(), bytesRead);
        }
        if (ifs.bad()) {
            result->setErrno(FILE_READ_ERROR);
            break;
        }
        response->setBody(file_content);
        success = true;
    } while (0);
    if (!success) {
        response->delHeader("Content-Disposition");
        response->setBody(result->toJsonString());
    }
    return 0;
}
