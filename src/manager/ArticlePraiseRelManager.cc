#include "ArticlePraiseRelManager.hpp"

#include "../Repository/ArticlePraiseRelRepository.hpp"
#include "../Service/ArticlePraiseRelService.hpp"
#include "../Util/Util.hpp"

void ArticlePraiseRelManager::add(DB::ArticlePraiseRelInfo::ptr info)
{
    ArticlePraiseRelRepository::Put(info);
}

DB::ArticlePraiseRelInfo::ptr ArticlePraiseRelManager::get(int64_t id)
{
    return ArticlePraiseRelRepository::Get(id, GetDB());
}

DB::ArticlePraiseRelInfo::ptr ArticlePraiseRelManager::getByUserAndArticle(int64_t user_id, int64_t article_id)
{
    return ArticlePraiseRelRepository::GetByUserAndArticle(user_id, article_id, GetDB());
}

DB::ArticlePraiseRelInfo::ptr ArticlePraiseRelManager::praise(int64_t user_id, int64_t article_id)
{
    return ArticlePraiseRelService::Praise(user_id, article_id);
}

bool ArticlePraiseRelManager::unpraise(int64_t user_id, int64_t article_id)
{
    return ArticlePraiseRelService::Unpraise(user_id, article_id);
}

bool ArticlePraiseRelManager::isPraising(int64_t user_id, int64_t article_id)
{
    return ArticlePraiseRelService::IsPraising(user_id, article_id);
}

void ArticlePraiseRelManager::listByArticle(std::vector<DB::ArticlePraiseRelInfo::ptr>& results,
    int64_t article_id,
    uint64_t offset,
    uint64_t size)
{
    ArticlePraiseRelRepository::ListByArticle(results, article_id, offset, size, GetDB());
}

void ArticlePraiseRelManager::listByUser(std::vector<DB::ArticlePraiseRelInfo::ptr>& results,
    int64_t user_id,
    uint64_t offset,
    uint64_t size)
{
    ArticlePraiseRelRepository::ListByUser(results, user_id, offset, size, GetDB());
}

int64_t ArticlePraiseRelManager::countByArticle(int64_t article_id)
{
    return ArticlePraiseRelRepository::CountByArticle(article_id, GetDB());
}

int64_t ArticlePraiseRelManager::countByUser(int64_t user_id)
{
    return ArticlePraiseRelRepository::CountByUser(user_id, GetDB());
}
