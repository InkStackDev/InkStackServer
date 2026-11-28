#include "UserStatsService.hpp"

#include "../Domain/ArticleTypes.hpp"
#include "../Domain/OrganizationTypes.hpp"
#include "../Repository/ArticleQueryRepository.hpp"
#include "../Repository/ArticleRelRepository.hpp"
#include "../Repository/BadgeRepository.hpp"
#include "../Repository/CommentQueryRepository.hpp"
#include "../Repository/OrganizationUserRelRepository.hpp"
#include "../Repository/UserFollowRelRepository.hpp"
#include "../Util/Util.hpp"

#include <Chen/Util/StringUtil.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include <ctime>
#include <map>
#include <set>
#include <utility>

static int32_t CountToLevel(int32_t count)
{
    if (count <= 0) {
        return 0;
    }
    if (count == 1) {
        return 1;
    }
    if (count <= 3) {
        return 2;
    }
    if (count <= 6) {
        return 3;
    }
    return 4;
}

bool UserStatsService::GetStats(int64_t uid, Stats& out)
{
    out = Stats{};
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<DB::ArticleInfo::ptr> articles;
    ArticleQueryRepository::ListByUserId(articles, uid, true, db);

    std::set<int64_t> unique_labels;
    std::vector<int64_t> published_ids;
    for (const auto& article : articles) {
        if (!article || article->getIsDeleted()) {
            continue;
        }
        out.total_articles++;
        if (article->getState() == ArticleStatus::PUBLISHED) {
            out.published_articles++;
            published_ids.push_back(article->getId());
        } else if (article->getState() == ArticleStatus::PRIVATE) {
            out.private_articles++;
        }
        out.total_views += article->getViews();
        out.total_likes += article->getPraise();

        std::vector<DB::ArticleLabelRelInfo::ptr> labels;
        ArticleLabelRelRepository::ListByArticle(labels, article->getId(), true, db);
        for (const auto& label : labels) {
            unique_labels.insert(label->getLabelId());
        }
    }

    std::vector<DB::OrganizationUserRelInfo::ptr> orgs;
    OrganizationUserRelRepository::ListByUser(orgs, uid, OrgMemberStatus::APPROVED, true, db);
    out.total_organizations = (int64_t)orgs.size();
    out.total_comments = CommentQueryRepository::CountByArticles(published_ids, db);
    out.total_tags = (int64_t)unique_labels.size();
    return true;
}

bool UserStatsService::GetAchievements(int64_t uid, Achievements& out)
{
    out = Achievements{};
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<DB::ArticleInfo::ptr> articles;
    ArticleQueryRepository::ListByUserId(articles, uid, true, db);

    std::map<std::string, int32_t> date_contributions;
    for (const auto& article : articles) {
        if (!article || article->getIsDeleted() || article->getState() != ArticleStatus::PUBLISHED) {
            continue;
        }
        out.published_articles++;
        out.total_likes += article->getPraise();
        std::string date_key = chen::TimeUtil::Time2StrSec(article->getCreateTime(), "%Y-%m-%d");
        date_contributions[date_key]++;
    }

    {
        time_t now = time(0);
        time_t cutoff = now - 371 * 86400;
        std::vector<int64_t> timestamps;
        CommentQueryRepository::ListCreateTimesByUser(uid, cutoff, timestamps, db);
        for (auto ts : timestamps) {
            std::string date_key = chen::TimeUtil::Time2StrSec(ts, "%Y-%m-%d");
            date_contributions[date_key]++;
        }
    }

    time_t now = time(0);
    time_t today = chen::TimeUtil::GetDayStart(now);
    int32_t today_dow = chen::TimeUtil::GetDayOfWeek(today);
    time_t start_date = today - (371 + today_dow) * 86400;

    out.heatmap.reserve(53);
    for (int32_t w = 0; w < 53; w++) {
        std::vector<int32_t> week;
        week.reserve(7);
        for (int32_t d = 0; d < 7; d++) {
            time_t day = start_date + (w * 7 + d) * 86400;
            std::string key = chen::TimeUtil::Time2StrSec(day, "%Y-%m-%d");
            int32_t count = date_contributions[key];
            int32_t level = CountToLevel(count);
            out.total_contributions += level;
            week.push_back(level);
        }
        out.heatmap.push_back(std::move(week));
    }

    int64_t total_followers = UserFollowRelRepository::CountFollowers(uid, db);

    std::vector<DB::BadgeInfo::ptr> badges;
    BadgeRepository::ListValid(badges, db);
    out.total_badges = (int64_t)badges.size();
    for (const auto& badge : badges) {
        if (!badge) {
            continue;
        }
        Badge item;
        item.id = badge->getId();
        item.name = badge->getName();
        item.description = badge->getDescription();
        item.icon = badge->getIcon();
        item.color = badge->getColor();
        item.target = badge->getConditionValue();

        const std::string& key = badge->getConditionKey();
        if (key == "published_articles") {
            item.current = out.published_articles;
        } else if (key == "total_likes") {
            item.current = out.total_likes;
        } else if (key == "total_followers") {
            item.current = total_followers;
        }
        item.unlocked = item.current >= item.target;
        if (item.unlocked) {
            out.unlocked_badges++;
        }
        out.badges.push_back(std::move(item));
    }
    return true;
}
