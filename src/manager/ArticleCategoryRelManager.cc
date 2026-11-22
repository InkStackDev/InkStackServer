#include "ArticleCategoryRelManager.hpp"

#include "../Repository/ArticleRelRepository.hpp"
#include "../Util/Util.hpp"

DB::ArticleCategoryRelInfo::ptr ArticleCategoryRelManager::get(int64_t id)
{
    return ArticleCategoryRelRepository::Get(id, GetDB());
}

bool ArticleCategoryRelManager::listByArticleId(std::vector<DB::ArticleCategoryRelInfo::ptr>& infos,
    int64_t id,
    bool valid)
{
    return ArticleCategoryRelRepository::ListByArticle(infos, id, valid, GetDB());
}

bool ArticleCategoryRelManager::listByCategoryId(std::vector<DB::ArticleCategoryRelInfo::ptr>& infos,
    int64_t category_id,
    bool valid)
{
    return ArticleCategoryRelRepository::ListByCategory(infos, category_id, valid, GetDB());
}

DB::ArticleCategoryRelInfo::ptr ArticleCategoryRelManager::getByArticleIdCategoryId(int64_t article_id,
    int64_t category_id)
{
    return ArticleCategoryRelRepository::GetByArticleAndCategory(article_id, category_id, GetDB());
}
