#include "ArticleSearchService.hpp"

#include "../Domain/ArticleTypes.hpp"
#include "../Repository/ArticleRepository.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Search/SearchSync.hpp"
#include "../Util/Util.hpp"

#include <utility>

bool ArticleSearchService::Search(const tagSearchQuery& query, Result& out)
{
    out = Result{};

    tagSearchQuery q = query;
    q.State = ArticleStatus::PUBLISHED;

    tagSearchResult search_result;
    if (!SearchSyncMgr::GetInstance()->query(q, search_result)) {
        return false;
    }

    out.total = search_result.Total;
    out.items.reserve(search_result.Ids.size());

    for (auto id : search_result.Ids) {
        auto article = ArticleRepository::Get(static_cast<int64_t>(id), GetDB());
        if (!article || article->getIsDeleted()) {
            continue;
        }
        auto user = UserRepository::Get(article->getUserId(), GetDB());

        Item item;
        item.id = article->getId();
        item.author = user ? user->getName() : "";
        item.author_avatar = user ? user->getAvatar() : "";
        item.title = article->getTitle();
        item.summary = article->getContent().substr(0, 100);
        item.type = article->getType();
        item.state = article->getState();
        item.views = article->getViews();
        item.praise = article->getPraise();
        item.favorites = article->getFavorites();
        item.publish_time = article->getPublishTime();
        out.items.push_back(std::move(item));
    }

    return true;
}
