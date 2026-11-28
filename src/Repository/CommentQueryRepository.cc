#include "CommentQueryRepository.hpp"

#include <sstream>
#include <utility>

#include <Chen/DB/Redis.hpp>
#include <Chen/Util/TypeUtil.hpp>

#include "../Config/AppConfig.hpp"
#include "../Manager/CacheUtil.hpp"
#include "../Util/Util.hpp"
#include "CommentRepository.hpp"

constexpr int32_t kStatusPending = 1;
constexpr int32_t kStatusApproved = 2;
constexpr int32_t kStatusSpam = 4;

static std::string ArticleVersionKey(int64_t article_id)
{
    return "cmt:art:ver:" + std::to_string(article_id);
}

static int64_t GetArticleVersion(int64_t article_id)
{
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get %s", ArticleVersionKey(article_id).c_str());
    if (!rpy || !rpy->str) {
        return 0;
    }
    return chen::TypeUtil::Atoi(rpy->str);
}

void CommentQueryRepository::ListAllByArticle(std::vector<DB::CommentInfo::ptr>& results,
    int64_t article_id,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return;
    }
    auto qb = DB::CommentInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->where("article_id", "=", article_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("status", "=", (int64_t)kStatusApproved);
    qb->orderBy("id", "ASC");
    DB::CommentInfoDao::QueryByBuilder(results, qb, conn);
}

void CommentQueryRepository::ListByArticle(std::vector<DB::CommentInfo::ptr>& results,
    int64_t article_id,
    uint64_t offset,
    uint64_t size,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return;
    }

    std::string list_key = "cmt:art:" + std::to_string(article_id) + ":v"
        + std::to_string(GetArticleVersion(article_id)) + ":" + std::to_string(offset) + ":" + std::to_string(size);
    std::vector<int64_t> cached_ids;
    if (GetCachedListResult(list_key, cached_ids)) {
        for (auto id : cached_ids) {
            auto info = CommentRepository::Get(id, conn);
            if (info) {
                results.push_back(info);
            }
        }
        return;
    }

    auto qb = DB::CommentInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->where("article_id", "=", article_id);
    qb->where("parent_id", "=", (int64_t)0);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("status", "=", (int64_t)kStatusApproved);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);
    if (DB::CommentInfoDao::QueryByBuilder(results, qb, conn)) {
        return;
    }

    std::vector<int64_t> ids;
    ids.reserve(results.size());
    for (auto& info : results) {
        ids.push_back(info->getId());
    }
    if (!ids.empty()) {
        CacheListResult(list_key, ids);
    }
}

void CommentQueryRepository::ListReplies(std::vector<DB::CommentInfo::ptr>& results,
    int64_t parent_id,
    uint64_t offset,
    uint64_t size,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return;
    }
    auto qb = DB::CommentInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->where("parent_id", "=", parent_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("status", "=", (int64_t)kStatusApproved);
    qb->orderBy("id", "ASC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);
    DB::CommentInfoDao::QueryByBuilder(results, qb, conn);
}

int64_t CommentQueryRepository::CountByArticle(int64_t article_id, chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::CommentInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("article_id", "=", article_id);
    qb->where("parent_id", "=", (int64_t)0);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("status", "=", (int64_t)kStatusApproved);
    return ExecuteCountCached(qb, conn, "cmt:cnt:" + std::to_string(article_id));
}

int64_t CommentQueryRepository::CountReplies(int64_t parent_id, chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::CommentInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("parent_id", "=", parent_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("status", "=", (int64_t)kStatusApproved);
    return ExecuteCountCached(qb, conn, "cmt:reply:" + std::to_string(parent_id));
}

int64_t CommentQueryRepository::ListByAdmin(std::vector<DB::CommentInfo::ptr>& results,
    int64_t page_num,
    int64_t page_size,
    int32_t status,
    const std::string& keyword,
    int64_t article_id,
    int32_t is_reported,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::CommentInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("is_deleted", "=", (int64_t)0);
    qb->whereIf(status > 0, "status", "=", (int64_t)status);
    qb->whereIf(article_id > 0, "article_id", "=", article_id);
    qb->whereIf(is_reported >= 0, "is_reported", "=", (int64_t)is_reported);
    qb->whereIf(!keyword.empty(), "content", "LIKE", "%" + keyword + "%");
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    int32_t offset = (int32_t)((page_num > 0 ? page_num - 1 : 0) * page_size);
    if (DB::CommentInfoDao::QueryByBuilderPages(results, total, qb, offset, (int32_t)page_size, conn)) {
        return 0;
    }
    return total;
}

CommentQueryRepository::Stats CommentQueryRepository::GetStats(chen::IDB::ptr conn)
{
    Stats stats;
    if (!conn) {
        return stats;
    }

    std::string cached;
    if (GetCachedStringResult("cmt:stats", cached)) {
        std::stringstream ss(cached);
        std::string token;
        auto next = [&]() -> int64_t {
            std::getline(ss, token, '|');
            return chen::TypeUtil::Atoi(token);
        };
        stats.total = next();
        stats.pending = next();
        stats.approved = next();
        stats.spam = next();
        stats.reported = next();
        return stats;
    }

    {
        auto qb = DB::CommentInfoDao::newQuery();
        if (qb) {
            qb->select("status, COUNT(*) AS cnt");
            qb->where("is_deleted", "=", (int64_t)0);
            qb->groupBy("status");
            std::vector<std::pair<int32_t, int64_t>> rows;
            if (qb->queryPairs<int32_t, int64_t>(rows, conn) == 0) {
                for (auto& [status, count] : rows) {
                    stats.total += count;
                    if (status == kStatusPending) {
                        stats.pending = count;
                    } else if (status == kStatusApproved) {
                        stats.approved = count;
                    } else if (status == kStatusSpam) {
                        stats.spam = count;
                    }
                }
            }
        }
    }

    {
        auto qb = DB::CommentInfoDao::newQuery();
        if (qb) {
            qb->where("is_deleted", "=", (int64_t)0);
            qb->where("is_reported", "=", (int64_t)1);
            int64_t reported = 0;
            if (qb->executeCount(reported, conn) == 0) {
                stats.reported = reported;
            }
        }
    }

    std::stringstream ss;
    ss << stats.total << "|" << stats.pending << "|" << stats.approved << "|" << stats.spam << "|" << stats.reported;
    CacheStringResult("cmt:stats", ss.str());
    return stats;
}

void CommentQueryRepository::InvalidateArticle(int64_t article_id)
{
    if (article_id <= 0) {
        return;
    }
    chen::RedisUtil::Cmd(GetRedisPoolName(), "del cache:count:cmt:cnt:%lld", (long long)article_id);
    const std::string version_key = ArticleVersionKey(article_id);
    chen::RedisUtil::Cmd(GetRedisPoolName(), "incr %s", version_key.c_str());
    chen::RedisUtil::Cmd(GetRedisPoolName(), "expire %s %d", version_key.c_str(), kIdMapCacheTTL);
}

void CommentQueryRepository::InvalidateReplies(int64_t parent_id)
{
    if (parent_id <= 0) {
        return;
    }
    chen::RedisUtil::Cmd(GetRedisPoolName(), "del cache:count:cmt:reply:%lld", (long long)parent_id);
}

void CommentQueryRepository::InvalidateStats()
{
    chen::RedisUtil::Cmd(GetRedisPoolName(), "del cache:str:cmt:stats");
}

void CommentQueryRepository::CountByUsers(const std::vector<int64_t>& user_ids,
    std::vector<std::pair<int64_t, int64_t>>& rows,
    chen::IDB::ptr conn)
{
    if (!conn || user_ids.empty()) {
        return;
    }
    auto qb = DB::CommentInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->select("user_id, COUNT(*) as cnt");
    qb->whereIn("user_id", user_ids);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->groupBy("user_id");
    qb->queryPairs<int64_t, int64_t>(rows, conn);
}

int64_t CommentQueryRepository::CountByArticles(const std::vector<int64_t>& article_ids, chen::IDB::ptr conn)
{
    if (!conn || article_ids.empty()) {
        return 0;
    }
    auto qb = DB::CommentInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->whereIn("article_id", article_ids);
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    if (qb->executeCount(total, conn)) {
        return 0;
    }
    return total;
}

void CommentQueryRepository::ListByUserId(std::vector<DB::CommentInfo::ptr>& results,
    int64_t user_id,
    chen::IDB::ptr conn)
{
    if (!conn || user_id <= 0) {
        return;
    }
    DB::CommentInfoDao::QueryByUserId(results, user_id, conn);
}

void CommentQueryRepository::ListCreateTimesByUser(int64_t user_id,
    int64_t cutoff,
    std::vector<int64_t>& timestamps,
    chen::IDB::ptr conn)
{
    if (!conn || user_id <= 0) {
        return;
    }
    auto qb = DB::CommentInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->select("create_time");
    qb->where("user_id", "=", user_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->whereSQL("create_time >= FROM_UNIXTIME(?)", cutoff);
    qb->queryColumn<int64_t>(timestamps, conn, "create_time");
}
