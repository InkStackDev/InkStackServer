#include "BugService.hpp"

#include <memory>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Permission.hpp"
#include "../Repository/BugRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

BugService::SaveResult BugService::Save(const BugInput& input, DB::BugInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return SaveResult::DbError;
    }

    DB::BugInfo::ptr info;
    if (input.id != 0) {
        info = BugRepository::Get(input.id, db);
        if (!info || info->getOrgId() != input.org_id) {
            return SaveResult::NotFound;
        }
    } else {
        info = std::make_shared<DB::BugInfo>();
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
    if (input.severity >= 0) {
        info->setSeverity(input.severity);
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
    if (input.requirement_id != 0) {
        info->setRequirementId(input.requirement_id);
    }
    if (!input.module.empty()) {
        info->setModule(input.module);
    }
    if (!input.steps_to_reproduce.empty()) {
        info->setStepsToReproduce(input.steps_to_reproduce);
    }
    if (!input.environment.empty()) {
        info->setEnvironment(input.environment);
    }
    info->setIsDeleted(0);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());

    if (!BugRepository::InsertOrUpdate(info, db)) {
        ERROR(logger) << "BugService::Save failed, id=" << input.id << " org=" << input.org_id;
        return SaveResult::DbError;
    }
    out = info;
    return SaveResult::Ok;
}

bool BugService::DeleteMany(int64_t org_id,
    const std::set<int64_t>& ids,
    int64_t uid,
    int32_t org_role,
    bool is_platform_admin,
    std::vector<DB::BugInfo::ptr>& deleted)
{
    if (ids.empty()) {
        return true;
    }
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<DB::BugInfo::ptr> all;
    BugRepository::ListByOrg(all, org_id, 0, INT32_MAX, -1, true, db);
    for (auto& info : all) {
        if (!ids.count(info->getId())) {
            continue;
        }
        if (!is_platform_admin && uid != info->getCreatorId() && !CanDeleteBug(org_role)) {
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
        if (!BugRepository::Update(info, db)) {
            ERROR(logger) << "BugService::DeleteMany update failed, id=" << info->getId();
            trans->rollback();
            return false;
        }
    }
    if (!trans->commit()) {
        ERROR(logger) << "BugService::DeleteMany commit failed";
        return false;
    }
    return true;
}
