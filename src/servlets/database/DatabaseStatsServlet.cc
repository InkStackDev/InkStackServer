#include "DatabaseStatsServlet.hpp"

#include "../../Manager/ArticleManager.hpp"
#include "../../Manager/BackupRecordManager.hpp"
#include "../../Manager/CategoryManager.hpp"
#include "../../Manager/CommentManager.hpp"
#include "../../Manager/UserManager.hpp"
#include "../../Util/Util.hpp"
#include "DB/LabelInfo.hpp"

DatabaseStatsServlet::DatabaseStatsServlet()
    : BlogLoginedServlet("DatabaseStatsServlet")
{
}

int32_t DatabaseStatsServlet::handle(
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
        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        if (current_user->getRole() != UserManager::Role::ADMIN) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        auto astats = ArticleMgr::GetInstance()->getStats(0, -1, 0, "");
        auto cstats = CommentMgr::GetInstance()->getStats();

        int64_t total_users = 0;
        std::vector<int64_t> uids;
        UserMgr::GetInstance()->getAllIds(uids, true);
        total_users = uids.size();

        int64_t total_categories = 0;
        std::vector<DB::CategoryInfo::ptr> categories;
        CategoryMgr::GetInstance()->listAll(categories, true);
        total_categories = categories.size();

        int64_t total_labels = 0;
        auto db = GetDB();
        if (db) {
            auto qb = DB::LabelInfoDao::newQuery();
            qb->where("is_deleted", "=", (int64_t)0);
            qb->executeCount(total_labels, db);
        }

        int64_t total_backups = 0;
        std::vector<DB::BackupRecordInfo::ptr> backups;
        BackupRecordMgr::GetInstance()->list(backups, total_backups, "", "", "", 0, 1);

        auto du = GetDiskUsage();

        result->set("total_size", du.total_size);
        result->set("used_size", du.used_size);
        result->set("available_size", du.available_size);
        result->set("total_records", astats.total + total_users + cstats.total + total_categories + total_labels);
        result->set("articles", astats.total);
        result->set("users", total_users);
        result->set("comments", cstats.total);
        result->set("categories", total_categories);
        result->set("tags", total_labels);
        result->set("backups", total_backups);

        result->setErrno(SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
