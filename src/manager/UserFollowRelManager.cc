#include "UserFollowRelManager.hpp"

#include "../Repository/UserFollowRelRepository.hpp"
#include "../Service/UserFollowRelService.hpp"
#include "../Util/Util.hpp"

void UserFollowRelManager::add(DB::UserFollowRelInfo::ptr info)
{
    UserFollowRelRepository::Put(info);
}

DB::UserFollowRelInfo::ptr UserFollowRelManager::get(int64_t id)
{
    return UserFollowRelRepository::Get(id, GetDB());
}

DB::UserFollowRelInfo::ptr UserFollowRelManager::getByFollowerAndFollowing(int64_t follower_id, int64_t following_id)
{
    return UserFollowRelRepository::GetByFollowerAndFollowing(follower_id, following_id, GetDB());
}

DB::UserFollowRelInfo::ptr UserFollowRelManager::follow(int64_t follower_id, int64_t following_id)
{
    return UserFollowRelService::Follow(follower_id, following_id);
}

bool UserFollowRelManager::unfollow(int64_t follower_id, int64_t following_id)
{
    return UserFollowRelService::Unfollow(follower_id, following_id);
}

bool UserFollowRelManager::isFollowing(int64_t follower_id, int64_t following_id)
{
    return UserFollowRelService::IsFollowing(follower_id, following_id);
}

void UserFollowRelManager::listFollowing(std::vector<DB::UserFollowRelInfo::ptr>& results,
    int64_t follower_id,
    uint64_t offset,
    uint64_t size)
{
    UserFollowRelRepository::ListFollowing(results, follower_id, offset, size, GetDB());
}

void UserFollowRelManager::listFollowers(std::vector<DB::UserFollowRelInfo::ptr>& results,
    int64_t following_id,
    uint64_t offset,
    uint64_t size)
{
    UserFollowRelRepository::ListFollowers(results, following_id, offset, size, GetDB());
}

int64_t UserFollowRelManager::countFollowing(int64_t follower_id)
{
    return UserFollowRelRepository::CountFollowing(follower_id, GetDB());
}

int64_t UserFollowRelManager::countFollowers(int64_t following_id)
{
    return UserFollowRelRepository::CountFollowers(following_id, GetDB());
}

void UserFollowRelManager::listMutualFollowing(std::vector<int64_t>& user_ids, int64_t uid)
{
    UserFollowRelRepository::ListMutualFollowing(user_ids, uid, GetDB());
}
