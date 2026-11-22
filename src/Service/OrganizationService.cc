#include "OrganizationService.hpp"

#include <climits>
#include <memory>
#include <utility>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Access/AccessPolicy.hpp"
#include "../Access/AccessScopeResolver.hpp"
#include "../Domain/OrganizationTypes.hpp"
#include "../Repository/OrganizationApplyRepository.hpp"
#include "../Repository/OrganizationRepository.hpp"
#include "../Repository/OrganizationUserRelRepository.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

namespace {
constexpr int32_t kOrgInactive = 0;
constexpr int32_t kOrgActive = 1;

constexpr int32_t kMemberPending = 0;
constexpr int32_t kMemberApproved = 1;
constexpr int32_t kMemberExited = 3;

constexpr int32_t kRoleMember = 1;
constexpr int32_t kRoleOrgAdmin = 5;

constexpr int32_t kApplyPending = 0;
constexpr int32_t kApplyApproved = 1;
constexpr int32_t kApplyRejected = 2;
} // namespace

bool OrganizationService::ListUserOrganizations(const Viewer& viewer, std::vector<UserOrganizationView>& out)
{
    out.clear();
    if (!viewer.valid()) {
        return false;
    }
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<std::pair<int64_t, int32_t>> grants;
    if (!OrganizationQueryRepository::ListUserOrgGrants(grants, viewer.user_id, db)) {
        return false;
    }
    for (const auto& [org_id, role] : grants) {
        if (role < OrgRole::REPORTER) {
            continue;
        }
        auto org = OrganizationRepository::Get(org_id, db);
        if (!org || org->getIsDeleted()) {
            continue;
        }
        out.push_back({org->getId(), org->getName(), role});
    }
    return true;
}

OrganizationService::ListResult OrganizationService::ListMembers(const Viewer& viewer,
    int64_t org_id,
    std::vector<OrganizationMemberView>& out)
{
    out.clear();
    if (!viewer.valid()) {
        return ListResult::AccessDenied;
    }
    auto db = GetDB();
    if (!db) {
        return ListResult::DbError;
    }

    OrgScope scope;
    if (!AccessScopeResolver::ResolveOrgs(scope, viewer, db)) {
        return ListResult::DbError;
    }
    if (!scope.allows(org_id, OrgRole::MEMBER)) {
        return ListResult::AccessDenied;
    }

    std::vector<DB::OrganizationUserRelInfo::ptr> members;
    OrganizationUserRelRepository::ListByOrgPages(members, org_id, 0, INT32_MAX, OrgMemberStatus::APPROVED, true, db);
    for (const auto& member : members) {
        if (!member || member->getRole() < OrgRole::REPORTER) {
            continue;
        }
        auto user = UserRepository::Get(member->getUserId(), db);
        if (!user) {
            continue;
        }
        out.push_back({member->getUserId(), user->getName(), user->getAvatar(), member->getRole()});
    }
    return ListResult::Ok;
}

OrganizationService::Result OrganizationService::CreateOrUpdate(const Viewer& viewer,
    const OrganizationInput& input,
    DB::OrganizationInfo::ptr& out,
    bool& created)
{
    out.reset();
    created = false;
    if (!AccessPolicy::CanAdminOrganization(viewer)) {
        return Result::AccessDenied;
    }
    auto db = GetDB();
    if (!db) {
        return Result::DbError;
    }

    DB::OrganizationInfo::ptr info;
    if (input.id != 0) {
        info = OrganizationRepository::Get(input.id, db);
        if (!info) {
            return Result::NotFound;
        }
    } else {
        info = OrganizationRepository::GetByName(input.name, db);
    }

    std::string old_name = info ? info->getName() : std::string();
    int64_t now = chen::TimeUtil::GetCurrentSec();
    if (!info) {
        info = std::make_shared<DB::OrganizationInfo>();
        info->setName(input.name);
        info->setCreateTime(now);
        created = true;
    } else if (info->getIsDeleted()) {
        info->setCreateTime(now);
        created = true;
    }
    if (input.id != 0) {
        info->setName(input.name);
    }
    info->setType(input.type);
    info->setStatus(input.status);
    info->setOwnerId(input.owner_id);
    info->setDescription(input.description);
    info->setIsDeleted(0);
    info->setUpdateTime(now);

    auto trans = db->openTransaction();
    if (!trans) {
        return Result::DbError;
    }
    if (!OrganizationRepository::InsertOrUpdate(info, db)) {
        ERROR(logger) << "CreateOrUpdate insert failed, name=" << input.name;
        trans->rollback();
        return Result::DbError;
    }

    DB::OrganizationUserRelInfo::ptr rel;
    if (created) {
        rel = std::make_shared<DB::OrganizationUserRelInfo>();
        rel->setOrgId(info->getId());
        rel->setUserId(input.owner_id);
        rel->setRole(kRoleOrgAdmin);
        rel->setStatus(kMemberApproved);
        rel->setCreateTime(now);
        rel->setUpdateTime(now);
        if (!OrganizationUserRelRepository::InsertOrUpdate(rel, db)) {
            ERROR(logger) << "CreateOrUpdate owner rel failed, org=" << info->getId();
            trans->rollback();
            return Result::DbError;
        }
    }
    if (!trans->commit()) {
        ERROR(logger) << "CreateOrUpdate commit failed";
        return Result::DbError;
    }

    OrganizationRepository::Put(info);
    if (rel) {
        OrganizationUserRelRepository::Put(rel);
    }
    if (!old_name.empty()) {
        OrganizationRepository::EvictName(old_name);
    }
    OrganizationRepository::EvictName(info->getName());
    OrganizationQueryRepository::InvalidateStats();

    out = info;
    return Result::Ok;
}

OrganizationService::DeleteResult OrganizationService::DeleteMany(const Viewer& viewer,
    const std::vector<int64_t>& ids,
    std::vector<DB::OrganizationInfo::ptr>& orgs,
    std::vector<DB::OrganizationUserRelInfo::ptr>& rels)
{
    if (!AccessPolicy::CanAdminOrganization(viewer)) {
        return DeleteResult::AccessDenied;
    }
    if (ids.empty()) {
        return DeleteResult::Ok;
    }
    auto db = GetDB();
    if (!db) {
        return DeleteResult::DbError;
    }

    for (auto id : ids) {
        auto org = OrganizationRepository::Get(id, db);
        if (!org || org->getIsDeleted()) {
            continue;
        }
        orgs.push_back(org);
        OrganizationUserRelRepository::ListByOrg(rels, id, true, db);
    }
    if (orgs.empty()) {
        return DeleteResult::NoValidOrgs;
    }

    auto trans = db->openTransaction();
    if (!trans) {
        return DeleteResult::DbError;
    }
    int64_t now = chen::TimeUtil::GetCurrentSec();
    for (auto& org : orgs) {
        org->setIsDeleted(1);
        org->setUpdateTime(now);
        if (!OrganizationRepository::Update(org, db)) {
            ERROR(logger) << "DeleteMany org update failed, id=" << org->getId();
            trans->rollback();
            return DeleteResult::DbError;
        }
    }
    for (auto& rel : rels) {
        rel->setIsDeleted(1);
        rel->setUpdateTime(now);
        if (!OrganizationUserRelRepository::Update(rel, db)) {
            ERROR(logger) << "DeleteMany rel update failed, id=" << rel->getId();
            trans->rollback();
            return DeleteResult::DbError;
        }
    }
    if (!trans->commit()) {
        ERROR(logger) << "DeleteMany commit failed";
        return DeleteResult::DbError;
    }

    for (auto& org : orgs) {
        OrganizationRepository::Put(org);
    }
    for (auto& rel : rels) {
        OrganizationUserRelRepository::Put(rel);
    }
    OrganizationQueryRepository::InvalidateStats();
    return DeleteResult::Ok;
}

OrganizationService::Result OrganizationService::SubmitApply(int64_t uid,
    const std::string& name,
    const std::string& type,
    const std::string& description,
    DB::OrganizationApplyInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return Result::DbError;
    }

    auto info = std::make_shared<DB::OrganizationApplyInfo>();
    info->setUserId(uid);
    info->setOrgName(name);
    info->setOrgType(type);
    info->setOrgDescription(description);
    info->setStatus(kApplyPending);
    info->setCreatedAt(chen::TimeUtil::GetCurrentSec());
    info->setIsDeleted(0);

    if (!OrganizationApplyRepository::Insert(info, db)) {
        ERROR(logger) << "SubmitApply insert failed, uid=" << uid;
        return Result::DbError;
    }
    OrganizationApplyRepository::Put(info);
    out = info;
    return Result::Ok;
}

OrganizationService::Result OrganizationService::ReviewApply(const Viewer& viewer,
    int64_t apply_id,
    bool approve,
    const std::string& reason,
    DB::OrganizationInfo::ptr& org_out,
    DB::OrganizationApplyInfo::ptr& apply_out)
{
    org_out.reset();
    apply_out.reset();
    if (!AccessPolicy::CanAdminOrganization(viewer)) {
        return Result::AccessDenied;
    }
    auto db = GetDB();
    if (!db) {
        return Result::DbError;
    }
    auto apply = OrganizationApplyRepository::Get(apply_id, db);
    if (!apply || apply->getIsDeleted()) {
        return Result::NotFound;
    }
    if (apply->getStatus() != kApplyPending) {
        return Result::Conflict;
    }

    auto trans = db->openTransaction();
    if (!trans) {
        return Result::DbError;
    }

    int64_t now = chen::TimeUtil::GetCurrentSec();
    DB::OrganizationInfo::ptr org;
    DB::OrganizationUserRelInfo::ptr rel;
    if (approve) {
        org = std::make_shared<DB::OrganizationInfo>();
        org->setName(apply->getOrgName());
        org->setType(apply->getOrgType());
        org->setDescription(apply->getOrgDescription());
        org->setOwnerId(apply->getUserId());
        org->setStatus(kOrgActive);
        org->setIsDeleted(0);
        org->setCreateTime(now);
        org->setUpdateTime(now);
        if (!OrganizationRepository::Insert(org, db)) {
            ERROR(logger) << "ReviewApply insert org failed, apply_id=" << apply_id;
            trans->rollback();
            return Result::DbError;
        }

        rel = std::make_shared<DB::OrganizationUserRelInfo>();
        rel->setOrgId(org->getId());
        rel->setUserId(apply->getUserId());
        rel->setRole(kRoleOrgAdmin);
        rel->setStatus(kMemberApproved);
        rel->setCreateTime(now);
        rel->setUpdateTime(now);
        if (!OrganizationUserRelRepository::InsertOrUpdate(rel, db)) {
            ERROR(logger) << "ReviewApply owner rel failed, apply_id=" << apply_id;
            trans->rollback();
            return Result::DbError;
        }
        apply->setStatus(kApplyApproved);
    } else {
        apply->setStatus(kApplyRejected);
    }
    apply->setReviewReason(reason);
    apply->setReviewedAt(now);
    if (!OrganizationApplyRepository::Update(apply, db)) {
        ERROR(logger) << "ReviewApply update apply failed, apply_id=" << apply_id;
        trans->rollback();
        return Result::DbError;
    }
    if (!trans->commit()) {
        ERROR(logger) << "ReviewApply commit failed, apply_id=" << apply_id;
        return Result::DbError;
    }

    OrganizationApplyRepository::Put(apply);
    if (org) {
        OrganizationRepository::Put(org);
        org_out = org;
    }
    if (rel) {
        OrganizationUserRelRepository::Put(rel);
    }
    if (approve) {
        OrganizationQueryRepository::InvalidateStats();
    }
    apply_out = apply;
    return Result::Ok;
}

OrganizationService::JoinResult OrganizationService::Join(int64_t org_id,
    int64_t uid,
    DB::OrganizationUserRelInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return JoinResult::DbError;
    }
    auto org = OrganizationRepository::Get(org_id, db);
    if (!org || org->getIsDeleted()) {
        return JoinResult::OrgNotFound;
    }
    if (org->getStatus() == kOrgInactive) {
        return JoinResult::OrgDisabled;
    }

    auto info = OrganizationUserRelRepository::GetByOrgAndUser(org_id, uid, db);
    if (info) {
        if (info->getStatus() == kMemberApproved) {
            return JoinResult::AlreadyJoined;
        }
        if (info->getStatus() == kMemberPending) {
            return JoinResult::AlreadyApplied;
        }
    } else {
        info = std::make_shared<DB::OrganizationUserRelInfo>();
        info->setOrgId(org_id);
        info->setUserId(uid);
        info->setCreateTime(chen::TimeUtil::GetCurrentSec());
    }
    info->setRole(kRoleMember);
    info->setStatus(kMemberPending);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!OrganizationUserRelRepository::InsertOrUpdate(info, db)) {
        ERROR(logger) << "Join upsert failed, org=" << org_id << " uid=" << uid;
        return JoinResult::DbError;
    }
    OrganizationUserRelRepository::Put(info);
    out = info;
    return JoinResult::Ok;
}

OrganizationService::MemberResult OrganizationService::SetMemberState(const Viewer& viewer,
    int64_t org_id,
    int64_t uid,
    int32_t state,
    DB::OrganizationUserRelInfo::ptr& out)
{
    out.reset();
    if (!viewer.valid()) {
        return MemberResult::AccessDenied;
    }
    auto db = GetDB();
    if (!db) {
        return MemberResult::DbError;
    }
    OrgScope scope;
    if (!AccessScopeResolver::ResolveOrgs(scope, viewer, db)) {
        return MemberResult::DbError;
    }
    if (!viewer.is_platform_admin && !scope.allows(org_id, OrgRole::MEMBER)) {
        return MemberResult::NotMember;
    }
    if (!AccessPolicy::CanManageOrgMembers(viewer, scope, org_id)) {
        return MemberResult::AccessDenied;
    }

    auto rel = OrganizationUserRelRepository::GetByOrgAndUser(org_id, uid, db);
    if (!rel || rel->getIsDeleted()) {
        return MemberResult::RelNotFound;
    }
    if (rel->getStatus() != kMemberPending) {
        return MemberResult::InvalidState;
    }
    rel->setStatus(state);
    rel->setRole(kRoleMember);
    rel->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!OrganizationUserRelRepository::InsertOrUpdate(rel, db)) {
        ERROR(logger) << "SetMemberState upsert failed, org=" << org_id << " uid=" << uid;
        return MemberResult::DbError;
    }
    OrganizationUserRelRepository::Put(rel);
    out = rel;
    return MemberResult::Ok;
}

OrganizationService::MemberResult OrganizationService::Kick(const Viewer& viewer,
    int64_t org_id,
    int64_t uid,
    DB::OrganizationUserRelInfo::ptr& out)
{
    out.reset();
    if (!viewer.valid()) {
        return MemberResult::AccessDenied;
    }
    auto db = GetDB();
    if (!db) {
        return MemberResult::DbError;
    }
    OrgScope scope;
    if (!AccessScopeResolver::ResolveOrgs(scope, viewer, db)) {
        return MemberResult::DbError;
    }
    if (!viewer.is_platform_admin && !scope.allows(org_id, OrgRole::MEMBER)) {
        return MemberResult::NotMember;
    }
    if (!AccessPolicy::CanManageOrgMembers(viewer, scope, org_id)) {
        return MemberResult::AccessDenied;
    }

    auto rel = OrganizationUserRelRepository::GetByOrgAndUser(org_id, uid, db);
    if (!rel || rel->getIsDeleted()) {
        return MemberResult::RelNotFound;
    }
    rel->setStatus(kMemberExited);
    rel->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!OrganizationUserRelRepository::InsertOrUpdate(rel, db)) {
        ERROR(logger) << "Kick upsert failed, org=" << org_id << " uid=" << uid;
        return MemberResult::DbError;
    }
    OrganizationUserRelRepository::Put(rel);
    out = rel;
    return MemberResult::Ok;
}

OrganizationService::MemberResult OrganizationService::SwitchRole(const Viewer& viewer,
    int64_t rel_id,
    int64_t org_id,
    int64_t uid,
    int32_t role,
    DB::OrganizationUserRelInfo::ptr& out)
{
    out.reset();
    if (!viewer.valid()) {
        return MemberResult::AccessDenied;
    }
    auto db = GetDB();
    if (!db) {
        return MemberResult::DbError;
    }
    OrgScope scope;
    if (!AccessScopeResolver::ResolveOrgs(scope, viewer, db)) {
        return MemberResult::DbError;
    }
    if (!viewer.is_platform_admin && !scope.allows(org_id, OrgRole::MEMBER)) {
        return MemberResult::NotMember;
    }
    if (!AccessPolicy::CanSwitchOrgRole(viewer, scope, org_id, role)) {
        return MemberResult::AccessDenied;
    }

    auto rel = OrganizationUserRelRepository::Get(rel_id, db);
    if (!rel || rel->getOrgId() != org_id || rel->getUserId() != uid) {
        return MemberResult::RelNotFound;
    }
    rel->setRole(role);
    rel->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!OrganizationUserRelRepository::InsertOrUpdate(rel, db)) {
        ERROR(logger) << "SwitchRole upsert failed, rel=" << rel_id;
        return MemberResult::DbError;
    }
    OrganizationUserRelRepository::Put(rel);
    out = rel;
    return MemberResult::Ok;
}
