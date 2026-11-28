#include "AssignmentService.hpp"

#include <memory>
#include <utility>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Repository/AssignmentRelRepository.hpp"
#include "../Repository/AssignmentRepository.hpp"
#include "../Repository/ResourceRepository.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

/**
 * @brief 按 input 填充作业字段；返回是否新建/复活了记录
 */
static bool ApplyInput(const AssignmentInput& input, DB::AssignmentInfo::ptr& info)
{
    bool created = false;
    int64_t now = chen::TimeUtil::GetCurrentSec();
    if (input.id == 0) {
        if (!info) {
            info = std::make_shared<DB::AssignmentInfo>();
            info->setName(input.name);
            info->setSubjectName(input.subject_name);
            info->setCreateTime(now);
            created = true;
        } else if (info->getIsDeleted()) {
            info->setCreateTime(now);
            created = true;
        }
    }

    if (input.id != 0) {
        info->setName(input.name);
        info->setSubjectName(input.subject_name);
    }
    info->setDescription(input.description);
    info->setFileType(input.file_type);
    info->setDeadline(input.deadline);
    info->setMaxSize(input.max_size);
    info->setStatus(input.status);
    info->setPriority(input.priority);
    info->setIsDeleted(0);
    info->setUpdateTime(now);
    return created;
}

AssignmentService::CreateResult AssignmentService::CreateOrUpdate(const AssignmentInput& input,
    DB::AssignmentInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return CreateResult::DbError;
    }

    DB::AssignmentInfo::ptr info;
    if (input.id != 0) {
        info = AssignmentRepository::Get(input.id, db);
        if (!info) {
            return CreateResult::NotFound;
        }
    } else {
        info = AssignmentRepository::GetByName(input.subject_name, input.name, db);
    }

    std::string old_subject = info ? info->getSubjectName() : std::string();
    std::string old_name = info ? info->getName() : std::string();

    ApplyInput(input, info);

    if (!AssignmentRepository::InsertOrUpdate(info, db)) {
        ERROR(logger) << "CreateOrUpdate failed, name=" << input.name;
        return CreateResult::DbError;
    }
    AssignmentRepository::Put(info);
    if (!old_name.empty()) {
        AssignmentRepository::EvictName(old_subject, old_name);
    }
    AssignmentRepository::EvictName(input.subject_name, input.name);

    out = info;
    return CreateResult::Ok;
}

AssignmentService::OrgCreateResult AssignmentService::CreateForOrganization(int64_t org_id,
    const std::string& assigned_by,
    const AssignmentInput& input,
    DB::AssignmentInfo::ptr& out,
    bool& created)
{
    out.reset();
    created = false;
    auto db = GetDB();
    if (!db) {
        return OrgCreateResult::DbError;
    }

    DB::AssignmentInfo::ptr info;
    if (input.id != 0) {
        info = AssignmentRepository::Get(input.id, db);
        if (!info) {
            return OrgCreateResult::NotFound;
        }
    } else {
        info = AssignmentRepository::GetByName(input.subject_name, input.name, db);
    }

    std::string old_subject = info ? info->getSubjectName() : std::string();
    std::string old_name = info ? info->getName() : std::string();

    created = ApplyInput(input, info);

    if (!AssignmentRepository::InsertOrUpdate(info, db)) {
        ERROR(logger) << "CreateForOrganization assignment upsert failed, name=" << input.name;
        return OrgCreateResult::DbError;
    }
    AssignmentRepository::Put(info);
    if (!old_name.empty()) {
        AssignmentRepository::EvictName(old_subject, old_name);
    }
    AssignmentRepository::EvictName(input.subject_name, input.name);

    if (created) {
        auto rel = AssignmentOrganizationRelRepository::GetByOrgAndAssign(org_id, info->getId(), db);
        if (!rel) {
            rel = std::make_shared<DB::AssignmentOrganizationRelInfo>();
            rel->setOrganizationId(org_id);
            rel->setAssignmentId(info->getId());
            rel->setCreateTime(chen::TimeUtil::GetCurrentSec());
        }
        rel->setAssignedBy(assigned_by);
        rel->setIsDeleted(0);
        rel->setUpdateTime(chen::TimeUtil::GetCurrentSec());
        if (!AssignmentOrganizationRelRepository::InsertOrUpdate(rel, db)) {
            ERROR(logger) << "CreateForOrganization rel upsert failed, org_id=" << org_id;
            return OrgCreateResult::DbError;
        }
    }

    out = info;
    return OrgCreateResult::Ok;
}

bool AssignmentService::DeleteMany(const std::vector<int64_t>& ids, std::vector<DB::AssignmentInfo::ptr>& deleted)
{
    if (ids.empty()) {
        return true;
    }
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto trans = db->openTransaction();
    if (!trans) {
        return false;
    }

    int64_t now = chen::TimeUtil::GetCurrentSec();
    for (auto id : ids) {
        auto info = AssignmentRepository::Get(id, db);
        if (!info || info->getIsDeleted()) {
            continue;
        }
        info->setIsDeleted(1);
        info->setUpdateTime(now);
        if (!AssignmentRepository::Update(info, db)) {
            ERROR(logger) << "DeleteMany update failed, id=" << id;
            trans->rollback();
            return false;
        }
        deleted.push_back(info);
    }
    if (!trans->commit()) {
        ERROR(logger) << "DeleteMany commit failed";
        return false;
    }
    for (auto& info : deleted) {
        AssignmentRepository::Put(info);
    }
    return true;
}

AssignmentService::ViewResult AssignmentService::GetDetail(int64_t id, DetailView& out)
{
    out = DetailView{};
    auto db = GetDB();
    if (!db) {
        return ViewResult::DbError;
    }
    auto info = AssignmentRepository::Get(id, db);
    if (!info || info->getIsDeleted()) {
        return ViewResult::NotFound;
    }
    out.id = info->getId();
    out.name = info->getName();
    out.subject_name = info->getSubjectName();
    out.description = info->getDescription();
    out.file_type = info->getFileType();
    out.priority = info->getPriority();
    out.status = info->getStatus();
    out.max_size = info->getMaxSize();
    out.deadline = info->getDeadline();
    out.create_time = info->getCreateTime();
    out.update_time = info->getUpdateTime();
    return ViewResult::Ok;
}

AssignmentService::ViewResult AssignmentService::GetSubmission(int64_t assignment_id, int64_t uid, SubmissionView& out)
{
    out = SubmissionView{};
    auto db = GetDB();
    if (!db) {
        return ViewResult::DbError;
    }
    auto assignment = AssignmentRepository::Get(assignment_id, db);
    if (!assignment || assignment->getIsDeleted()) {
        return ViewResult::NotFound;
    }

    auto rel = AssignmentUserRelRepository::GetByAssignAndUser(assignment_id, uid, db);
    if (!rel) {
        return ViewResult::Ok;
    }
    out.has_submission = true;
    out.user_id = uid;
    out.submit_time = rel->getSubmitTime();
    out.status = rel->getStatus();

    auto user = UserRepository::Get(uid, db);
    if (user) {
        out.user_name = user->getName();
        out.user_avatar = user->getAvatar();
    }

    std::vector<DB::ResourceInfo::ptr> resources;
    ResourceRepository::ListByBizUid(resources, "assignment_submission", assignment_id, uid, db);
    for (const auto& resource : resources) {
        ResourceView item;
        item.id = resource->getId();
        item.name = resource->getName();
        item.path = resource->getPath();
        item.size = resource->getSize();
        out.resources.push_back(std::move(item));
    }
    return ViewResult::Ok;
}

void AssignmentService::ListSubmitters(int64_t assignment_id, std::vector<int64_t>& out)
{
    out.clear();
    auto db = GetDB();
    if (!db) {
        return;
    }
    std::vector<DB::AssignmentUserRelInfo::ptr> submitters;
    AssignmentUserRelRepository::ListByAssignment(submitters, assignment_id, true, db);
    for (const auto& info : submitters) {
        if (info) {
            out.push_back(info->getUserId());
        }
    }
}
