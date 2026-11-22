#include "ArticleCounterService.hpp"

#include <mutex>
#include <set>
#include <shared_mutex>
#include <vector>

#include <Chen/DB/Redis.hpp>
#include <Chen/IOManager/IOManager.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>
#include <Chen/Util/TypeUtil.hpp>

#include "DB/ArticleInfo.hpp"

#include "../Config/AppConfig.hpp"
#include "../Manager/ArticleManager.hpp"
#include "../Repository/ArticleRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

/// 浏览去重窗口（秒）
static const int kViewDedupTTL = 10 * 60;

static std::shared_mutex g_updates_mutex;
static std::set<int64_t> g_updates;
static std::mutex g_counter_mutex;

namespace {

void AddUpdate(int64_t id)
{
    std::unique_lock<std::shared_mutex> lock(g_updates_mutex);
    g_updates.insert(id);
}

bool AddViews(uint64_t id, const std::string& cookie_id)
{
    std::string key = "art:view:" + std::to_string(id) + ":" + cookie_id;
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "set %s 1 ex %d nx", key.c_str(), kViewDedupTTL);
    if (!rpy) {
        return true;
    }
    return rpy->type != REDIS_REPLY_NIL;
}

} // namespace

bool ArticleCounterService::IncViews(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    std::lock_guard<std::mutex> lock(g_counter_mutex);
    auto info = ArticleRepository::Get(id, GetDB());
    if (!info) {
        return false;
    }
    if (AddViews(id, cookie_id)) {
        info->setViews(info->getViews() + 1);
        ArticleRepository::Put(info);
        AddUpdate(id);
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
    std::lock_guard<std::mutex> lock(g_counter_mutex);
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
    AddUpdate(id);
    return true;
}

bool ArticleCounterService::IncFavorites(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    std::lock_guard<std::mutex> lock(g_counter_mutex);
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
    AddUpdate(id);
    return true;
}

bool ArticleCounterService::DecPraise(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    std::lock_guard<std::mutex> lock(g_counter_mutex);
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
        AddUpdate(id);
    }
    return true;
}

bool ArticleCounterService::DecFavorites(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    std::lock_guard<std::mutex> lock(g_counter_mutex);
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
        AddUpdate(id);
    }
    return true;
}

void ArticleCounterService::IncPraiseCount(int64_t id)
{
    std::lock_guard<std::mutex> lock(g_counter_mutex);
    auto info = ArticleRepository::Get(id, GetDB());
    if (!info) {
        return;
    }
    info->setPraise(info->getPraise() + 1);
    ArticleRepository::Put(info);
    AddUpdate(id);
}

void ArticleCounterService::DecPraiseCount(int64_t id)
{
    std::lock_guard<std::mutex> lock(g_counter_mutex);
    auto info = ArticleRepository::Get(id, GetDB());
    if (!info) {
        return;
    }
    if (info->getPraise() > 0) {
        info->setPraise(info->getPraise() - 1);
        ArticleRepository::Put(info);
        AddUpdate(id);
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
    auto qb = DB::ArticleInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->select("CAST(COALESCE(SUM(views), 0) AS SIGNED)");
    qb->where("state", "=", (int64_t)ArticleManager::Status::PUBLISHED);
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    qb->queryScalarInt64(total, db);
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
    std::set<int64_t> updates;
    {
        std::unique_lock<std::shared_mutex> lock(g_updates_mutex);
        updates.swap(g_updates);
    }
    if (updates.empty()) {
        return;
    }

    auto conn = GetDB();
    if (!conn) {
        ERROR(logger) << "FlushDirty: get db connect fail";
        std::unique_lock<std::shared_mutex> lock(g_updates_mutex);
        for (auto& id : updates) {
            g_updates.insert(id);
        }
        return;
    }
    auto trans = conn->openTransaction();
    if (!trans) {
        ERROR(logger) << "FlushDirty: open transaction failed";
        std::unique_lock<std::shared_mutex> lock(g_updates_mutex);
        for (auto& id : updates) {
            g_updates.insert(id);
        }
        return;
    }

    std::vector<DB::ArticleInfo::ptr> updated;
    for (auto& id : updates) {
        auto info = ArticleRepository::Get(id, conn);
        if (!info) {
            continue;
        }
        // Redis 反序列化后对象是 clean 的，这里显式标记计数字段，确保能写回 DB
        info->setViews(info->getViews());
        info->setPraise(info->getPraise());
        info->setFavorites(info->getFavorites());
        if (!ArticleRepository::Update(info, conn)) {
            AddUpdate(id);
        } else {
            updated.push_back(info);
        }
    }
    if (!trans->commit()) {
        ERROR(logger) << "FlushDirty: commit failed";
        std::unique_lock<std::shared_mutex> lock(g_updates_mutex);
        for (auto& id : updates) {
            g_updates.insert(id);
        }
        return;
    }
    for (const auto& info : updated) {
        ArticleRepository::Put(info);
    }
    INFO(logger) << "FlushDirty: flushed " << updates.size() << " articles";
}
