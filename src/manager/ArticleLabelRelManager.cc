#include "ArticleLabelRelManager.hpp"

#include "../Repository/ArticleRelRepository.hpp"
#include "../Util/Util.hpp"

DB::ArticleLabelRelInfo::ptr ArticleLabelRelManager::get(int64_t id)
{
    return ArticleLabelRelRepository::Get(id, GetDB());
}

bool ArticleLabelRelManager::listByArticleId(std::vector<DB::ArticleLabelRelInfo::ptr>& infos, int64_t id, bool valid)
{
    return ArticleLabelRelRepository::ListByArticle(infos, id, valid, GetDB());
}

bool ArticleLabelRelManager::listByLabelId(std::vector<DB::ArticleLabelRelInfo::ptr>& infos,
    int64_t label_id,
    bool valid)
{
    return ArticleLabelRelRepository::ListByLabel(infos, label_id, valid, GetDB());
}

DB::ArticleLabelRelInfo::ptr ArticleLabelRelManager::getByArticleIdLabelId(int64_t article_id, int64_t label_id)
{
    return ArticleLabelRelRepository::GetByArticleAndLabel(article_id, label_id, GetDB());
}
