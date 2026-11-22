/**
 * @file UserQueryService.hpp
 * @brief 用户侧查询用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

class UserQueryService {
public:
    struct Profile {
        int64_t id = 0;
        std::string name;
        std::string account;
        std::string avatar;
        std::string email;
        int32_t role = 0;
        std::string bio;
        std::string website;
        std::string location;
        int32_t status = 0;
        std::string github;
        int64_t login_time = 0;
        int64_t create_time = 0;
        int64_t following_count = 0;
        int64_t follower_count = 0;
        bool include_email = false;
    };

    struct FollowUser {
        int64_t id = 0;
        std::string name;
        std::string account;
        std::string avatar;
        std::string bio;
        int64_t following_count = 0;
        int64_t follower_count = 0;
        int64_t create_time = 0;
    };

    struct Brief {
        int64_t id = 0;
        std::string name;
        std::string account;
        std::string avatar;
    };

    struct OrganizationItem {
        int64_t rel_id = 0;
        int64_t org_id = 0;
        std::string org_name;
        std::string org_description;
        std::string type;
        int32_t role = 0;
        int64_t join_time = 0;
        int64_t member_count = 0;
    };

    struct AssignmentItem {
        int64_t id = 0;
        std::string name;
        std::string subject_name;
        int64_t end_time = 0;
        int32_t status = 0;
        int32_t priority = 0;
        int64_t create_time = 0;
        std::string description;
        std::string file_type;
        int64_t file_size = 0;
    };

    enum class ExistsResult {
        Ok,
        InvalidAuthId,
        DbError
    };

    static ExistsResult Exists(const std::string& auth_id, bool& exists);

    static void ListAllIds(std::vector<int64_t>& ids);

    static void ListActiveIds(std::vector<int64_t>& ids);

    static void QueryBrief(const std::vector<int64_t>& ids, std::vector<Brief>& out);

    static bool GetProfile(int64_t viewer_uid, int64_t target_uid, Profile& out);

    static bool GetUserBrief(int64_t user_id, Brief& out);

    static bool GetRole(int64_t user_id, int32_t& role, bool& deleted);

    static bool ListFollowers(int64_t target_uid,
        int32_t offset,
        int32_t size,
        std::vector<FollowUser>& out,
        int64_t& total);

    static bool ListFollowing(int64_t target_uid,
        int32_t offset,
        int32_t size,
        std::vector<FollowUser>& out,
        int64_t& total);

    static bool ListMutual(int64_t uid,
        const std::string& keyword,
        int32_t offset,
        int32_t size,
        std::vector<Brief>& out,
        int64_t& total);

    static bool ListOrganizations(int64_t uid, std::vector<OrganizationItem>& out);

    static bool ListAssignments(int64_t uid, std::vector<AssignmentItem>& out);
};
