#include "RdTaskDeleteServlet.hpp"

#include "../../Permission.hpp"
#include "../../Manager/OrganizationUserRelManager.hpp"
#include "../../Manager/TaskManager.hpp"
#include "../../Manager/UserManager.hpp"
#include "../../Util/Util.hpp"

RdTaskDeleteServlet::RdTaskDeleteServlet()
    : BlogLoginedServlet("RdTaskDeleteServlet")
{
}

int32_t RdTaskDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");
        std::string idsStr = request->getParam("ids");

        std::set<int64_t> delIds;
        if (id) {
            delIds.insert(id);
        }
        if (!idsStr.empty()) {
            for (auto& s : chen::StringUtil::Split(idsStr, ",")) {
                delIds.insert(chen::TypeUtil::Atoi(s));
            }
        }

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

        int32_t org_role = 0;
        if (!is_platform_admin) {
            auto rel = OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(org_id, uid);
            if (!rel || rel->getStatus() != OrganizationUserRelManager::Status::APPROVED) {
                result->setErrno(ACCESS_DENIED);
                break;
            }
            org_role = rel->getRole();
        }

        std::vector<DB::TaskInfo::ptr> all;
        TaskMgr::GetInstance()->listByOrg(all, org_id, 0, INT32_MAX, -1, true);
        std::vector<DB::TaskInfo::ptr> delItems;
        for (auto& i : all) {
            if (!delIds.count(i->getId())) {
                continue;
            }
            if (!is_platform_admin && uid != i->getCreatorId() && !CanDeleteTask(org_role)) {
                continue;
            }
            delItems.push_back(i);
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
        for (auto& i : delItems) {
            i->setIsDeleted(1);
            i->setUpdateTime(now);
            DB::TaskInfoDao::Update(i, db);
        }
        if (!trans->commit()) {
            for (auto& i : delItems) {
                i->setIsDeleted(0);
            }
            result->setErrno(DB_COMMIT_FAILED);
            break;
        }
        if (!delItems.empty()) {
            Json::Value json_ids;
            for (auto& i : delItems) {
                json_ids.append(i->getId());
            }
            result->set("ids", json_ids);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
