#include "RdStatsServlet.hpp"

#include "../../Manager/UserManager.hpp"
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"
#include "../../Util/Util.hpp"
#include "../../Repository/TaskRepository.hpp"
#include "../../Repository/BugRepository.hpp"
#include "../../Repository/RequirementRepository.hpp"
#include "../../Domain/RdTypes.hpp"

RdStatsServlet::RdStatsServlet()
    : BlogLoginedServlet("RdStatsServlet")
{
}

int32_t RdStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t org_id = request->getParamAs<int64_t>("org_id", 0);
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        auto user = UserMgr::GetInstance()->get(uid);
        if (!user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        int32_t system_role = user->getRole();
        bool is_platform_admin = (system_role == UserManager::Role::ADMIN);
        time_t now = time(0);

        std::vector<DB::RequirementInfo::ptr> reqs;
        std::vector<DB::BugInfo::ptr> bugs;
        std::vector<DB::TaskInfo::ptr> tasks;

        if (is_platform_admin) {
            if (org_id) {
                RequirementRepository::ListByOrg(reqs, org_id, 0, INT32_MAX, -1, true, GetDB());
                BugRepository::ListByOrg(bugs, org_id, 0, INT32_MAX, -1, true, GetDB());
                TaskRepository::ListByOrg(tasks, org_id, 0, INT32_MAX, -1, true, GetDB());
            } else {
                RequirementRepository::ListByPages(reqs, 0, INT32_MAX, -1, true, GetDB());
                BugRepository::ListByPages(bugs, 0, INT32_MAX, -1, true, GetDB());
                TaskRepository::ListByPages(tasks, 0, INT32_MAX, -1, true, GetDB());
            }
        } else {
            std::vector<DB::OrganizationUserRelInfo::ptr> user_orgs;
            OrganizationUserRelRepository::ListByUser(user_orgs, uid, OrgMemberStatus::APPROVED, true, GetDB());
            for (auto& rel : user_orgs) {
                if (org_id && rel->getOrgId() != org_id) {
                    continue;
                }
                std::vector<DB::RequirementInfo::ptr> oReqs;
                RequirementRepository::ListByOrg(oReqs, rel->getOrgId(), 0, INT32_MAX, -1, true, GetDB());
                reqs.insert(reqs.end(), oReqs.begin(), oReqs.end());
                std::vector<DB::BugInfo::ptr> oBugs;
                BugRepository::ListByOrg(oBugs, rel->getOrgId(), 0, INT32_MAX, -1, true, GetDB());
                bugs.insert(bugs.end(), oBugs.begin(), oBugs.end());
                std::vector<DB::TaskInfo::ptr> oTasks;
                TaskRepository::ListByOrg(oTasks, rel->getOrgId(), 0, INT32_MAX, -1, true, GetDB());
                tasks.insert(tasks.end(), oTasks.begin(), oTasks.end());
            }
        }

        int total_reqs = 0, open_reqs = 0;
        for (auto& info : reqs) {
            total_reqs++;
            int st = info->getStatus();
            if (st != RequirementStatus::DONE && st != RequirementStatus::CLOSED) {
                open_reqs++;
            }
        }

        int total_bugs = 0, unresolved_bugs = 0;
        for (auto& info : bugs) {
            total_bugs++;
            int st = info->getStatus();
            if (st != BugStatus::FIXED && st != BugStatus::CLOSED) {
                unresolved_bugs++;
            }
        }

        int total_tasks = 0, overdue_tasks = 0;
        for (auto& info : tasks) {
            total_tasks++;
            int st = info->getStatus();
            if (st != TaskStatus::DONE && st != TaskStatus::CLOSED && info->getDeadline() > 0
                && info->getDeadline() < now) {
                overdue_tasks++;
            }
        }

        result->set("total_requirements", total_reqs);
        result->set("open_requirements", open_reqs);
        result->set("total_bugs", total_bugs);
        result->set("unresolved_bugs", unresolved_bugs);
        result->set("total_tasks", total_tasks);
        result->set("overdue_tasks", overdue_tasks);

    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
