#include "ArticleQueryRepository.hpp"

#include <sstream>

#include <Chen/Util/TimeUtil.hpp>
#include <Chen/Util/TypeUtil.hpp>

#include "../Cache/CacheUtil.hpp"
#include "ArticleRepository.hpp"

constexpr int32_t kStateChecking = 1;
constexpr int32_t kStatePublished = 2;
constexpr int32_t kStateRejected = 3;

bool ArticleQueryRepository::ListByUserId(std::vector<DB::ArticleInfo::ptr>& infos,
    int64_t user_id,
    bool valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return false;
    }
    auto qb = DB::ArticleInfoDao::newQuery();
    if (!qb) {
        return false;
    }
    qb->where("user_id", "=", user_id);
    qb->whereIf(valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    return DB::ArticleInfoDao::QueryByBuilder(infos, qb, conn) == 0;
}

int64_t ArticleQueryRepository::ListByUserIdPages(std::vector<DB::ArticleInfo::ptr>& infos,
    int64_t user_id,
    int32_t offset,
    int32_t size,
    bool valid,
    int32_t state,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::ArticleInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->whereIf(user_id != 0, "user_id", "=", user_id);
    qb->whereIf(state != 0, "state", "=", (int64_t)state);
    qb->whereIf(valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (DB::ArticleInfoDao::QueryByBuilderPages(infos, total, qb, offset, size, conn)) {
        return 0;
    }
    return total;
}

int64_t ArticleQueryRepository::ListVerifyPages(std::vector<DB::ArticleInfo::ptr>& infos,
    int32_t offset,
    int32_t size,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::ArticleInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("state", "=", (int64_t)kStateChecking);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (DB::ArticleInfoDao::QueryByBuilderPages(infos, total, qb, offset, size, conn)) {
        return 0;
    }
    return total;
}

int64_t ArticleQueryRepository::ListByPages(std::vector<DB::ArticleInfo::ptr>& infos,
    int32_t offset,
    int32_t state,
    int32_t category,
    int32_t role,
    int32_t days,
    int32_t size,
    bool valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::ArticleInfoDao::newQuery("a");
    if (!qb) {
        return 0;
    }
    qb->select("a.id, a.user_id, a.title, a.content, a.type, a.state, a.channel, a.is_deleted, a.publish_time, "
               "a.weight, a.views, a.praise, a.favorites, "
               "a.create_time, a.update_time");

    if (category > 0) {
        qb->join("article_category_rel acr", "a.id = acr.article_id");
        qb->where("acr.category_id", "=", (int64_t)category);
        qb->where("acr.is_deleted", "=", (int64_t)0);
    }
    if (role != -1) {
        qb->join("user u", "a.user_id = u.id");
        qb->where("u.role", "=", (int64_t)role);
        qb->where("u.is_deleted", "=", (int64_t)0);
    }
    qb->whereIf(state != 0, "a.state", "=", (int64_t)state);
    qb->whereIf(valid, "a.is_deleted", "=", (int64_t)0);
    if (days > 0) {
        qb->whereSQL("a.create_time >= ?", (int64_t)(chen::TimeUtil::GetCurrentSec() - days * 24 * 3600));
    }
    qb->orderBy("a.id", "DESC");

    int64_t total = 0;
    if (DB::ArticleInfoDao::QueryByBuilderPages(infos, total, qb, offset, size, conn)) {
        return 0;
    }
    return total;
}

int64_t ArticleQueryRepository::ListByLabelPages(std::vector<DB::ArticleInfo::ptr>& infos,
    int64_t label_id,
    int32_t offset,
    int32_t size,
    bool valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::ArticleInfoDao::newQuery("a");
    if (!qb) {
        return 0;
    }
    qb->select("a.id, a.user_id, a.title, a.content, a.type, a.state, a.channel, a.is_deleted, a.publish_time, "
               "a.weight, a.views, a.praise, a.favorites, "
               "a.create_time, a.update_time");
    qb->join("article_label_rel alr", "a.id = alr.article_id");
    qb->where("alr.label_id", "=", label_id);
    qb->where("alr.is_deleted", "=", (int64_t)0);
    qb->where("a.state", "=", (int64_t)kStatePublished);
    qb->whereIf(valid, "a.is_deleted", "=", (int64_t)0);
    qb->orderBy("a.id", "DESC");

    std::string list_key = "art:lbl:" + std::to_string(label_id) + ":" + (valid ? "1" : "0") + ":"
        + std::to_string(offset) + ":" + std::to_string(size);
    std::vector<int64_t> cached_ids;
    if (GetCachedListResult(list_key, cached_ids)) {
        for (auto id : cached_ids) {
            auto info = ArticleRepository::Get(id, conn);
            if (info) {
                infos.push_back(info);
            }
        }
        return ExecuteCountCached(qb, conn, "art:lbl:" + std::to_string(label_id) + ":" + (valid ? "1" : "0"));
    }

    int64_t total = 0;
    if (DB::ArticleInfoDao::QueryByBuilderPages(infos, total, qb, offset, size, conn)) {
        return 0;
    }
    std::vector<int64_t> ids;
    ids.reserve(infos.size());
    for (auto& info : infos) {
        ids.push_back(info->getId());
    }
    if (!ids.empty()) {
        CacheListResult(list_key, ids);
    }
    return total;
}

int64_t ArticleQueryRepository::ListByCategoryPages(std::vector<DB::ArticleInfo::ptr>& infos,
    int64_t category_id,
    int32_t offset,
    int32_t size,
    bool valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::ArticleInfoDao::newQuery("a");
    if (!qb) {
        return 0;
    }
    qb->select("a.id, a.user_id, a.title, a.content, a.type, a.state, a.channel, a.is_deleted, a.publish_time, "
               "a.weight, a.views, a.praise, a.favorites, "
               "a.create_time, a.update_time");
    qb->join("article_category_rel acr", "a.id = acr.article_id");
    qb->where("acr.category_id", "=", category_id);
    qb->where("acr.is_deleted", "=", (int64_t)0);
    qb->where("a.state", "=", (int64_t)kStatePublished);
    qb->whereIf(valid, "a.is_deleted", "=", (int64_t)0);
    qb->orderBy("a.id", "DESC");

    std::string list_key = "art:cat:" + std::to_string(category_id) + ":" + (valid ? "1" : "0") + ":"
        + std::to_string(offset) + ":" + std::to_string(size);
    std::vector<int64_t> cached_ids;
    if (GetCachedListResult(list_key, cached_ids)) {
        for (auto id : cached_ids) {
            auto info = ArticleRepository::Get(id, conn);
            if (info) {
                infos.push_back(info);
            }
        }
        return ExecuteCountCached(qb, conn, "art:cat:" + std::to_string(category_id) + ":" + (valid ? "1" : "0"));
    }

    int64_t total = 0;
    if (DB::ArticleInfoDao::QueryByBuilderPages(infos, total, qb, offset, size, conn)) {
        return 0;
    }
    std::vector<int64_t> ids;
    ids.reserve(infos.size());
    for (auto& info : infos) {
        ids.push_back(info->getId());
    }
    if (!ids.empty()) {
        CacheListResult(list_key, ids);
    }
    return total;
}

std::pair<DB::ArticleInfo::ptr, DB::ArticleInfo::ptr> ArticleQueryRepository::Nearby(int64_t id, chen::IDB::ptr conn)
{
    if (!conn) {
        return std::pair(nullptr, nullptr);
    }

    DB::ArticleInfo::ptr prev;
    {
        auto qb = DB::ArticleInfoDao::newQuery();
        if (qb) {
            qb->whereSQL("id < ?", id);
            qb->where("state", "=", (int64_t)kStatePublished);
            qb->where("is_deleted", "=", (int64_t)0);
            qb->orderBy("id", "DESC");
            qb->limit(1);
            std::vector<DB::ArticleInfo::ptr> results;
            if (DB::ArticleInfoDao::QueryByBuilder(results, qb, conn) == 0 && !results.empty()) {
                prev = results[0];
            }
        }
    }

    DB::ArticleInfo::ptr next;
    {
        auto qb = DB::ArticleInfoDao::newQuery();
        if (qb) {
            qb->whereSQL("id > ?", id);
            qb->where("state", "=", (int64_t)kStatePublished);
            qb->where("is_deleted", "=", (int64_t)0);
            qb->orderBy("id", "ASC");
            qb->limit(1);
            std::vector<DB::ArticleInfo::ptr> results;
            if (DB::ArticleInfoDao::QueryByBuilder(results, qb, conn) == 0 && !results.empty()) {
                next = results[0];
            }
        }
    }
    return std::pair(prev, next);
}

ArticleQueryRepository::Stats ArticleQueryRepository::GetStats(int32_t category,
    int32_t role,
    int32_t days,
    const std::string& keyword,
    chen::IDB::ptr conn)
{
    Stats stats;
    if (!conn) {
        return stats;
    }

    if (category <= 0 && role == -1 && days <= 0 && keyword.empty()) {
        std::string cached;
        if (GetCachedStringResult("art:stats", cached)) {
            std::stringstream ss(cached);
            std::string token;
            auto next = [&]() -> int64_t {
                std::getline(ss, token, '|');
                return chen::TypeUtil::Atoi(token);
            };
            stats.total = next();
            stats.pending = next();
            stats.published = next();
            stats.rejected = next();
            stats.reported = next();
            return stats;
        }
    }

    auto qb = DB::ArticleInfoDao::newQuery("a");
    if (!qb) {
        return stats;
    }
    qb->select("a.state, COUNT(*) as cnt");
    if (category > 0) {
        qb->join("article_category_rel acr", "a.id = acr.article_id");
        qb->where("acr.category_id", "=", (int64_t)category);
        qb->where("acr.is_deleted", "=", (int64_t)0);
    }
    if (role != -1) {
        qb->join("user u", "a.user_id = u.id");
        qb->where("u.role", "=", (int64_t)role);
        qb->where("u.is_deleted", "=", (int64_t)0);
    }
    qb->where("a.is_deleted", "=", (int64_t)0);
    if (days > 0) {
        qb->whereSQL("a.create_time >= ?", (int64_t)(chen::TimeUtil::GetCurrentSec() - days * 24 * 3600));
    }
    if (!keyword.empty()) {
        qb->where("a.title", "LIKE", "%" + keyword + "%");
    }
    qb->groupBy("a.state");

    std::vector<std::pair<int32_t, int64_t>> rows;
    if (qb->queryPairs<int32_t, int64_t>(rows, conn)) {
        return stats;
    }
    for (auto& [st, cnt] : rows) {
        stats.total += cnt;
        if (st == kStateChecking) {
            stats.pending += cnt;
        } else if (st == kStatePublished) {
            stats.published += cnt;
        } else if (st == kStateRejected) {
            stats.rejected += cnt;
        }
    }

    if (category <= 0 && role == -1 && days <= 0 && keyword.empty()) {
        std::stringstream ss;
        ss << stats.total << "|" << stats.pending << "|" << stats.published << "|" << stats.rejected << "|"
           << stats.reported;
        CacheStringResult("art:stats", ss.str());
    }
    return stats;
}

void ArticleQueryRepository::CountByUsers(const std::vector<int64_t>& user_ids,
    std::vector<std::pair<int64_t, int64_t>>& rows,
    chen::IDB::ptr conn)
{
    if (!conn || user_ids.empty()) {
        return;
    }
    auto qb = DB::ArticleInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->select("user_id, COUNT(*) as cnt");
    qb->whereIn("user_id", user_ids);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->groupBy("user_id");
    qb->queryPairs<int64_t, int64_t>(rows, conn);
}

int64_t ArticleQueryRepository::SumPublishedViews(chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::ArticleInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->select("CAST(COALESCE(SUM(views), 0) AS SIGNED)");
    qb->where("state", "=", (int64_t)kStatePublished);
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    qb->queryScalarInt64(total, conn);
    return total;
}

void ArticleQueryRepository::ListPublishedDays(int64_t user_id,
    const std::string& start_time,
    const std::string& end_time,
    std::vector<int32_t>& days,
    chen::IDB::ptr conn)
{
    if (!conn || user_id <= 0) {
        return;
    }
    auto qb = DB::ArticleInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->select("DISTINCT DAY(publish_time) as d");
    qb->where("user_id", "=", user_id);
    qb->where("state", "=", (int64_t)kStatePublished);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("publish_time", ">=", start_time);
    qb->where("publish_time", "<", end_time);
    qb->orderBy("d", "ASC");
    qb->queryColumn<int32_t>(days, conn, "d");
}

bool ArticleQueryRepository::ListSnapshotAfter(int64_t last_id,
    int32_t limit,
    std::vector<DB::ArticleInfo::ptr>& infos,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return false;
    }
    auto qb = DB::ArticleInfoDao::newQuery();
    if (!qb) {
        return false;
    }
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("id", ">", last_id);
    qb->orderBy("id", "ASC");
    qb->limit(limit);
    return DB::ArticleInfoDao::QueryByBuilder(infos, qb, conn) == 0;
}
