#include "DashboardService.hpp"

#include "../Repository/ArticleQueryRepository.hpp"
#include "../Repository/ArticleRepository.hpp"
#include "../Repository/CommentQueryRepository.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Util/Util.hpp"

#include <algorithm>
#include <ctime>
#include <utility>

namespace {

int64_t PctChange(int64_t cur, int64_t prev)
{
    if (prev == 0) {
        return cur > 0 ? 100 : 0;
    }
    return (cur - prev) * 100 / prev;
}

} // namespace

bool DashboardService::GetStats(Stats& out)
{
    out = Stats{};
    auto db = GetDB();
    if (!db) {
        return false;
    }

    int64_t now = time(0);
    int64_t today_start = now - (now % 86400);
    int64_t yesterday_start = today_start - 86400;
    int64_t week_ago_start = today_start - 7 * 86400;
    int64_t two_weeks_ago_start = today_start - 14 * 86400;

    std::vector<int64_t> ids;
    UserRepository::GetAllIds(ids, true, db);
    out.total_users = (int64_t)ids.size();
    int64_t this_week_users = 0;
    int64_t last_week_users = 0;
    int64_t today_users = 0;
    int64_t yesterday_users = 0;
    for (auto id : ids) {
        auto user = UserRepository::Get(id, db);
        if (!user) {
            continue;
        }
        auto ct = user->getCreateTime();
        if (ct >= week_ago_start) {
            this_week_users++;
        } else if (ct >= two_weeks_ago_start) {
            last_week_users++;
        }
        if (ct >= today_start) {
            today_users++;
        } else if (ct >= yesterday_start) {
            yesterday_users++;
        }
    }

    auto article_stats = ArticleQueryRepository::GetStats(0, -1, 0, "", db);
    out.total_articles = article_stats.total;
    std::vector<DB::ArticleInfo::ptr> articles;
    ArticleQueryRepository::ListByPages(articles, 0, 0, 0, -1, 0, 0x7FFFFFFF, true, db);
    int64_t this_week_articles = 0;
    int64_t last_week_articles = 0;
    int64_t today_articles = 0;
    int64_t yesterday_articles = 0;
    for (const auto& article : articles) {
        auto ct = article->getCreateTime();
        if (ct >= week_ago_start) {
            this_week_articles++;
        } else if (ct >= two_weeks_ago_start) {
            last_week_articles++;
        }
        if (ct >= today_start) {
            today_articles++;
        } else if (ct >= yesterday_start) {
            yesterday_articles++;
        }
    }

    auto comment_stats = CommentQueryRepository::GetStats(db);
    out.total_comments = comment_stats.total;
    std::vector<DB::CommentInfo::ptr> comments;
    CommentQueryRepository::ListByAdmin(comments, 1, 0x7FFFFFFFLL, 0, "", 0, -1, db);
    int64_t this_week_comments = 0;
    int64_t last_week_comments = 0;
    int64_t today_comments = 0;
    int64_t yesterday_comments = 0;
    for (const auto& comment : comments) {
        auto ct = comment->getCreateTime();
        if (ct >= week_ago_start) {
            this_week_comments++;
        } else if (ct >= two_weeks_ago_start) {
            last_week_comments++;
        }
        if (ct >= today_start) {
            today_comments++;
        } else if (ct >= yesterday_start) {
            yesterday_comments++;
        }
    }

    int64_t today_visits = today_users + today_articles + today_comments;
    int64_t yesterday_visits = yesterday_users + yesterday_articles + yesterday_comments;

    out.total_users_change = PctChange(this_week_users, last_week_users);
    out.total_articles_change = PctChange(this_week_articles, last_week_articles);
    out.total_comments_change = PctChange(this_week_comments, last_week_comments);
    out.today_visits = today_visits;
    out.today_visits_change = PctChange(today_visits, yesterday_visits);
    out.new_users_today = today_users;
    out.new_users_today_change = PctChange(today_users, yesterday_users);
    return true;
}

bool DashboardService::GetTrend(int32_t period, Trend& out)
{
    out = Trend{};
    auto db = GetDB();
    if (!db) {
        return false;
    }

    if (period != 7 && period != 30 && period != 90) {
        period = 7;
    }
    int64_t now = time(0);
    int64_t today_start = now - (now % 86400);
    if (period == 90) {
        out.num_buckets = 13;
        out.bucket_sec = 7 * 86400;
        out.period_start = today_start - (out.num_buckets - 1) * out.bucket_sec;
    } else {
        out.num_buckets = period;
        out.bucket_sec = 86400;
        out.period_start = today_start - (period - 1) * 86400;
    }
    out.buckets.assign(out.num_buckets, 0);

    std::vector<int64_t> ids;
    UserRepository::GetAllIds(ids, true, db);
    for (auto id : ids) {
        auto user = UserRepository::Get(id, db);
        if (!user) {
            continue;
        }
        int64_t ts = user->getCreateTime();
        if (ts >= out.period_start) {
            int32_t idx = (ts - out.period_start) / out.bucket_sec;
            if (idx < out.num_buckets) {
                out.buckets[idx]++;
            }
        }
    }

    std::vector<DB::ArticleInfo::ptr> articles;
    ArticleQueryRepository::ListByPages(articles, 0, 0, 0, -1, 0, 0x7FFFFFFF, true, db);
    for (const auto& article : articles) {
        int64_t ts = article->getCreateTime();
        if (ts >= out.period_start) {
            int32_t idx = (ts - out.period_start) / out.bucket_sec;
            if (idx < out.num_buckets) {
                out.buckets[idx]++;
            }
        }
    }

    std::vector<DB::CommentInfo::ptr> comments;
    CommentQueryRepository::ListByAdmin(comments, 1, 0x7FFFFFFFLL, 0, "", 0, -1, db);
    for (const auto& comment : comments) {
        int64_t ts = comment->getCreateTime();
        if (ts >= out.period_start) {
            int32_t idx = (ts - out.period_start) / out.bucket_sec;
            if (idx < out.num_buckets) {
                out.buckets[idx]++;
            }
        }
    }
    return true;
}

bool DashboardService::GetActivities(int32_t limit, std::vector<Activity>& out)
{
    out.clear();
    auto db = GetDB();
    if (!db) {
        return false;
    }
    if (limit <= 0) {
        limit = 5;
    }
    int32_t fetch_count = limit * 2;

    std::vector<int64_t> ids;
    UserRepository::GetAllIds(ids, true, db);
    std::vector<DB::UserInfo::ptr> users;
    for (auto id : ids) {
        auto user = UserRepository::Get(id, db);
        if (user) {
            users.push_back(user);
        }
    }
    std::sort(users.begin(), users.end(), [](const auto& a, const auto& b) {
        return a->getCreateTime() > b->getCreateTime();
    });
    for (size_t i = 0; i < users.size() && i < (size_t)fetch_count; i++) {
        Activity activity;
        activity.type = "user_register";
        activity.title = "New user \"" + users[i]->getName() + "\" registered an account";
        activity.create_time = users[i]->getCreateTime();
        out.push_back(std::move(activity));
    }

    std::vector<DB::ArticleInfo::ptr> articles;
    ArticleQueryRepository::ListByPages(articles, 0, 0, 0, -1, 0, 0x7FFFFFFF, true, db);
    std::sort(articles.begin(), articles.end(), [](const auto& a, const auto& b) {
        return a->getCreateTime() > b->getCreateTime();
    });
    for (size_t i = 0; i < articles.size() && i < (size_t)fetch_count; i++) {
        Activity activity;
        activity.type = "article_publish";
        auto author = UserRepository::Get(articles[i]->getUserId(), db);
        std::string author_name = author ? author->getName() : "Unknown";
        activity.title = author_name + " published a new article \"" + articles[i]->getTitle() + "\"";
        activity.create_time = articles[i]->getCreateTime();
        out.push_back(std::move(activity));
    }

    std::vector<DB::CommentInfo::ptr> comments;
    CommentQueryRepository::ListByAdmin(comments, 1, fetch_count, 0, "", 0, -1, db);
    for (const auto& comment : comments) {
        Activity activity;
        activity.type = "comment_create";
        auto commenter = UserRepository::Get(comment->getUserId(), db);
        std::string commenter_name = commenter ? commenter->getName() : "Unknown";
        auto article = ArticleRepository::Get(comment->getArticleId(), db);
        std::string article_title = article ? article->getTitle() : "Unknown";
        activity.title = commenter_name + " commented on \"" + article_title + "\"";
        activity.create_time = comment->getCreateTime();
        out.push_back(std::move(activity));
    }

    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        return a.create_time > b.create_time;
    });
    if ((int32_t)out.size() > limit) {
        out.resize(limit);
    }
    return true;
}

bool DashboardService::GetRecentUsers(int32_t limit, std::vector<RecentUser>& out)
{
    out.clear();
    auto db = GetDB();
    if (!db) {
        return false;
    }
    if (limit <= 0) {
        limit = 5;
    }

    std::vector<int64_t> ids;
    UserRepository::GetAllIds(ids, true, db);
    std::vector<DB::UserInfo::ptr> users;
    for (auto id : ids) {
        auto user = UserRepository::Get(id, db);
        if (user) {
            users.push_back(user);
        }
    }
    std::sort(users.begin(), users.end(), [](const auto& a, const auto& b) {
        return a->getCreateTime() > b->getCreateTime();
    });
    if ((int32_t)users.size() > limit) {
        users.resize(limit);
    }
    for (const auto& user : users) {
        RecentUser item;
        item.name = user->getName();
        item.avatar = user->getAvatar();
        item.role = user->getRole();
        item.state = user->getState();
        out.push_back(std::move(item));
    }
    return true;
}
