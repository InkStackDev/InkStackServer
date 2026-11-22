#include "OrganizationDeleteServlet.hpp"

#include "../../Event/EventDefine.hpp"
#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"

#include <json/json.h>

#include <set>

OrganizationDeleteServlet::OrganizationDeleteServlet()
    : BlogLoginedServlet("OrganizationDeleteServlet")
{
}

int32_t OrganizationDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, ids, "ids");
        std::set<int64_t> id_set;
        for (auto& i : chen::StringUtil::Split(ids, ",")) {
            if (!i.empty()) {
                id_set.insert(chen::TypeUtil::Atoi(i));
            }
        }
        std::vector<int64_t> org_ids(id_set.begin(), id_set.end());

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        std::vector<DB::OrganizationInfo::ptr> orgs;
        std::vector<DB::OrganizationUserRelInfo::ptr> rels;
        auto rt = OrganizationService::DeleteMany(viewer, org_ids, orgs, rels);
        if (rt == OrganizationService::DeleteResult::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt == OrganizationService::DeleteResult::NoValidOrgs) {
            result->setErrno(ORG_NO_VALID_ORGS);
            break;
        }
        if (rt != OrganizationService::DeleteResult::Ok) {
            result->setErrno(DB_COMMIT_FAILED);
            break;
        }

        Json::Value json_ids;
        for (auto& org : orgs) {
            json_ids.append(org->getId());
        }
        result->set("ids", json_ids);

        // 通知所有被删除组织的成员
        for (auto& org : orgs) {
            std::set<int64_t> notified_users;
            for (auto& rel : rels) {
                if (rel->getOrgId() != org->getId()) {
                    continue;
                }
                int64_t member_id = rel->getUserId();
                if (!notified_users.insert(member_id).second) {
                    continue;
                }

                EventOrgDeletedData data = {};
                data.member_id = member_id;
                data.org_id = org->getId();
                data.org_name = org->getName();

                chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ORG_DELETED, std::move(data));
            }
        }
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
