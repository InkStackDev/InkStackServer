#include "TaskService.hpp"

#include <memory>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Permission.hpp"
#include "../Repository/TaskRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

TaskService::SaveResult TaskService::Save(const TaskInput& input, DB::TaskInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return SaveResult::DbError;
    }

    DB::TaskInfo::ptr info;
    if (input.id != 0) {
        info = TaskRepository::Get(input.id, db);
        if (!info || info->getOrgId() != input.org_id) {
            return SaveResult::NotFound;
        }
    } else {
        info = std::make_shared<DB::TaskInfo>();
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
    if (input.requirement_id != 0) {
        info->setRequirementId(input.requirement_id);
    }
    if (input.bug_id != 0) {
        info->setBugId(input.bug_id);
    }
    if (input.deadline != 0) {
        info->setDeadline(input.deadline);
    }
    if (input.estimated_hours != 0) {
        info->setEstimatedHours(input.estimated_hours);
    }
    info->setIsDeleted(0);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());

    if (!TaskRepository::InsertOrUpdate(info, db)) {
        ERROR(logger) << "TaskService::Save failed, id=" << input.id << " org=" << input.org_id;
        return SaveResult::DbError;
    }
    out = info;
    return SaveResult::Ok;
}

bool TaskService::DeleteMany(int64_t org_id,
    const std::set<int64_t>& ids,
    int64_t uid,
    int32_t org_role,
    bool is_platform_admin,
    std::vector<DB::TaskInfo::ptr>& deleted)
{
    if (ids.empty()) {
        return true;
    }
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<DB::TaskInfo::ptr> all;
    TaskRepository::ListByOrg(all, org_id, 0, INT32_MAX, -1, true, db);
    for (auto& info : all) {
        if (!ids.count(info->getId())) {
            continue;
        }
        if (!is_platform_admin && uid != info->getCreatorId() && !CanDeleteTask(org_role)) {
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
        if (!TaskRepository::Update(info, db)) {
            ERROR(logger) << "TaskService::DeleteMany update failed, id=" << info->getId();
            trans->rollback();
            return false;
        }
    }
    if (!trans->commit()) {
        ERROR(logger) << "TaskService::DeleteMany commit failed";
        return false;
    }
    return true;
}
