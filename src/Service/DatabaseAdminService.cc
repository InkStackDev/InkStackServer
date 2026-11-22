#include "DatabaseAdminService.hpp"

#include "../Config/AppConfig.hpp"
#include "../Repository/ArticleQueryRepository.hpp"
#include "../Repository/CategoryRepository.hpp"
#include "../Repository/CommentQueryRepository.hpp"
#include "../Repository/LabelRepository.hpp"
#include "../Repository/RecordRepository.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Util/Util.hpp"

#include <Chen/Util/FSUtil.hpp>

#include <string>
#include <vector>

bool DatabaseAdminService::GetStats(Stats& out)
{
    out = Stats{};
    auto db = GetDB();
    if (!db) {
        return false;
    }

    auto article_stats = ArticleQueryRepository::GetStats(0, -1, 0, "", db);
    auto comment_stats = CommentQueryRepository::GetStats(db);

    std::vector<int64_t> uids;
    UserRepository::GetAllIds(uids, true, db);
    out.users = (int64_t)uids.size();

    std::vector<DB::CategoryInfo::ptr> categories;
    CategoryRepository::ListAll(categories, true, db);
    out.categories = (int64_t)categories.size();

    out.labels = LabelRepository::CountValid(db);
    out.articles = article_stats.total;
    out.comments = comment_stats.total;

    int64_t total_backups = 0;
    std::vector<DB::BackupRecordInfo::ptr> backups;
    BackupRecordRepository::ListByPages(backups, total_backups, "", "", "", 0, 1, db);
    out.backups = total_backups;
    return true;
}

bool DatabaseAdminService::Cleanup(int64_t deadline, int64_t& backup_count, int64_t& export_count, int64_t& freed_bytes)
{
    backup_count = 0;
    export_count = 0;
    freed_bytes = 0;
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<DB::BackupRecordInfo::ptr> old_backups;
    BackupRecordRepository::ListDeletedBefore(old_backups, deadline, db);
    for (const auto& backup : old_backups) {
        if (!backup->getFilePath().empty()) {
            std::string path = backup->getFilePath();
            if (path[0] != '/') {
                path = GetServerWorkPath() + "/" + path;
            }
            int64_t size = backup->getSize();
            if (chen::FSUtil::Unlink(path, true) == 0) {
                freed_bytes += size;
            }
        }
        BackupRecordRepository::DeleteById(backup->getId(), db);
        backup_count++;
    }

    std::vector<DB::ExportRecordInfo::ptr> old_exports;
    ExportRecordRepository::ListDeletedBefore(old_exports, deadline, db);
    for (const auto& record : old_exports) {
        if (!record->getFilePath().empty()) {
            std::string path = record->getFilePath();
            if (path[0] != '/') {
                path = GetServerWorkPath() + "/" + path;
            }
            int64_t size = record->getSize();
            if (chen::FSUtil::Unlink(path, true) == 0) {
                freed_bytes += size;
            }
        }
        ExportRecordRepository::DeleteById(record->getId(), db);
        export_count++;
    }
    return true;
}
