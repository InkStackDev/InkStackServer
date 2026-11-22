#include "RequirementService.hpp"

#include <memory>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Permission.hpp"
#include "../Repository/RequirementRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

RequirementService::SaveResult RequirementService::Save(const RequirementInput& input, DB::RequirementInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return SaveResult::DbError;
    }

    DB::RequirementInfo::ptr info;
    if (input.id != 0) {
        info = RequirementRepository::Get(input.id, db);
        if (!info || info->getOrgId() != input.org_id) {
            return SaveResult::NotFound;
        }
    } else {
        info = std::make_shared<DB::RequirementInfo>();
        info->setOrgId(input.org_id);
        info->setCreatorId(input.creator_id);
        info->setCreateTime(chen::TimeUtil::GetCurrentSec());
    }

    if (!input.title.empty()) {
        info->setTitle(input.title);
    }
    if (!input.description.empty()) {
        info->setDescription(input.description);
    }
    if (input.priority >= 0) {
        info->setPriority(input.priority);
    }
    if (input.status >= 0) {
        info->setStatus(input.status);
    }
    if (input.assignee_id != 0) {
        info->setAssigneeId(input.assignee_id);
    }
    if (input.deadline != 0) {
        info->setDeadline(input.deadline);
    }
    if (!input.iteration.empty()) {
        info->setIteration(input.iteration);
    }
    if (!input.category.empty()) {
        info->setCategory(input.category);
    }
    if (!input.source.empty()) {
        info->setSource(input.source);
    }
    info->setIsDeleted(0);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());

    if (!RequirementRepository::InsertOrUpdate(info, db)) {
        ERROR(logger) << "RequirementService::Save failed, id=" << input.id << " org=" << input.org_id;
        return SaveResult::DbError;
    }
    out = info;
    return SaveResult::Ok;
}

bool RequirementService::DeleteMany(int64_t org_id,
    const std::set<int64_t>& ids,
    int64_t uid,
    int32_t org_role,
    bool is_platform_admin,
    std::vector<DB::RequirementInfo::ptr>& deleted)
{
    if (ids.empty()) {
        return true;
    }
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<DB::RequirementInfo::ptr> all;
    RequirementRepository::ListByOrg(all, org_id, 0, INT32_MAX, -1, true, db);
    for (auto& info : all) {
        if (!ids.count(info->getId())) {
            continue;
        }
        if (!is_platform_admin && uid != info->getCreatorId() && !CanDeleteRequirement(org_role)) {
            continue;
        }
        deleted.push_back(info);
    }
    if (deleted.empty()) {
        return true;
    }

    auto trans = db->openTransaction();
    if (!trans) {
        return false;
    }
    int64_t now = chen::TimeUtil::GetCurrentSec();
    for (auto& info : deleted) {
        info->setIsDeleted(1);
        info->setUpdateTime(now);
        if (!RequirementRepository::Update(info, db)) {
            ERROR(logger) << "RequirementService::DeleteMany update failed, id=" << info->getId();
            trans->rollback();
            return false;
        }
    }
    if (!trans->commit()) {
        ERROR(logger) << "RequirementService::DeleteMany commit failed";
        return false;
    }
    return true;
}
