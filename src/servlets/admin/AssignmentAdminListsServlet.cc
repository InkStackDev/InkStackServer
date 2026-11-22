#include "AssignmentAdminListsServlet.hpp"

#include "../../Util/Util.hpp"

#include <Chen/Log/Log.hpp>
#include "../../Repository/OrganizationRepository.hpp"
#include "../../Repository/AssignmentQueryRepository.hpp"
#include "../../Repository/AssignmentRelRepository.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Domain/UserTypes.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

AssignmentAdminListsServlet::AssignmentAdminListsServlet()
    : BlogLoginedServlet("AssignmentAdminListsServlet")
{
}

int32_t AssignmentAdminListsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");
        int32_t state = request->getParamAs<int32_t>("state", -1);

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

        uint64_t offset = (page_num - 1) * page_size;

        std::vector<DB::AssignmentInfo::ptr> infos;
        uint64_t total = AssignmentQueryRepository::ListByPages(infos, offset, page_size, state, true, GetDB());

        result->set("total", total);

        Json::Value list;
        for (const auto& i : infos) {
            Json::Value item;
            item["id"] = i->getId();
            item["name"] = i->getName();
            item["subject_name"] = i->getSubjectName();
            item["end_time"] = i->getDeadline();
            item["status"] = i->getStatus();
            item["priority"] = i->getPriority();
            item["create_time"] = i->getCreateTime();
            item["description"] = i->getDescription();
            item["file_type"] = i->getFileType();
            item["file_size"] = i->getMaxSize();

            // organization info (负责人)
            std::vector<DB::AssignmentOrganizationRelInfo::ptr> org_rels;
            AssignmentOrganizationRelRepository::ListByAssignment(org_rels, i->getId(), GetDB());
            if (!org_rels.empty() && org_rels[0]) {
                auto org = OrganizationRepository::Get(org_rels[0]->getOrganizationId(), GetDB());
                if (org) {
                    item["organization_name"] = org->getName();
                }
                item["assigned_by"] = org_rels[0]->getAssignedBy();
            }
            list.append(item);
        }
        result->set("list", list);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
