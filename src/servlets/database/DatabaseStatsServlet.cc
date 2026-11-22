#include "DatabaseStatsServlet.hpp"

#include "../../Repository/LabelRepository.hpp"
#include "../../Util/Util.hpp"
#include "DB/LabelInfo.hpp"
#include "../../Repository/ArticleQueryRepository.hpp"
#include "../../Repository/CategoryRepository.hpp"
#include "../../Repository/CommentQueryRepository.hpp"
#include "../../Repository/RecordRepository.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Domain/UserTypes.hpp"

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
        auto current_user = UserRepository::Get(uid, GetDB());
        if (!current_user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        if (current_user->getRole() != UserRole::ADMIN) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        auto astats = ArticleQueryRepository::GetStats(0, -1, 0, "", GetDB());
        auto cstats = CommentQueryRepository::GetStats();

        int64_t total_users = 0;
        std::vector<int64_t> uids;
        UserRepository::GetAllIds(uids, true, GetDB());
        total_users = uids.size();

        int64_t total_categories = 0;
        std::vector<DB::CategoryInfo::ptr> categories;
        CategoryRepository::ListAll(categories, true, GetDB());
        total_categories = categories.size();

        int64_t total_labels = LabelRepository::CountValid(GetDB());

        int64_t total_backups = 0;
        std::vector<DB::BackupRecordInfo::ptr> backups;
        BackupRecordRepository::ListByPages(backups, total_backups, "", "", "", 0, 1, GetDB());

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
