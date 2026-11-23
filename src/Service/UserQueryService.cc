#include "UserQueryService.hpp"

#include "../Domain/ArticleTypes.hpp"
#include "../Domain/OrganizationTypes.hpp"
#include "../Domain/RdTypes.hpp"
#include "../Domain/UserTypes.hpp"
#include "../Repository/AssignmentRelRepository.hpp"
#include "../Repository/AssignmentRepository.hpp"
#include "../Repository/OrganizationRepository.hpp"
#include "../Repository/OrganizationUserRelRepository.hpp"
#include "../Repository/UserFollowRelRepository.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Util/Util.hpp"

#include <Chen/Util/StringUtil.hpp>

#include <algorithm>
#include <climits>
#include <functional>
#include <utility>

UserQueryService::ExistsResult UserQueryService::Exists(const std::string& auth_id, bool& exists)
{
    exists = false;
    auto db = GetDB();
    if (!db) {
        return ExistsResult::DbError;
    }

    DB::UserInfo::ptr info;
    if (IsEmail(auth_id)) {
        info = UserRepository::GetByEmail(auth_id, db);
    } else if (IsValidAccount(auth_id)) {
        info = UserRepository::GetByAccount(auth_id, db);
    } else {
        return ExistsResult::InvalidAuthId;
    }
    exists = info != nullptr;
    return ExistsResult::Ok;
}

void UserQueryService::ListAllIds(std::vector<int64_t>& ids)
{
    ids.clear();
    auto db = GetDB();
    if (!db) {
        return;
    }
    UserRepository::GetAllIds(ids, false, db);
    std::sort(ids.begin(), ids.end(), std::less<>());
}

void UserQueryService::ListActiveIds(std::vector<int64_t>& ids)
{
    ids.clear();
    auto db = GetDB();
    if (!db) {
        return;
    }
    UserRepository::GetAllIds(ids, true, db);
}

void UserQueryService::QueryBrief(const std::vector<int64_t>& ids, std::vector<Brief>& out)
{
    out.clear();
    auto db = GetDB();
    if (!db) {
        return;
    }
    for (auto id : ids) {
        auto user = UserRepository::Get(id, db);
        if (!user) {
            continue;
        }
        Brief brief;
        brief.id = user->getId();
        brief.name = user->getName();
        brief.account = user->getAccount();
        brief.avatar = user->getAvatar();
        out.push_back(std::move(brief));
    }
}

bool UserQueryService::GetProfile(int64_t viewer_uid, int64_t target_uid, Profile& out)
{
    out = Profile{};
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto info = UserRepository::Get(target_uid, db);
    if (!info) {
        return false;
    }

    out.id = info->getId();
    out.name = info->getName();
    out.account = info->getAccount();
    out.avatar = info->getAvatar();
    out.email = info->getEmail();
    out.role = info->getRole();
    out.bio = info->getBio();
    out.website = info->getWebsite();
    out.location = info->getLocation();
    out.status = info->getIsDeleted();
    out.github = info->getGithub();
    out.login_time = info->getLoginTime();
    out.create_time = info->getCreateTime();
    out.following_count = UserFollowRelRepository::CountFollowing(target_uid, db);
    out.follower_count = UserFollowRelRepository::CountFollowers(target_uid, db);

    out.include_email = (target_uid == viewer_uid);
    if (!out.include_email) {
        auto viewer = UserRepository::Get(viewer_uid, db);
        out.include_email = viewer && viewer->getRole() == UserRole::ADMIN;
    }
    return true;
}

bool UserQueryService::GetUserBrief(int64_t user_id, Brief& out)
{
    out = Brief{};
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto user = UserRepository::Get(user_id, db);
    if (!user || user->getIsDeleted()) {
        return false;
    }
    out.id = user->getId();
    out.name = user->getName();
    out.account = user->getAccount();
    out.avatar = user->getAvatar();
    return true;
}

bool UserQueryService::GetRole(int64_t user_id, int32_t& role, bool& deleted)
{
    role = 0;
    deleted = false;
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto user = UserRepository::Get(user_id, db);
    if (!user) {
        return false;
    }
    role = user->getRole();
    deleted = user->getIsDeleted() != 0;
    return true;
}

bool UserQueryService::ListFollowers(int64_t target_uid,
    int32_t offset,
    int32_t size,
    std::vector<FollowUser>& out,
    int64_t& total)
{
    out.clear();
    total = 0;
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<DB::UserFollowRelInfo::ptr> rels;
    UserFollowRelRepository::ListFollowers(rels, target_uid, offset, size, db);
    for (const auto& rel : rels) {
        auto user = UserRepository::Get(rel->getFollowerId(), db);
        if (!user || user->getIsDeleted()) {
            continue;
        }
        FollowUser item;
        item.id = user->getId();
        item.name = user->getName();
        item.account = user->getAccount();
        item.avatar = user->getAvatar();
        item.bio = user->getBio();
        item.following_count = UserFollowRelRepository::CountFollowing(item.id, db);
        item.follower_count = UserFollowRelRepository::CountFollowers(item.id, db);
        item.create_time = rel->getCreateTime();
        out.push_back(std::move(item));
    }
    total = UserFollowRelRepository::CountFollowers(target_uid, db);
    return true;
}

bool UserQueryService::ListFollowing(int64_t target_uid,
    int32_t offset,
    int32_t size,
    std::vector<FollowUser>& out,
    int64_t& total)
{
    out.clear();
    total = 0;
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<DB::UserFollowRelInfo::ptr> rels;
    UserFollowRelRepository::ListFollowing(rels, target_uid, offset, size, db);
    for (const auto& rel : rels) {
        auto user = UserRepository::Get(rel->getFollowingId(), db);
        if (!user || user->getIsDeleted()) {
            continue;
        }
        FollowUser item;
        item.id = user->getId();
        item.name = user->getName();
        item.account = user->getAccount();
        item.avatar = user->getAvatar();
        item.bio = user->getBio();
        item.following_count = UserFollowRelRepository::CountFollowing(item.id, db);
        item.follower_count = UserFollowRelRepository::CountFollowers(item.id, db);
        item.create_time = rel->getCreateTime();
        out.push_back(std::move(item));
    }
    total = UserFollowRelRepository::CountFollowing(target_uid, db);
    return true;
}

bool UserQueryService::ListMutual(int64_t uid,
    const std::string& keyword,
    int32_t offset,
    int32_t size,
    std::vector<Brief>& out,
    int64_t& total)
{
    out.clear();
    total = 0;
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::string kw_lower = chen::StringUtil::ToLower(keyword);
    std::vector<int64_t> mutual_ids;
    UserFollowRelRepository::ListMutualFollowing(mutual_ids, uid, db);

    std::vector<int64_t> filtered;
    filtered.reserve(mutual_ids.size());
    for (auto user_id : mutual_ids) {
        auto user = UserRepository::Get(user_id, db);
        if (!user || user->getIsDeleted()) {
            continue;
        }
        if (!kw_lower.empty()) {
            std::string name_lower = chen::StringUtil::ToLower(user->getName());
            std::string account_lower = chen::StringUtil::ToLower(user->getAccount());
            if (name_lower.find(kw_lower) == std::string::npos && account_lower.find(kw_lower) == std::string::npos) {
                continue;
            }
        }
        filtered.push_back(user_id);
    }

    total = (int64_t)filtered.size();
    int32_t start = offset < 0 ? 0 : offset;
    int32_t end = std::min(start + size, (int32_t)filtered.size());
    if (start > (int32_t)filtered.size()) {
        start = (int32_t)filtered.size();
        end = start;
    }

    for (int32_t i = start; i < end; ++i) {
        auto user = UserRepository::Get(filtered[i], db);
        if (!user) {
            continue;
        }
        Brief item;
        item.id = user->getId();
        item.name = user->getName();
        item.account = user->getAccount();
        item.avatar = user->getAvatar();
        out.push_back(std::move(item));
    }
    return true;
}

bool UserQueryService::ListOrganizations(int64_t uid, std::vector<OrganizationItem>& out)
{
    out.clear();
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<DB::OrganizationUserRelInfo::ptr> orgs;
    OrganizationUserRelRepository::ListByUser(orgs, uid, -1, true, db);
    std::sort(orgs.begin(), orgs.end(), [](const auto& a, const auto& b) {
        return a->getCreateTime() < b->getCreateTime();
    });

    for (const auto& rel : orgs) {
        auto org = OrganizationRepository::Get(rel->getOrgId(), db);
        if (!org) {
            continue;
        }
        OrganizationItem item;
        item.rel_id = rel->getId();
        item.org_id = rel->getOrgId();
        item.org_name = org->getName();
        item.org_description = org->getDescription();
        item.type = org->getType();
        item.role = rel->getRole();
        item.join_time = rel->getCreateTime();
        item.member_count =
            OrganizationUserRelRepository::CountByOrg(rel->getOrgId(), OrgMemberStatus::APPROVED, true, db);
        out.push_back(std::move(item));
    }
    return true;
}

bool UserQueryService::ListAssignments(int64_t uid, std::vector<AssignmentItem>& out)
{
    out.clear();
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<DB::OrganizationUserRelInfo::ptr> user_org_rels;
    OrganizationUserRelRepository::ListByUser(user_org_rels, uid, -1, true, db);
    for (const auto& org_rel : user_org_rels) {
        std::vector<DB::AssignmentOrganizationRelInfo::ptr> assign_org_rels;
        AssignmentOrganizationRelRepository::ListByOrgPages(
            assign_org_rels, org_rel->getOrgId(), 0, INT32_MAX, -1, true, db);
        for (const auto& assign_org_rel : assign_org_rels) {
            auto assign = AssignmentRepository::Get(assign_org_rel->getAssignmentId(), db);
            if (!assign || assign->getStatus() == AssignmentStatus::DRAFT || assign->getIsDeleted()) {
                continue;
            }
            AssignmentItem item;
            item.id = assign->getId();
            item.name = assign->getName();
            item.subject_name = assign->getSubjectName();
            item.end_time = assign->getDeadline();
            item.status = assign->getStatus();
            item.priority = assign->getPriority();
            item.create_time = assign->getCreateTime();
            item.description = assign->getDescription();
            item.file_type = assign->getFileType();
            item.file_size = assign->getMaxSize();
            out.push_back(std::move(item));
        }
    }
    return true;
}
