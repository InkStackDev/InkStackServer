#include "AdminArticleService.hpp"

#include "../Repository/ArticleQueryRepository.hpp"
#include "../Repository/CommentQueryRepository.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Util/Util.hpp"

#include <utility>

bool AdminArticleService::List(uint64_t offset,
    int32_t state,
    int64_t category,
    int32_t role,
    int32_t days,
    uint64_t page_size,
    std::vector<Item>& out,
    int64_t& total)
{
    out.clear();
    total = 0;
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<DB::ArticleInfo::ptr> articles;
    total = ArticleQueryRepository::ListByPages(articles, offset, state, category, role, days, page_size, true, db);
    for (const auto& article : articles) {
        if (!article) {
            continue;
        }
        Item item;
        item.id = article->getId();
        item.user_id = article->getUserId();
        auto user = UserRepository::Get(article->getUserId(), db);
        if (user) {
            item.has_author = true;
            item.author = user->getName();
            item.email = user->getEmail();
            item.author_role = user->getRole();
        }
        item.title = article->getTitle();
        item.state = article->getState();
        item.views = article->getViews();
        item.praise = article->getPraise();
        item.favorites = article->getFavorites();
        item.publish_time = article->getPublishTime();
        item.summary = article->getContent().substr(0, 100);
        item.comment_count = CommentQueryRepository::CountByArticle(article->getId(), db);
        out.push_back(std::move(item));
    }
    return true;
}

bool AdminArticleService::GetStats(int32_t category, int32_t role, int32_t days, const std::string& keyword, Stats& out)
{
    out = Stats{};
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto stats = ArticleQueryRepository::GetStats(category, role, days, keyword, db);
    out.total = stats.total;
    out.pending = stats.pending;
    out.published = stats.published;
    out.rejected = stats.rejected;
    out.reported = stats.reported;
    return true;
}
