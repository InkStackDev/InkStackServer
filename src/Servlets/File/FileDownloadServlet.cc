#include "FileDownloadServlet.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/Log/Log.hpp>

#include "../../Util/Util.hpp"

#include <fstream>

#include "../../Config/AppConfig.hpp"
#include "../../Service/ResourceService.hpp"

static chen::Logger::ptr logger = LOG_ROOT();
static const int64_t MAX_FILE_SIZE = 100 * 1024 * 1024; // 100MB文件大小限制

// 安全处理文件名，移除危险字符
std::string SanitizeFileName(const std::string& file_name)
{
    std::string result;
    for (char c : file_name) {
        // 只保留安全字符：字母、数字、点、下划线、连字符
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-') {
            result += c;
        } else {
            result += '_'; // 替换危险字符为下划线
        }
    }
    return result;
}

// 验证路径安全性，防止路径遍历
bool IsPathSafe(const std::string& path)
{
    // 检查是否包含路径遍历字符
    if (path.find("..") != std::string::npos) {
        return false;
    }
    // 检查是否包含空字节
    if (path.find('\0') != std::string::npos) {
        return false;
    }
    // 检查危险字符
    const std::string dangerousChars = "<>\"|?*";
    if (path.find_first_of(dangerousChars) != std::string::npos) {
        return false;
    }
    return true;
}

FileDownloadServlet::FileDownloadServlet()
    : BlogLoginedServlet("FileDownloadServlet")
{
}

int32_t FileDownloadServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    bool success = false;
    do {
        DEFINE_AND_CHECK_STRING(result, url, "url");
        DEFINE_AND_CHECK_STRING(result, fileName, "file_name");

        // 安全处理文件名
        std::string safe_file_name = SanitizeFileName(fileName);
        if (safe_file_name.empty()) {
            result->setErrno(FILE_INVALID_NAME);
            break;
        }

        DB::ResourceInfo::ptr res_info;
        if (!ResourceService::GetByPath(url, res_info)) {
            result->setErrno(FILE_NOT_FOUND);
            break;
        }
        if (!res_info) {
            result->setErrno(FILE_NOT_FOUND);
            break;
        }

        // 验证路径安全性
        if (!IsPathSafe(res_info->getPath())) {
            result->setErrno(FILE_INVALID_PATH);
            break;
        }

        // 检查文件大小限制
        if (res_info->getSize() > MAX_FILE_SIZE) {
            result->setErrno(FILE_TOO_LARGE);
            break;
        }

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        if (!viewer.is_platform_admin || res_info->getOwnerId() != viewer.user_id) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        INFO(logger) << "File download: " << res_info->getPath();

        // 设置安全的响应头
        response->setHeader("Content-Disposition", "attachment; filename=\"" + safe_file_name + "\"");
        response->setHeader("Content-Type", "application/octet-stream");
        // 移除手动设置的Content-Length，让框架自动处理

        // 防止缓存
        response->setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->setHeader("Pragma", "no-cache");
        response->setHeader("Expires", "0");

        std::string filepath = GetServerWorkPath() + res_info->getPath();

        // 使用流式读取，避免内存问题
        std::ifstream ifs(filepath, std::ios::binary);
        if (!ifs) {
            result->setErrno(FILE_NOT_FOUND);
            break;
        }

        // 由于框架可能不支持appendBody，我们需要构建完整的文件内容
        // 但对于大文件，这会导致内存问题，所以先检查文件大小
        if (res_info->getSize() > 20 * 1024 * 1024) { // 20MB阈值
            // 对于大文件，使用简化的流式方法：一次性读取
            std::string file_content;
            file_content.reserve(res_info->getSize());

            ifs.clear();
            ifs.seekg(0, std::ios::beg);

            const std::streamsize BUFFER_SIZE = 64 * 1024; // 64KB缓冲区
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
        } else {
            // 对于较小文件，使用原来的分块方法但累积内容
            std::string file_content;
            file_content.reserve(res_info->getSize());

            const std::streamsize CHUNK_SIZE = 8192; // 8KB块大小
            std::vector<char> buffer(CHUNK_SIZE);

            while (ifs.read(buffer.data(), CHUNK_SIZE) || ifs.gcount() > 0) {
                std::streamsize bytesRead = ifs.gcount();
                file_content.append(buffer.data(), bytesRead);
            }

            if (ifs.bad()) {
                result->setErrno(FILE_READ_ERROR);
                break;
            }

            response->setBody(file_content);
        }

        success = true;
    } while (0);
    if (!success) {
        // 清除文件下载相关的响应头，防止浏览器将错误信息当作文件下载
        response->delHeader("Content-Disposition");
        response->delHeader("Content-Length");
        response->setHeader("Content-Type", "application/json");
        response->setBody(result->toJsonString());
    }
    const std::string& content = response->getBody();
    INFO(logger) << "File downloaded: " << request->getParam("file_name") << " (Size: " << content.size()
                 << " bytes -- " << (1.0 * content.size() / 1024) << " kb -- " << (1.0 * content.size() / (1024 * 1024)) << " mb)";
    return 0;
}
