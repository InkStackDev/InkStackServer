#include "AdminAssignmentService.hpp"

#include "../Repository/AssignmentQueryRepository.hpp"
#include "../Repository/AssignmentRelRepository.hpp"
#include "../Repository/OrganizationRepository.hpp"
#include "../Util/Util.hpp"

#include <utility>

bool AdminAssignmentService::List(uint64_t offset,
    uint64_t page_size,
    int32_t state,
    std::vector<Item>& out,
    int64_t& total)
{
    out.clear();
    total = 0;
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<DB::AssignmentInfo::ptr> infos;
    total = (int64_t)AssignmentQueryRepository::ListByPages(infos, offset, page_size, state, true, db);
    for (const auto& info : infos) {
        if (!info) {
            continue;
        }
        Item item;
        item.id = info->getId();
        item.name = info->getName();
        item.subject_name = info->getSubjectName();
        item.end_time = info->getDeadline();
        item.status = info->getStatus();
        item.priority = info->getPriority();
        item.create_time = info->getCreateTime();
        item.description = info->getDescription();
        item.file_type = info->getFileType();
        item.file_size = info->getMaxSize();

        std::vector<DB::AssignmentOrganizationRelInfo::ptr> org_rels;
        AssignmentOrganizationRelRepository::ListByAssignment(org_rels, info->getId(), db);
        if (!org_rels.empty() && org_rels[0]) {
            auto org = OrganizationRepository::Get(org_rels[0]->getOrganizationId(), db);
            if (org) {
                item.organization_name = org->getName();
            }
            item.assigned_by = org_rels[0]->getAssignedBy();
        }
        out.push_back(std::move(item));
    }
    return true;
}

bool AdminAssignmentService::GetStats(Stats& out)
{
    out = Stats{};
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto stats = AssignmentQueryRepository::GetStats(db);
    out.total = stats.total;
    out.active = stats.active;
    out.closed = stats.closed;
    out.draft = stats.draft;
    return true;
}
