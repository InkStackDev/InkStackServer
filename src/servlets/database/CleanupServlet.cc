#include "CleanupServlet.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/Util.hpp>

#include "../../Repository/RecordRepository.hpp"
#include "../../Util/Util.hpp"
#include "DB/BackupRecordInfo.hpp"
#include "DB/ExportRecordInfo.hpp"
#include "../../Config/AppConfig.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Domain/UserTypes.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

CleanupServlet::CleanupServlet()
    : BlogLoginedServlet("CleanupServlet")
{
}

int32_t CleanupServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        auto current_user = UserRepository::Get(uid, GetDB());
        if (!current_user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        if (current_user->getRole() != UserRole::ADMIN) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        int64_t now = time(0);
        int64_t deadline = now - 30 * 24 * 3600;
        int64_t cleaned = 0;
        int64_t freed = 0;

        auto db = GetDB();
        if (!db) {
            result->setErrno(DB_CONNECTION_FAILED);
            break;
        }

        std::vector<DB::BackupRecordInfo::ptr> old_backups;
        BackupRecordRepository::ListDeletedBefore(old_backups, deadline, db);
        for (auto& b : old_backups) {
            if (!b->getFilePath().empty()) {
                std::string fp = b->getFilePath();
                if (fp[0] != '/') {
                    fp = GetServerWorkPath() + "/" + fp;
                }
                int64_t sz = b->getSize();
                if (chen::FSUtil::Unlink(fp, true) == 0) {
                    freed += sz;
                }
            }
            BackupRecordRepository::DeleteById(b->getId(), db);
            cleaned++;
        }

        std::vector<DB::ExportRecordInfo::ptr> old_exports;
        ExportRecordRepository::ListDeletedBefore(old_exports, deadline, db);
        for (auto& e : old_exports) {
            if (!e->getFilePath().empty()) {
                std::string fp = e->getFilePath();
                if (fp[0] != '/') {
                    fp = GetServerWorkPath() + "/" + fp;
                }
                int64_t sz = e->getSize();
                if (chen::FSUtil::Unlink(fp, true) == 0) {
                    freed += sz;
                }
            }
            ExportRecordRepository::DeleteById(e->getId(), db);
            cleaned++;
        }

        result->set("cleaned", cleaned);
        result->set("freedBytes", freed);
        result->setErrno(SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
