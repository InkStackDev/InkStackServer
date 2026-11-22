#include "UserFollowRelService.hpp"

#include <memory>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Repository/UserFollowRelRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

DB::UserFollowRelInfo::ptr UserFollowRelService::Follow(int64_t follower_id, int64_t following_id)
{
    auto db = GetDB();
    if (!db) {
        return nullptr;
    }

    auto existing = UserFollowRelRepository::GetByFollowerAndFollowing(follower_id, following_id, db);
    int64_t now = chen::TimeUtil::GetCurrentSec();
    if (existing) {
        if (!existing->getIsDeleted()) {
            return existing;
        }
        existing->setIsDeleted(0);
        existing->setUpdateTime(now);
        if (!UserFollowRelRepository::Update(existing, db)) {
            ERROR(logger) << "UserFollowRelService::Follow update failed";
            return nullptr;
        }
        UserFollowRelRepository::Put(existing);
        return existing;
    }

    auto info = std::make_shared<DB::UserFollowRelInfo>();
    info->setFollowerId(follower_id);
    info->setFollowingId(following_id);
    info->setIsDeleted(0);
    info->setCreateTime(now);
    info->setUpdateTime(now);
    if (!UserFollowRelRepository::Insert(info, db)) {
        ERROR(logger) << "UserFollowRelService::Follow insert failed";
        return nullptr;
    }
    UserFollowRelRepository::Put(info);
    return info;
}

bool UserFollowRelService::Unfollow(int64_t follower_id, int64_t following_id)
{
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto info = UserFollowRelRepository::GetByFollowerAndFollowing(follower_id, following_id, db);
    if (!info || info->getIsDeleted()) {
        return false;
    }
    info->setIsDeleted(1);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!UserFollowRelRepository::Update(info, db)) {
        ERROR(logger) << "UserFollowRelService::Unfollow update failed";
        return false;
    }
    UserFollowRelRepository::Put(info);
    return true;
}

bool UserFollowRelService::IsFollowing(int64_t follower_id, int64_t following_id)
{
    auto info = UserFollowRelRepository::GetByFollowerAndFollowing(follower_id, following_id, GetDB());
    return info && !info->getIsDeleted();
}
