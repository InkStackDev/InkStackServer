#include "ArticleRelRepository.hpp"

#include <Chen/DB/Redis.hpp>

#include "../Manager/CacheUtil.hpp"

namespace {

std::string CategoryKey(int64_t article_id, int64_t category_id)
{
    return "acr:" + std::to_string(article_id) + ":" + std::to_string(category_id);
}

std::string LabelKey(int64_t article_id, int64_t label_id)
{
    return "alr:" + std::to_string(article_id) + ":" + std::to_string(label_id);
}

constexpr int32_t kStatePublished = 2;

} // namespace

DB::ArticleCategoryRelInfo::ptr ArticleCategoryRelRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return DB::ArticleCategoryRelInfoDao::Query(id, conn);
}

DB::ArticleCategoryRelInfo::ptr ArticleCategoryRelRepository::GetByArticleAndCategory(int64_t article_id,
    int64_t category_id,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return nullptr;
    }
    const std::string key = CategoryKey(article_id, category_id);
    int64_t cached_id = GetCachedIdMapping(key);
    if (cached_id > 0) {
        auto info = Get(cached_id, conn);
        if (info) {
            return info;
        }
        RedisCacheDel("cache:map:" + key);
    }

    auto info = DB::ArticleCategoryRelInfoDao::QueryByArticleIdCategoryId(article_id, category_id, conn);
    if (info) {
        CacheIdMapping(key, info->getId());
    }
    return info;
}

bool ArticleCategoryRelRepository::Insert(DB::ArticleCategoryRelInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    if (DB::ArticleCategoryRelInfoDao::Insert(info, conn) != 0) {
        return false;
    }
    CacheIdMapping(CategoryKey(info->getArticleId(), info->getCategoryId()), info->getId());
    return true;
}

bool ArticleCategoryRelRepository::Update(DB::ArticleCategoryRelInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::ArticleCategoryRelInfoDao::Update(info, conn) == 0;
}

bool ArticleCategoryRelRepository::ListByArticle(std::vector<DB::ArticleCategoryRelInfo::ptr>& infos,
    int64_t article_id,
    bool is_valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return false;
    }
    auto qb = DB::ArticleCategoryRelInfoDao::newQuery();
    if (!qb) {
        return false;
    }
    qb->where("article_id", "=", article_id);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    return DB::ArticleCategoryRelInfoDao::QueryByBuilder(infos, qb, conn) == 0;
}

bool ArticleCategoryRelRepository::ListByCategory(std::vector<DB::ArticleCategoryRelInfo::ptr>& infos,
    int64_t category_id,
    bool is_valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return false;
    }
    auto qb = DB::ArticleCategoryRelInfoDao::newQuery();
    if (!qb) {
        return false;
    }
    qb->where("category_id", "=", category_id);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    return DB::ArticleCategoryRelInfoDao::QueryByBuilder(infos, qb, conn) == 0;
}

DB::ArticleLabelRelInfo::ptr ArticleLabelRelRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return DB::ArticleLabelRelInfoDao::Query(id, conn);
}

DB::ArticleLabelRelInfo::ptr ArticleLabelRelRepository::GetByArticleAndLabel(int64_t article_id,
    int64_t label_id,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return nullptr;
    }
    const std::string key = LabelKey(article_id, label_id);
    int64_t cached_id = GetCachedIdMapping(key);
    if (cached_id > 0) {
        auto info = Get(cached_id, conn);
        if (info) {
            return info;
        }
        RedisCacheDel("cache:map:" + key);
    }

    auto info = DB::ArticleLabelRelInfoDao::QueryByArticleIdLabelId(article_id, label_id, conn);
    if (info) {
        CacheIdMapping(key, info->getId());
    }
    return info;
}

bool ArticleLabelRelRepository::Insert(DB::ArticleLabelRelInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    if (DB::ArticleLabelRelInfoDao::Insert(info, conn) != 0) {
        return false;
    }
    CacheIdMapping(LabelKey(info->getArticleId(), info->getLabelId()), info->getId());
    return true;
}

bool ArticleLabelRelRepository::Update(DB::ArticleLabelRelInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::ArticleLabelRelInfoDao::Update(info, conn) == 0;
}

bool ArticleLabelRelRepository::ListByArticle(std::vector<DB::ArticleLabelRelInfo::ptr>& infos,
    int64_t article_id,
    bool is_valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return false;
    }
    auto qb = DB::ArticleLabelRelInfoDao::newQuery();
    if (!qb) {
        return false;
    }
    qb->where("article_id", "=", article_id);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    return DB::ArticleLabelRelInfoDao::QueryByBuilder(infos, qb, conn) == 0;
}

bool ArticleLabelRelRepository::ListByLabel(std::vector<DB::ArticleLabelRelInfo::ptr>& infos,
    int64_t label_id,
    bool is_valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return false;
    }
    auto qb = DB::ArticleLabelRelInfoDao::newQuery();
    if (!qb) {
        return false;
    }
    qb->where("label_id", "=", label_id);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    return DB::ArticleLabelRelInfoDao::QueryByBuilder(infos, qb, conn) == 0;
}

void ArticleLabelRelRepository::CountPublishedByLabels(const std::vector<int64_t>& label_ids,
    std::vector<std::pair<int64_t, int64_t>>& rows,
    chen::IDB::ptr conn)
{
    if (!conn || label_ids.empty()) {
        return;
    }
    auto qb = DB::ArticleLabelRelInfoDao::newQuery("r");
    if (!qb) {
        return;
    }
    qb->select("r.label_id, COUNT(*) cnt");
    qb->join("article a", "r.article_id = a.id");
    qb->whereIn("r.label_id", label_ids);
    qb->where("r.is_deleted", "=", (int64_t)0);
    qb->where("a.state", "=", (int64_t)kStatePublished);
    qb->where("a.is_deleted", "=", (int64_t)0);
    qb->groupBy("r.label_id");
    qb->queryPairs<int64_t, int64_t>(rows, conn);
}

bool ArticleCategoryRelRepository::ListByArticles(std::vector<DB::ArticleCategoryRelInfo::ptr>& infos,
    const std::vector<int64_t>& article_ids,
    chen::IDB::ptr conn)
{
    if (!conn || article_ids.empty()) {
        return false;
    }
    auto qb = DB::ArticleCategoryRelInfoDao::newQuery();
    if (!qb) {
        return false;
    }
    qb->whereIn("article_id", article_ids);
    qb->where("is_deleted", "=", (int64_t)0);
    return DB::ArticleCategoryRelInfoDao::QueryByBuilder(infos, qb, conn) == 0;
}

bool ArticleLabelRelRepository::ListByArticles(std::vector<DB::ArticleLabelRelInfo::ptr>& infos,
    const std::vector<int64_t>& article_ids,
    chen::IDB::ptr conn)
{
    if (!conn || article_ids.empty()) {
        return false;
    }
    auto qb = DB::ArticleLabelRelInfoDao::newQuery();
    if (!qb) {
        return false;
    }
    qb->whereIn("article_id", article_ids);
    qb->where("is_deleted", "=", (int64_t)0);
    return DB::ArticleLabelRelInfoDao::QueryByBuilder(infos, qb, conn) == 0;
}
