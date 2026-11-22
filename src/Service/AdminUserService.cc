#include "AdminUserService.hpp"

#include "../Domain/UserTypes.hpp"
#include "../Repository/ArticleQueryRepository.hpp"
#include "../Repository/CommentQueryRepository.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Util/Util.hpp"

#include <ctime>
#include <map>
#include <utility>

bool AdminUserService::List(uint64_t offset,
    uint64_t page_size,
    int32_t role,
    int32_t state,
    int32_t days,
    std::vector<ListItem>& out,
    uint64_t& total)
{
    out.clear();
    total = 0;
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<DB::UserInfo::ptr> users;
    total = UserRepository::ListByPages(users, offset, page_size, role, state, days, true, db);

    std::map<int64_t, int64_t> article_counts;
    std::map<int64_t, int64_t> comment_counts;
    if (!users.empty()) {
        std::vector<int64_t> user_ids;
        user_ids.reserve(users.size());
        for (const auto& user : users) {
            user_ids.push_back(user->getId());
        }

        std::vector<std::pair<int64_t, int64_t>> rows;
        ArticleQueryRepository::CountByUsers(user_ids, rows, db);
        for (const auto& [uid, count] : rows) {
            article_counts[uid] = count;
        }

        rows.clear();
        CommentQueryRepository::CountByUsers(user_ids, rows, db);
        for (const auto& [uid, count] : rows) {
            comment_counts[uid] = count;
        }
    }

    for (const auto& user : users) {
        if (!user) {
            continue;
        }
        ListItem item;
        item.id = user->getId();
        item.name = user->getName();
        item.email = user->getEmail();
        item.avatar = user->getAvatar();
        item.role = user->getRole();
        item.state = user->getState();
        item.create_time = user->getCreateTime();
        item.login_time = user->getLoginTime();
        item.article_count = article_counts[user->getId()];
        item.comment_count = comment_counts[user->getId()];
        out.push_back(std::move(item));
    }
    return true;
}

bool AdminUserService::GetStats(Stats& out)
{
    out = Stats{};
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<int64_t> ids;
    UserRepository::GetAllIds(ids, false, db);

    int64_t threshold = time(0) - 30 * 24 * 3600;
    for (auto id : ids) {
        auto user = UserRepository::Get(id, db);
        if (!user || user->getIsDeleted()) {
            continue;
        }
        out.total++;
        if (user->getState() == UserState::ACTIVE) {
            out.active++;
        } else {
            out.inactive++;
        }
        if (user->getCreateTime() >= threshold) {
            out.new_30d++;
        }
    }
    return true;
}

bool AdminUserService::GetDetail(int64_t user_id, Detail& out)
{
    out = Detail{};
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto user = UserRepository::Get(user_id, db);
    if (!user) {
        return false;
    }

    out.id = user->getId();
    out.account = user->getAccount();
    out.name = user->getName();
    out.email = user->getEmail();
    out.avatar = user->getAvatar();
    out.role = user->getRole();
    out.state = user->getState();
    out.create_time = user->getCreateTime();
    out.login_time = user->getLoginTime();
    out.bio = user->getBio();
    out.website = user->getWebsite();

    std::vector<DB::ArticleInfo::ptr> articles;
    ArticleQueryRepository::ListByUserId(articles, user_id, false, db);
    out.article_count = (int64_t)articles.size();

    std::vector<DB::CommentInfo::ptr> comments;
    CommentQueryRepository::ListByUserId(comments, user_id, db);
    out.comment_count = (int64_t)comments.size();
    return true;
}
