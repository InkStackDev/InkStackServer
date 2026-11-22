#pragma once

#include "DB/ArticleLabelRelInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

class ArticleLabelRelManager {
public:
    DB::ArticleLabelRelInfo::ptr get(int64_t id);

    bool listByArticleId(std::vector<DB::ArticleLabelRelInfo::ptr>& infos, int64_t id, bool valid);

    bool listByLabelId(std::vector<DB::ArticleLabelRelInfo::ptr>& infos, int64_t label_id, bool valid);

    DB::ArticleLabelRelInfo::ptr getByArticleIdLabelId(int64_t article_id, int64_t label_id);
};

typedef chen::Singleton<ArticleLabelRelManager> ArticleLabelRelMgr;
