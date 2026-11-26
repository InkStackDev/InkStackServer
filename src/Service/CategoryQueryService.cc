#include "CategoryQueryService.hpp"

#include "../Repository/ArticleRelRepository.hpp"
#include "../Repository/ArticleRepository.hpp"
#include "../Repository/CategoryRepository.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Util/Util.hpp"

#include <utility>

bool CategoryQueryService::List(int64_t requester_uid, bool admin_route, int64_t owner_uid, std::vector<Item>& out)
{
    out.clear();
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<DB::CategoryInfo::ptr> infos;
    if (owner_uid) {
        if (!CategoryRepository::ListByUserId(infos, owner_uid, true, db)) {
            return false;
        }
    } else if (admin_route) {
        CategoryRepository::ListAll(infos, true, db);
    } else if (!CategoryRepository::ListByUserId(infos, requester_uid, true, db)) {
        return false;
    }

    for (const auto& category : infos) {
        if (!category) {
            continue;
        }
        Item item;
        item.id = category->getId();
        item.user_id = category->getUserId();
        auto owner = UserRepository::Get(item.user_id, db);
        item.author = owner ? owner->getName() : "";
        item.author_avatar = owner ? owner->getAvatar() : "";
        item.name = category->getName();
        item.url = category->getUrl();
        item.icon = category->getIcon();
        item.color = category->getColor();
        item.description = category->getDescription();
        item.parent_id = category->getParentId();
        item.status = category->getStatus();
        item.create_time = category->getCreateTime();
        item.update_time = category->getUpdateTime();

        std::vector<DB::ArticleCategoryRelInfo::ptr> rels;
        ArticleCategoryRelRepository::ListByCategory(rels, item.id, true, db);
        for (const auto& rel : rels) {
            auto article = ArticleRepository::Get(rel->getArticleId(), db);
            if (article) {
                item.article_count++;
                item.view_count += article->getViews();
            }
        }
        out.push_back(std::move(item));
    }
    return true;
}

bool CategoryQueryService::ListAdmin(const Filter& filter,
    uint64_t offset,
    uint64_t limit,
    std::vector<Item>& out,
    int64_t& total,
    Stats& stats)
{
    out.clear();
    total = 0;
    stats = Stats{};
    auto db = GetDB();
    if (!db) {
        return false;
    }

    CategoryRepository::ListFilter repo_filter;
    repo_filter.user_id = filter.user_id;
    repo_filter.status = filter.status;
    repo_filter.keyword = filter.keyword;
    repo_filter.sort_by = filter.sort_by;
    repo_filter.sort_order = filter.sort_order;

    std::vector<CategoryRepository::ListItem> rows;
    total = CategoryRepository::ListByPages(rows, offset, limit, repo_filter, true, db);
    for (const auto& row : rows) {
        Item item;
        item.id = row.id;
        item.user_id = row.user_id;
        auto owner = UserRepository::Get(item.user_id, db);
        item.author = owner ? owner->getName() : "";
        item.author_avatar = owner ? owner->getAvatar() : "";
        item.name = row.name;
        item.url = row.url;
        item.icon = row.icon;
        item.color = row.color;
        item.description = row.description;
        item.parent_id = row.parent_id;
        item.status = row.status;
        item.create_time = row.create_time;
        item.update_time = row.update_time;
        item.article_count = row.article_count;
        item.view_count = row.view_count;
        out.push_back(std::move(item));
    }

    auto repo_stats = CategoryRepository::GetStats(db);
    stats.total_categories = repo_stats.total;
    stats.active_categories = repo_stats.active;
    stats.total_articles = repo_stats.articles;
    stats.total_views = repo_stats.views;
    return true;
}
