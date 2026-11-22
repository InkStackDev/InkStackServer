#include "AssignmentDeleteServlet.hpp"

#include "../../Event/EventDefine.hpp"
#include "../../Manager/AssignmentManager.hpp"
#include "../../Manager/UserManager.hpp"
#include "../../Util/Util.hpp"
#include "DB/AssignmentInfo.hpp"
#include "DB/AssignmentUserRelInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Log/Log.hpp>

#include <set>

static chen::Logger::ptr logger = LOG_ROOT();

AssignmentDeleteServlet::AssignmentDeleteServlet()
    : BlogLoginedServlet("AssignmentDeleteServlet")
{
}

int32_t AssignmentDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, ids, "ids");
        std::set<int64_t> assignment_ids;
        auto tmp = chen::StringUtil::Split(ids, ",");
        for (auto& i : tmp) {
            assignment_ids.insert(chen::TypeUtil::Atoi(i));
        }

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
        std::vector<DB::AssignmentInfo::ptr> infos;
        if (!AssignmentMgr::GetInstance()->listByPages(infos, 0, INT32_MAX, -1, true)) {
            break;
        }

        std::vector<DB::AssignmentInfo::ptr> del_assignments;
        for (auto& i : infos) {
            if (assignment_ids.count(i->getId())) {
                del_assignments.push_back(i);
            }
        }

        auto db = GetDB();
        if (!db) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        auto trans = db->openTransaction();
        if (!trans) {
            result->setErrno(DB_TRANSACTION_FAILED);
            break;
        }
        time_t now = time(0);
        for (auto& i : del_assignments) {
            i->setIsDeleted(1);
            i->setUpdateTime(now);
            DB::AssignmentInfoDao::Update(i, db);
        }
        if (!trans->commit()) {
            ERROR(logger) << "commit fail";
            result->setErrno(DB_COMMIT_FAILED);

            for (auto& i : del_assignments) {
                i->setIsDeleted(0);
            }
            break;
        }
        for (auto& i : del_assignments) {
            AssignmentMgr::GetInstance()->add(i);
        }
        if (!del_assignments.empty()) {
            Json::Value json_ids;
            for (auto& i : del_assignments) {
                json_ids.append(i->getId());
            }
            result->set("ids", json_ids);

            // Notify submitters
            for (auto& assign : del_assignments) {
                std::string assign_name = assign->getName();
                std::set<int64_t> notified;
                auto db2 = GetDB();
                if (db2) {
                    auto qb = DB::AssignmentUserRelInfoDao::newQuery();
                    qb->where("assignment_id", "=", assign->getId());
                    qb->where("is_deleted", "=", (int64_t)0);
                    std::vector<DB::AssignmentUserRelInfo::ptr> submitters;
                    if (DB::AssignmentUserRelInfoDao::QueryByBuilder(submitters, qb, db2) == 0) {
                        for (auto& info : submitters) {
                            if (!info) {
                                continue;
                            }
                            int64_t submitter_id = info->getUserId();
                            if (notified.count(submitter_id)) {
                                continue;
                            }
                            notified.insert(submitter_id);

                            EventAssignmentData data = {};
                            data.type = "deleted";
                            data.assignment_id = assign->getId();
                            data.assignment_name = assign_name;
                            data.submitter_id = submitter_id;

                            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ASSIGNMENT, std::move(data));
                        }
                    }
                }
            }
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
