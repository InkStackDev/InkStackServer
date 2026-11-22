#include "RdBugServlet.hpp"

#include "../../Access/AccessPolicy.hpp"
#include "../../Access/AccessScopeResolver.hpp"
#include "../../Repository/BugRepository.hpp"
#include "../../Repository/RdListQueryRepository.hpp"
#include "../../Util/Util.hpp"
#include "RdHelper.hpp"

RdBugServlet::RdBugServlet()
    : BlogLoginedServlet("RdBugServlet")
{
}

int32_t RdBugServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t id = request->getParamAs<int64_t>("id", 0);
        int64_t org_id = request->getParamAs<int64_t>("org_id", 0);

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        auto db = GetDB();
        if (!db) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        OrgScope scope;
        if (!AccessScopeResolver::ResolveOrgs(scope, viewer, db)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        if (id) {
            auto info = BugRepository::Get(id, db);
            if (!info || info->getIsDeleted()) {
                result->setErrno(BUG_NOT_FOUND);
                break;
            }
            if (!AccessPolicy::CanViewBug(viewer, scope, info->getOrgId())) {
                result->setErrno(ACCESS_DENIED);
                break;
            }
            Json::Value data;
            BuildBugJson(data, info);
            result->set("data", data);
            break;
        }

        uint64_t page = request->getParamAs<uint64_t>("page", 1);
        uint64_t page_size = request->getParamAs<uint64_t>("page_size", 10);
        uint64_t offset = 0;
        uint64_t size = 0;
        ParsePagination(page, page_size, offset, size);

        RdListQueryRepository::Filter filter;
        filter.offset = offset;
        filter.limit = size;
        filter.search = request->getParam("search");
        std::string status_str = request->getParam("status");
        if (!status_str.empty()) {
            filter.status = StringToBugStatus(status_str);
        }
        std::string priority_str = request->getParam("priority");
        if (!priority_str.empty()) {
            filter.priority = StringToPriority(priority_str);
        }
        std::string severity_str = request->getParam("severity");
        if (!severity_str.empty()) {
            filter.severity = StringToSeverity(severity_str);
        }

        std::vector<DB::BugInfo::ptr> bugs;
        int64_t total = 0;
        if (!RdListQueryRepository::ListBugs(bugs, total, scope, org_id, filter, db)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value arr(Json::arrayValue);
        for (const auto& info : bugs) {
            Json::Value item;
            BuildBugJson(item, info);
            arr.append(item);
        }

        result->set("total", total);
        result->set("list", arr);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
