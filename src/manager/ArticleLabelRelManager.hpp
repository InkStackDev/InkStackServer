#pragma once

#include "blog/data/article_label_rel_info.h"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

class ArticleLabelRelManager {
public:
    data::ArticleLabelRelInfo::ptr get(int64_t id);

    bool listByArticleId(std::vector<data::ArticleLabelRelInfo::ptr>& infos, int64_t id, bool valid);

    bool listByLabelId(std::vector<data::ArticleLabelRelInfo::ptr>& infos, int64_t label_id, bool valid);

    data::ArticleLabelRelInfo::ptr getByArticleIdLabelId(int64_t article_id, int64_t label_id);

private:
    static data::ArticleLabelRelInfo::ptr ParseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<ArticleLabelRelManager> ArticleLabelRelMgr;
