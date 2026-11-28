#include "ArticleCounterService.hpp"

#include <map>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include <Chen/DB/Redis.hpp>
#include <Chen/IOManager/IOManager.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>
#include <Chen/Util/TypeUtil.hpp>

#include "DB/ArticleInfo.hpp"

#include "../Config/AppConfig.hpp"
#include "../Repository/ArticleQueryRepository.hpp"
#include "../Repository/ArticleRepository.hpp"
#include "../Util/FiberMutex.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

/// 浏览去重窗口（秒）
static const int kViewDedupTTL = 10 * 60;

static FiberMutex g_counter_mutex;

/**
 * @brief 进程内待刷盘计数增量
 * @details 多实例部署时每个实例各持一份增量，`FlushDirty` 用
 *          `views = views + ?` 增量写库，避免多实例之间用绝对值互相覆盖。
 */
struct ArticleCounterServiceDelta {
    int64_t views = 0;
    int64_t praise = 0;
    int64_t favorites = 0;
};

static std::shared_mutex g_deltas_mutex;
static std::map<int64_t, ArticleCounterServiceDelta> g_deltas;

static void AddDelta(int64_t id, int64_t views, int64_t praise, int64_t favorites)
{
    if (id <= 0 || (views == 0 && praise == 0 && favorites == 0)) {
        return;
    }
    std::unique_lock<std::shared_mutex> lock(g_deltas_mutex);
    auto& delta = g_deltas[id];
    delta.views += views;
    delta.praise += praise;
    delta.favorites += favorites;
    if (delta.views == 0 && delta.praise == 0 && delta.favorites == 0) {
        g_deltas.erase(id);
    }
}

/// 刷盘失败时把增量合并回本地，等待下一轮重试
static void MergeDeltas(const std::map<int64_t, ArticleCounterServiceDelta>& deltas)
{
    if (deltas.empty()) {
        return;
    }
    std::unique_lock<std::shared_mutex> lock(g_deltas_mutex);
    for (const auto& [id, delta] : deltas) {
        auto& target = g_deltas[id];
        target.views += delta.views;
        target.praise += delta.praise;
        target.favorites += delta.favorites;
        if (target.views == 0 && target.praise == 0 && target.favorites == 0) {
            g_deltas.erase(id);
        }
    }
}

static bool AddViews(uint64_t id, const std::string& cookie_id)
{
    std::string key = "art:view:" + std::to_string(id) + ":" + cookie_id;
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "set %s 1 ex %d nx", key.c_str(), kViewDedupTTL);
    if (!rpy) {
        return true;
    }
    return rpy->type != REDIS_REPLY_NIL;
}

bool ArticleCounterService::IncViews(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    FiberMutex::Lock lock(g_counter_mutex);
    auto info = ArticleRepository::Get(id, GetDB());
    if (!info) {
        return false;
    }
    if (AddViews(id, cookie_id)) {
        info->setViews(info->getViews() + 1);
        ArticleRepository::Put(info);
        AddDelta(id, 1, 0, 0);
        chen::IOManager::GetThis()->schedule([user_id]() {
            chen::RedisUtil::Cmd(GetRedisPoolName(), "incr blog:total_visits");
            auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "incr blog:today_visits");
            if (rpy && rpy->integer == 1) {
                int64_t now = chen::TimeUtil::GetCurrentSec();
                chen::RedisUtil::Cmd(
                    GetRedisPoolName(), "expireat blog:today_visits %lld", chen::TimeUtil::GetDayStart(now + 86400));
            }
            chen::RedisUtil::Cmd(GetRedisPoolName(), "pfadd blog:visitors %lld", user_id);
        });
    }
    return true;
}

bool ArticleCounterService::IncPraise(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    FiberMutex::Lock lock(g_counter_mutex);
    auto info = ArticleRepository::Get(id, GetDB());
    if (!info) {
        return false;
    }
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "hexist pra_a2u:%lld %lld", id, user_id);
    if (!rpy) {
        ERROR(logger) << "hexists fail";
        return false;
    }
    if (rpy->integer == 1) {
        return true;
    }
    rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "hset pra_a2u:%lld %lld %lld", id, user_id, time(0));
    if (!rpy) {
        ERROR(logger) << "hset fail";
        return false;
    }
    chen::IOManager::GetThis()->schedule([id, user_id]() {
        chen::RedisUtil::Cmd(GetRedisPoolName(), "hset pra_u2a:%lld %lld %lld", user_id, id, time(0));
    });
    info->setPraise(info->getPraise() + 1);
    ArticleRepository::Put(info);
    AddDelta(id, 0, 1, 0);
    return true;
}

bool ArticleCounterService::IncFavorites(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    FiberMutex::Lock lock(g_counter_mutex);
    auto info = ArticleRepository::Get(id, GetDB());
    if (!info) {
        return false;
    }
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "hexist fav_a2u:%lld %lld", id, user_id);
    if (!rpy) {
        ERROR(logger) << "hexists fail";
        return false;
    }
    if (rpy->integer == 1) {
        return true;
    }
    rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "hset fav_a2u:%lld %lld %lld", id, user_id, time(0));
    if (!rpy) {
        ERROR(logger) << "hset fail";
        return false;
    }
    chen::IOManager::GetThis()->schedule([id, user_id]() {
        chen::RedisUtil::Cmd(GetRedisPoolName(), "hset fav_u2a:%lld %lld %lld", user_id, id, time(0));
    });
    info->setFavorites(info->getFavorites() + 1);
    ArticleRepository::Put(info);
    AddDelta(id, 0, 0, 1);
    return true;
}

bool ArticleCounterService::DecPraise(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    FiberMutex::Lock lock(g_counter_mutex);
    auto info = ArticleRepository::Get(id, GetDB());
    if (!info) {
        return false;
    }
    bool changed = false;
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "hdel pra_a2u:%lld %lld", id, user_id);
    if (!rpy) {
        ERROR(logger) << "hdel fail";
        return false;
    }
    if (rpy->integer == 1) {
        changed = true;
    }
    rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "hdel pra_u2a:%lld %lld", user_id, id);
    if (!rpy) {
        ERROR(logger) << "hdel fail";
        return false;
    }
    if (rpy->integer == 1) {
        changed = true;
    }
    if (changed) {
        info->setPraise(info->getPraise() - 1);
        ArticleRepository::Put(info);
        AddDelta(id, 0, -1, 0);
    }
    return true;
}

bool ArticleCounterService::DecFavorites(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    FiberMutex::Lock lock(g_counter_mutex);
    auto info = ArticleRepository::Get(id, GetDB());
    if (!info) {
        return false;
    }
    bool changed = false;
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "hdel fav_a2u:%lld %lld", id, user_id);
    if (!rpy) {
        ERROR(logger) << "hdel fail";
        return false;
    }
    if (rpy->integer == 1) {
        changed = true;
    }
    rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "hdel fav_u2a:%lld %lld", user_id, id);
    if (!rpy) {
        ERROR(logger) << "hdel fail";
        return false;
    }
    if (rpy->integer == 1) {
        changed = true;
    }
    if (changed) {
        info->setFavorites(info->getFavorites() - 1);
        ArticleRepository::Put(info);
        AddDelta(id, 0, 0, -1);
    }
    return true;
}

void ArticleCounterService::IncPraiseCount(int64_t id)
{
    FiberMutex::Lock lock(g_counter_mutex);
    auto info = ArticleRepository::Get(id, GetDB());
    if (!info) {
        return;
    }
    info->setPraise(info->getPraise() + 1);
    ArticleRepository::Put(info);
    AddDelta(id, 0, 1, 0);
}

void ArticleCounterService::DecPraiseCount(int64_t id)
{
    FiberMutex::Lock lock(g_counter_mutex);
    auto info = ArticleRepository::Get(id, GetDB());
    if (!info) {
        return;
    }
    if (info->getPraise() > 0) {
        info->setPraise(info->getPraise() - 1);
        ArticleRepository::Put(info);
        AddDelta(id, 0, -1, 0);
    }
}

#define PROC(id, mask, out)                                                                                            \
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), mask, id);                                                     \
    if (!rpy) {                                                                                                        \
        ERROR(logger) << "hgetall fail";                                                                               \
        return false;                                                                                                  \
    }                                                                                                                  \
    for (size_t i = 0; i + 1 < rpy->elements; i += 2) {                                                                \
        out[chen::TypeUtil::Atoi(rpy->element[i]->str)] = chen::TypeUtil::Atoi(rpy->element[i + 1]->str);              \
    }                                                                                                                  \
    return true;

bool ArticleCounterService::ListUserFav(int64_t id, std::map<int64_t, int64_t>& articles)
{
    PROC(id, "hgetall fav_u2a:%lld", articles);
}

bool ArticleCounterService::ListUserPra(int64_t id, std::map<int64_t, int64_t>& articles)
{
    PROC(id, "hgetall pra_u2a:%lld", articles);
}

bool ArticleCounterService::ListArticleFav(int64_t id, std::map<int64_t, int64_t>& users)
{
    PROC(id, "hgetall fav_a2u:%lld", users);
}

bool ArticleCounterService::ListArticlePra(int64_t id, std::map<int64_t, int64_t>& users)
{
    PROC(id, "hgetall pra_a2u:%lld", users);
}
#undef PROC

int64_t ArticleCounterService::GetTodayViews()
{
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get blog:today_visits");
    if (rpy && rpy->str) {
        return chen::TypeUtil::Atoi(rpy->str);
    }
    return 0;
}

int64_t ArticleCounterService::GetTotalViews()
{
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get blog:total_visits");
    if (rpy && rpy->str) {
        return chen::TypeUtil::Atoi(rpy->str);
    }
    auto db = GetDB();
    if (!db) {
        return 0;
    }
    int64_t total = ArticleQueryRepository::SumPublishedViews(db);
    chen::RedisUtil::Cmd(GetRedisPoolName(), "set blog:total_visits %lld", total);
    return total;
}

int64_t ArticleCounterService::GetTotalVisitors()
{
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "pfcount blog:visitors");
    if (rpy) {
        return rpy->integer;
    }
    return 0;
}

void ArticleCounterService::SyncStatsFromDB()
{
    chen::RedisUtil::Cmd(GetRedisPoolName(), "del blog:total_visits");
    int64_t total = GetTotalViews();
    INFO(logger) << "SyncStatsFromDB: blog:total_visits recalculated from DB = " << total;
}

void ArticleCounterService::FlushDirty()
{
    std::map<int64_t, ArticleCounterServiceDelta> deltas;
    {
        std::unique_lock<std::shared_mutex> lock(g_deltas_mutex);
        deltas.swap(g_deltas);
    }
    if (deltas.empty()) {
        return;
    }

    auto conn = GetDB();
    if (!conn) {
        ERROR(logger) << "FlushDirty: get db connect fail";
        MergeDeltas(deltas);
        return;
    }
    auto trans = conn->openTransaction();
    if (!trans) {
        ERROR(logger) << "FlushDirty: open transaction failed";
        MergeDeltas(deltas);
        return;
    }

    // 增量写：多实例各自刷自己的增量，DB 侧不会出现“绝对值互相覆盖”
    auto stmt = conn->prepare(
        "UPDATE article SET views = views + ?, praise = praise + ?, favorites = favorites + ? WHERE id = ?");
    if (!stmt) {
        ERROR(logger) << "FlushDirty: prepare failed";
        MergeDeltas(deltas);
        return;
    }

    std::map<int64_t, ArticleCounterServiceDelta> failed;
    for (const auto& [id, delta] : deltas) {
        if (delta.views == 0 && delta.praise == 0 && delta.favorites == 0) {
            continue;
        }
        stmt->bindInt64(1, delta.views);
        stmt->bindInt64(2, delta.praise);
        stmt->bindInt64(3, delta.favorites);
        stmt->bindInt64(4, id);
        if (stmt->execute() != 0) {
            ERROR(logger) << "FlushDirty: update failed, id=" << id << ", err=" << stmt->getErrStr();
            failed[id] = delta;
        }
    }

    if (!trans->commit()) {
        ERROR(logger) << "FlushDirty: commit failed";
        MergeDeltas(deltas);
        return;
    }

    // 写库成功：让对象缓存下次从 DB 重新加载，避免缓存值与 DB 长期漂移
    for (const auto& [id, delta] : deltas) {
        if (failed.find(id) == failed.end()) {
            ArticleRepository::Evict(id);
        }
    }
    MergeDeltas(failed);
    DEBUG(logger) << "FlushDirty: flushed " << (deltas.size() - failed.size()) << " articles";
}
