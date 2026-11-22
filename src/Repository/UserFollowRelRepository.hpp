/**
 * @file UserFollowRelRepository.hpp
 * @brief 用户关注关系读写与缓存
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/UserFollowRelInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <vector>

class UserFollowRelRepository {
public:
    static DB::UserFollowRelInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static DB::UserFollowRelInfo::ptr GetByFollowerAndFollowing(int64_t follower_id,
        int64_t following_id,
        chen::IDB::ptr conn);

    static bool Insert(DB::UserFollowRelInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::UserFollowRelInfo::ptr info, chen::IDB::ptr conn);

    static void Put(const DB::UserFollowRelInfo::ptr& info);

    static void Evict(int64_t id);

    static void ListFollowing(std::vector<DB::UserFollowRelInfo::ptr>& results,
        int64_t follower_id,
        uint64_t offset,
        uint64_t size,
        chen::IDB::ptr conn);

    static void ListFollowers(std::vector<DB::UserFollowRelInfo::ptr>& results,
        int64_t following_id,
        uint64_t offset,
        uint64_t size,
        chen::IDB::ptr conn);

    static int64_t CountFollowing(int64_t follower_id, chen::IDB::ptr conn);

    static int64_t CountFollowers(int64_t following_id, chen::IDB::ptr conn);

    static void ListMutualFollowing(std::vector<int64_t>& user_ids, int64_t uid, chen::IDB::ptr conn);
};
