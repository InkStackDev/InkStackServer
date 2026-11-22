#pragma once

#include "DB/ArticleCategoryRelInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

class ArticleCategoryRelManager {
public:
    DB::ArticleCategoryRelInfo::ptr get(int64_t id);

    bool listByArticleId(std::vector<DB::ArticleCategoryRelInfo::ptr>& infos, int64_t id, bool valid);

    bool listByCategoryId(std::vector<DB::ArticleCategoryRelInfo::ptr>& infos, int64_t category_id, bool valid);

    DB::ArticleCategoryRelInfo::ptr getByArticleIdCategoryId(int64_t article_id, int64_t category_id);

private:
    static DB::ArticleCategoryRelInfo::ptr ParseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<ArticleCategoryRelManager> ArticleCategoryRelMgr;
