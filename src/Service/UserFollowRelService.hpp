/**
 * @file UserFollowRelService.hpp
 * @brief 用户关注用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/UserFollowRelInfo.hpp"

#include <cstdint>

class UserFollowRelService {
public:
    /**
     * @brief 关注（软删除记录会复活）
     */
    static DB::UserFollowRelInfo::ptr Follow(int64_t follower_id, int64_t following_id);

    static bool Unfollow(int64_t follower_id, int64_t following_id);

    static bool IsFollowing(int64_t follower_id, int64_t following_id);
};
