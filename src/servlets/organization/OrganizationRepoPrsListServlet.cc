/**
 * @file OrganizationRepoPrsListServlet.cc
 * @brief 组织仓库 PR 列表接口实现
 * @author Christins
 * @date 2026-06-24
 * @copyright Apache 2.0
 */
#include "OrganizationRepoPrsListServlet.hpp"

#include "../../Service/OrganizationRepoPrService.hpp"
#include "../../Util/Util.hpp"

OrganizationRepoPrsListServlet::OrganizationRepoPrsListServlet()
    : BlogLoginedServlet("OrganizationRepoPrsListServlet")
{
}

int32_t OrganizationRepoPrsListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t repo_id = request->getParamAs<int64_t>("repo_id", 0);
        int64_t org_id = request->getParamAs<int64_t>("org_id", 0);
        uint64_t page = request->getParamAs<uint64_t>("page", 1);
        uint64_t page_size = request->getParamAs<uint64_t>("page_size", 20);
        std::string state = request->getParam("state");

        if (page_size > 50) {
            page_size = 50;
        }
        if (page < 1) {
            page = 1;
        }

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        uint64_t offset = (page - 1) * page_size;
        std::vector<DB::OrganizationRepoPrsInfo::ptr> prs;
        int64_t total = 0;
        auto rt = OrganizationRepoPrService::List(viewer, repo_id, org_id, state, offset, page_size, prs, total);
        if (rt == OrganizationRepoPrService::ListStatus::REPO_NOT_FOUND) {
            result->setErrno(ORG_REPO_NOT_FOUND);
            break;
        }
        if (rt == OrganizationRepoPrService::ListStatus::ORG_NOT_FOUND) {
            result->setErrno(ORG_NOT_FOUND);
            break;
        }
        if (rt == OrganizationRepoPrService::ListStatus::NOT_MEMBER
            || rt == OrganizationRepoPrService::ListStatus::ACCESS_DENIED) {
            result->setErrno(ACCESS_DENIED, "无权访问该组织");
            break;
        }
        if (rt != OrganizationRepoPrService::ListStatus::OK) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total", total);
        result->set("page", page);
        result->set("page_size", page_size);
        Json::Value list;
        for (const auto& pr : prs) {
            Json::Value item;
            item["id"] = pr->getId();
            item["repo_id"] = pr->getRepoId();
            item["github_pr_id"] = pr->getGithubPrId();
            item["title"] = pr->getTitle();
            item["description"] = pr->getDescription();
            item["state"] = pr->getState();
            item["priority"] = pr->getPriority();
            item["author"] = pr->getAuthor();
            item["head_branch"] = pr->getHeadBranch();
            item["base_branch"] = pr->getBaseBranch();
            item["commit_sha"] = pr->getCommitSha().substr(0, 7);
            item["changed_files"] = pr->getChangedFiles();
            item["additions"] = pr->getAdditions();
            item["deletions"] = pr->getDeletions();
            item["reviewers"] = pr->getReviewers();
            item["review_status"] = pr->getReviewStatus();
            item["closed_at"] = pr->getClosedAt();
            item["merged_at"] = pr->getMergedAt();
            item["created_at"] = pr->getCreateTime();
            item["updated_at"] = pr->getUpdateTime();
            list.append(item);
        }
        result->set("list", list);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
