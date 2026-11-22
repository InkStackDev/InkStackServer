#pragma once

#include "DB/ArticlePraiseRelInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

class ArticlePraiseRelManager {
public:
    void add(DB::ArticlePraiseRelInfo::ptr info);

    DB::ArticlePraiseRelInfo::ptr get(int64_t id);

    DB::ArticlePraiseRelInfo::ptr getByUserAndArticle(int64_t user_id, int64_t article_id);

    // praise/unpraise
    DB::ArticlePraiseRelInfo::ptr praise(int64_t user_id, int64_t article_id);

    bool unpraise(int64_t user_id, int64_t article_id);

    bool isPraising(int64_t user_id, int64_t article_id);

    // queries
    void listByArticle(std::vector<DB::ArticlePraiseRelInfo::ptr>& results,
        int64_t article_id,
        uint64_t offset,
        uint64_t size);

    void listByUser(std::vector<DB::ArticlePraiseRelInfo::ptr>& results,
        int64_t user_id,
        uint64_t offset,
        uint64_t size);

    int64_t countByArticle(int64_t article_id);

    int64_t countByUser(int64_t user_id);

private:
    static DB::ArticlePraiseRelInfo::ptr ParseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<ArticlePraiseRelManager> ArticlePraiseRelMgr;
