#include "ArticleQueryService.hpp"

#include "../Domain/ArticleTypes.hpp"
#include "../Domain/UserTypes.hpp"
#include "../Repository/ArticlePraiseRelRepository.hpp"
#include "../Repository/ArticleQueryRepository.hpp"
#include "../Repository/ArticleRelRepository.hpp"
#include "../Repository/ArticleRepository.hpp"
#include "../Repository/CategoryRepository.hpp"
#include "../Repository/CommentQueryRepository.hpp"
#include "../Repository/LabelRepository.hpp"
#include "../Repository/UserFollowRelRepository.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Util/Util.hpp"

#include <utility>

namespace {

void FillCategories(std::vector<ArticleQueryService::CategoryBrief>& out, int64_t article_id, chen::IDB::ptr db)
{
    out.clear();
    std::vector<DB::ArticleCategoryRelInfo::ptr> rels;
    ArticleCategoryRelRepository::ListByArticle(rels, article_id, true, db);
    for (const auto& rel : rels) {
        auto category = CategoryRepository::Get(rel->getCategoryId(), db);
        if (!category || category->getIsDeleted()) {
            continue;
        }
        out.push_back({category->getId(), category->getName(), category->getColor()});
    }
}

void FillLabels(std::vector<ArticleQueryService::CategoryBrief>& out, int64_t article_id, chen::IDB::ptr db)
{
    out.clear();
    std::vector<DB::ArticleLabelRelInfo::ptr> rels;
    ArticleLabelRelRepository::ListByArticle(rels, article_id, true, db);
    for (const auto& rel : rels) {
        auto label = LabelRepository::Get(rel->getLabelId(), db);
        if (!label || label->getIsDeleted()) {
            continue;
        }
        out.push_back({label->getId(), label->getName(), label->getColor()});
    }
}

bool FillListItem(ArticleQueryService::ListItem& item,
    const DB::ArticleInfo::ptr& article,
    bool with_details,
    chen::IDB::ptr db)
{
    if (!article) {
        return false;
    }
    item.id = article->getId();
    item.user_id = article->getUserId();
    auto user = UserRepository::Get(item.user_id, db);
    item.author = user ? user->getName() : "";
    item.title = article->getTitle();
    item.summary = article->getContent().substr(0, 100);
    item.type = article->getType();
    item.state = article->getState();
    item.views = article->getViews();
    item.praise = article->getPraise();
    item.favorites = article->getFavorites();
    item.publish_time = article->getPublishTime();
    if (with_details) {
        item.comment_count = CommentQueryRepository::CountByArticle(item.id, db);
        FillCategories(item.categories, item.id, db);
    }
    return true;
}

} // namespace

bool ArticleQueryService::ListByUser(int64_t user_id,
    int64_t offset,
    int64_t page_size,
    int32_t state,
    std::vector<ListItem>& out,
    int64_t& total)
{
    out.clear();
    total = 0;
    auto db = GetDB();
    if (!db) {
        return false;
    }
    std::vector<DB::ArticleInfo::ptr> articles;
    total = ArticleQueryRepository::ListByUserIdPages(articles, user_id, offset, page_size, true, state, db);
    for (const auto& article : articles) {
        ListItem item;
        if (FillListItem(item, article, true, db)) {
            out.push_back(std::move(item));
        }
    }
    return true;
}

bool ArticleQueryService::ListByCategory(int64_t category_id,
    int64_t offset,
    int64_t page_size,
    std::vector<ListItem>& out,
    int64_t& total)
{
    out.clear();
    total = 0;
    auto db = GetDB();
    if (!db) {
        return false;
    }
    std::vector<DB::ArticleInfo::ptr> articles;
    total = ArticleQueryRepository::ListByCategoryPages(articles, category_id, offset, page_size, true, db);
    for (const auto& article : articles) {
        ListItem item;
        if (FillListItem(item, article, false, db)) {
            out.push_back(std::move(item));
        }
    }
    return true;
}

bool ArticleQueryService::ListByLabel(int64_t label_id,
    int64_t offset,
    int64_t page_size,
    std::vector<ListItem>& out,
    int64_t& total)
{
    out.clear();
    total = 0;
    auto db = GetDB();
    if (!db) {
        return false;
    }
    std::vector<DB::ArticleInfo::ptr> articles;
    total = ArticleQueryRepository::ListByLabelPages(articles, label_id, offset, page_size, true, db);
    for (const auto& article : articles) {
        ListItem item;
        if (FillListItem(item, article, false, db)) {
            out.push_back(std::move(item));
        }
    }
    return true;
}

ArticleQueryService::DetailResult ArticleQueryService::GetDetail(const Viewer& viewer,
    int64_t article_id,
    uint32_t type,
    Detail& out)
{
    out = Detail{};
    if (!viewer.valid()) {
        return DetailResult::AccessDenied;
    }
    auto db = GetDB();
    if (!db) {
        return DetailResult::DbError;
    }

    auto article = ArticleRepository::Get(article_id, db);
    if (!article) {
        return DetailResult::NotFound;
    }
    if (article->getIsDeleted()) {
        return DetailResult::AccessDenied;
    }
    bool is_author = viewer.user_id == article->getUserId();
    if (article->getState() != ArticleStatus::PUBLISHED && !viewer.is_platform_admin && !is_author) {
        return DetailResult::AccessDenied;
    }
    if (type == 1 && !is_author) {
        return DetailResult::AccessDenied;
    }

    out.id = article->getId();
    out.user_id = article->getUserId();
    out.title = article->getTitle();
    out.content = article->getContent();
    out.type = article->getType();
    out.publish_time = article->getPublishTime();
    out.update_time = article->getUpdateTime();
    out.state = article->getState();
    out.is_deleted = article->getIsDeleted();
    out.views = article->getViews();
    out.praise = article->getPraise();
    out.favorites = article->getFavorites();

    auto author = UserRepository::Get(out.user_id, db);
    out.author = author ? author->getName() : "";
    out.author_avatar = author ? author->getAvatar() : "";

    std::vector<DB::ArticleInfo::ptr> author_articles;
    ArticleQueryRepository::ListByUserId(author_articles, out.user_id, true, db);
    for (const auto& item : author_articles) {
        if (item && item->getState() == ArticleStatus::PUBLISHED) {
            out.author_article_count++;
            out.author_praise_count += item->getPraise();
        }
    }
    out.author_follower_count = UserFollowRelRepository::CountFollowers(out.user_id, db);

    FillCategories(out.categories, out.id, db);
    FillLabels(out.labels, out.id, db);
    return DetailResult::Ok;
}

bool ArticleQueryService::GetArticle(int64_t article_id, DB::ArticleInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return false;
    }
    out = ArticleRepository::Get(article_id, db);
    return out != nullptr;
}

bool ArticleQueryService::ListPraiseUsers(int64_t article_id,
    uint64_t offset,
    uint64_t size,
    std::vector<PraiseUser>& out,
    int64_t& total)
{
    out.clear();
    total = 0;
    auto db = GetDB();
    if (!db) {
        return false;
    }
    std::vector<DB::ArticlePraiseRelInfo::ptr> rels;
    ArticlePraiseRelRepository::ListByArticle(rels, article_id, offset, size, db);
    for (const auto& rel : rels) {
        PraiseUser item;
        item.id = rel->getId();
        item.user_id = rel->getUserId();
        item.article_id = rel->getArticleId();
        item.create_time = rel->getCreateTime();
        auto user = UserRepository::Get(rel->getUserId(), db);
        if (user && !user->getIsDeleted()) {
            item.user_name = user->getName();
            item.user_avatar = user->getAvatar();
        }
        out.push_back(std::move(item));
    }
    total = ArticlePraiseRelRepository::CountByArticle(article_id, db);
    return true;
}

bool ArticleQueryService::ListPraisedArticles(int64_t user_id,
    uint64_t offset,
    uint64_t size,
    std::vector<PraiseArticle>& out,
    int64_t& total)
{
    out.clear();
    total = 0;
    auto db = GetDB();
    if (!db) {
        return false;
    }
    std::vector<DB::ArticlePraiseRelInfo::ptr> rels;
    ArticlePraiseRelRepository::ListByUser(rels, user_id, offset, size, db);
    for (const auto& rel : rels) {
        PraiseArticle item;
        item.id = rel->getId();
        item.user_id = rel->getUserId();
        item.article_id = rel->getArticleId();
        item.create_time = rel->getCreateTime();
        auto article = ArticleRepository::Get(rel->getArticleId(), db);
        if (article && !article->getIsDeleted()) {
            item.title = article->getTitle();
        }
        out.push_back(std::move(item));
    }
    total = ArticlePraiseRelRepository::CountByUser(user_id, db);
    return true;
}
