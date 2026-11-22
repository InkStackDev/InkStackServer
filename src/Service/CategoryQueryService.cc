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
