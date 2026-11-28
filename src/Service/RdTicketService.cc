#include "RdTicketService.hpp"

#include "../Repository/BugRepository.hpp"
#include "../Repository/OrganizationRepository.hpp"
#include "../Repository/RequirementRepository.hpp"
#include "../Repository/TaskRepository.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Util/Util.hpp"

#include <climits>
#include <utility>

template <typename Info>
static void FillBase(RdTicketService::Item& item, const Info& info, chen::IDB::ptr db)
{
    item.id = info->getId();
    item.title = info->getTitle();
    item.description = info->getDescription();
    item.status = info->getStatus();
    item.priority = info->getPriority();
    item.creator_id = info->getCreatorId();
    item.assignee_id = info->getAssigneeId();
    item.org_id = info->getOrgId();
    item.created_at = info->getCreateTime();
    item.updated_at = info->getUpdateTime();

    auto creator = UserRepository::Get(item.creator_id, db);
    item.creator = creator ? creator->getName() : "";
    item.creator_avatar = creator ? creator->getAvatar() : "";
    auto assignee = UserRepository::Get(item.assignee_id, db);
    item.assignee = assignee ? assignee->getName() : "";
    item.assignee_avatar = assignee ? assignee->getAvatar() : "";
    auto org = OrganizationRepository::Get(item.org_id, db);
    item.org_name = org ? org->getName() : "";
}

RdTicketService::Result RdTicketService::ListMine(const Viewer& viewer,
    const std::string& type,
    int64_t org_id,
    int32_t status,
    const std::string& search,
    uint64_t offset,
    uint64_t limit,
    std::vector<Item>& out,
    int64_t& total)
{
    out.clear();
    total = 0;
    if (!viewer.valid()) {
        return Result::DbError;
    }
    auto db = GetDB();
    if (!db) {
        return Result::DbError;
    }

    if (type == "requirement") {
        std::vector<DB::RequirementInfo::ptr> all;
        RequirementRepository::ListByPages(all, 0, INT32_MAX, -1, true, db);
        std::vector<DB::RequirementInfo::ptr> mine;
        for (const auto& info : all) {
            if (org_id && info->getOrgId() != org_id) {
                continue;
            }
            if (info->getCreatorId() != viewer.user_id && info->getAssigneeId() != viewer.user_id) {
                continue;
            }
            if (status >= 0 && info->getStatus() != status) {
                continue;
            }
            if (!search.empty() && info->getTitle().find(search) == std::string::npos
                && info->getDescription().find(search) == std::string::npos) {
                continue;
            }
            mine.push_back(info);
        }
        total = (int64_t)mine.size();
        for (uint64_t i = offset; i < mine.size() && out.size() < limit; ++i) {
            Item item;
            FillBase(item, mine[i], db);
            item.deadline = mine[i]->getDeadline();
            out.push_back(std::move(item));
        }
        return Result::Ok;
    }

    if (type == "bug") {
        std::vector<DB::BugInfo::ptr> all;
        BugRepository::ListByPages(all, 0, INT32_MAX, -1, true, db);
        std::vector<DB::BugInfo::ptr> mine;
        for (const auto& info : all) {
            if (org_id && info->getOrgId() != org_id) {
                continue;
            }
            if (info->getCreatorId() != viewer.user_id && info->getAssigneeId() != viewer.user_id) {
                continue;
            }
            if (status >= 0 && info->getStatus() != status) {
                continue;
            }
            if (!search.empty() && info->getTitle().find(search) == std::string::npos
                && info->getDescription().find(search) == std::string::npos) {
                continue;
            }
            mine.push_back(info);
        }
        total = (int64_t)mine.size();
        for (uint64_t i = offset; i < mine.size() && out.size() < limit; ++i) {
            Item item;
            FillBase(item, mine[i], db);
            item.severity = mine[i]->getSeverity();
            item.module = mine[i]->getModule();
            out.push_back(std::move(item));
        }
        return Result::Ok;
    }

    if (type == "task") {
        std::vector<DB::TaskInfo::ptr> all;
        TaskRepository::ListByPages(all, 0, INT32_MAX, -1, true, db);
        std::vector<DB::TaskInfo::ptr> mine;
        for (const auto& info : all) {
            if (org_id && info->getOrgId() != org_id) {
                continue;
            }
            if (info->getCreatorId() != viewer.user_id && info->getAssigneeId() != viewer.user_id) {
                continue;
            }
            if (status >= 0 && info->getStatus() != status) {
                continue;
            }
            if (!search.empty() && info->getTitle().find(search) == std::string::npos) {
                continue;
            }
            mine.push_back(info);
        }
        total = (int64_t)mine.size();
        for (uint64_t i = offset; i < mine.size() && out.size() < limit; ++i) {
            Item item;
            FillBase(item, mine[i], db);
            item.deadline = mine[i]->getDeadline();
            out.push_back(std::move(item));
        }
        return Result::Ok;
    }

    return Result::InvalidType;
}
