#include "ArticleScheduleService.hpp"

#include <cstdio>
#include <cstring>
#include <ctime>

#include <json/json.h>

#include <Chen/DB/Redis.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/JsonUtil.hpp>
#include <Chen/Util/TimeUtil.hpp>
#include <Chen/Util/TypeUtil.hpp>

#include "DB/ArticleInfo.hpp"

#include "../Config/AppConfig.hpp"
#include "../Manager/ArticleManager.hpp"
#include "../Repository/ArticleRepository.hpp"
#include "../Search/SearchSync.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

static const char* kScheduleKey = "article:schedule";

void ArticleScheduleService::Schedule(int64_t article_id, int64_t publish_time)
{
    chen::RedisUtil::Cmd(
        GetRedisPoolName(), "zadd %s %lld %lld", kScheduleKey, (long long)publish_time, (long long)article_id);
}

void ArticleScheduleService::Unschedule(int64_t article_id)
{
    chen::RedisUtil::Cmd(GetRedisPoolName(), "zrem %s %lld", kScheduleKey, (long long)article_id);
}

void ArticleScheduleService::PublishDue()
{
    time_t now = chen::TimeUtil::GetCurrentSec();
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "zrangebyscore %s 0 %lld", kScheduleKey, now);
    if (!rpy || rpy->type != REDIS_REPLY_ARRAY || rpy->elements == 0) {
        return;
    }

    std::vector<int64_t> article_ids;
    for (size_t i = 0; i < rpy->elements; ++i) {
        if (rpy->element[i]->type == REDIS_REPLY_STRING && rpy->element[i]->str) {
            article_ids.push_back(chen::TypeUtil::Atoi(rpy->element[i]->str));
        }
    }
    if (article_ids.empty()) {
        return;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "PublishDue: get db failed";
        return;
    }

    // 先原子领取，避免多 worker 重复发布
    std::string zrem_cmd = "zrem " + std::string(kScheduleKey);
    for (auto id : article_ids) {
        zrem_cmd += " " + std::to_string(id);
    }
    chen::RedisUtil::Cmd(GetRedisPoolName(), zrem_cmd.c_str());

    auto trans = db->openTransaction();
    if (!trans) {
        ERROR(logger) << "PublishDue: open transaction failed";
        return;
    }

    std::vector<int64_t> published_ids;
    std::vector<DB::ArticleInfo::ptr> published;
    for (auto id : article_ids) {
        auto info = ArticleRepository::Get(id, db);
        if (!info || info->getIsDeleted()) {
            continue;
        }
        if (info->getState() == ArticleManager::Status::PUBLISHED) {
            continue;
        }

        info->setState(ArticleManager::Status::PUBLISHED);
        info->setUpdateTime(now);
        if (!ArticleRepository::Update(info, db)) {
            ERROR(logger) << "PublishDue update failed, id=" << id;
            continue;
        }
        published.push_back(info);
        published_ids.push_back(info->getId());
    }
    if (!trans->commit()) {
        ERROR(logger) << "PublishDue commit failed";
        return;
    }

    for (const auto& info : published) {
        ArticleRepository::Put(info);
        ClearCalendarCache(info->getUserId(), info->getPublishTime());
    }
    for (auto id : published_ids) {
        SearchSyncMgr::GetInstance()->markDirty(id, SearchSync::OP_UPSERT);
    }
}

void ArticleScheduleService::GetCalendarDays(int64_t user_id, int32_t year, int32_t month, std::vector<int32_t>& days)
{
    {
        auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "GET calendar:%lld:%d:%d", user_id, year, month);
        if (rpy && rpy->str && strlen(rpy->str) > 0) {
            Json::Value cached;
            if (chen::JsonUtil::FromString(cached, rpy->str) && cached.isArray() && cached.size() > 0) {
                for (auto& v : cached) {
                    days.push_back(v.asInt());
                }
                return;
            }
        }
    }

    char start_str[32], end_str[32];
    snprintf(start_str, sizeof(start_str), "%04d-%02d-01 00:00:00", year, month);
    int next_month = month + 1;
    int next_year = year;
    if (next_month > 12) {
        next_month = 1;
        next_year++;
    }
    snprintf(end_str, sizeof(end_str), "%04d-%02d-01 00:00:00", next_year, next_month);

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "GetCalendarDays: get db failed";
        return;
    }
    auto qb = DB::ArticleInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->select("DISTINCT DAY(publish_time) as d");
    qb->where("user_id", "=", user_id);
    qb->where("state", "=", (int64_t)ArticleManager::Status::PUBLISHED);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("publish_time", ">=", std::string(start_str));
    qb->where("publish_time", "<", std::string(end_str));
    qb->orderBy("d", "ASC");
    qb->queryColumn<int32_t>(days, db, "d");

    if (!days.empty()) {
        Json::Value days_json(Json::arrayValue);
        for (auto d : days) {
            days_json.append(d);
        }
        std::string json_str = chen::JsonUtil::ToString(days_json);
        chen::RedisUtil::Cmd(
            GetRedisPoolName(), "SETEX calendar:%lld:%d:%d 300 %s", user_id, year, month, json_str.c_str());
    }
}

void ArticleScheduleService::ClearCalendarCache(int64_t user_id, int64_t publish_time)
{
    time_t pt = static_cast<time_t>(publish_time);
    struct tm tm_pt = {};
    localtime_r(&pt, &tm_pt);
    int32_t year = tm_pt.tm_year + 1900;
    int32_t month = tm_pt.tm_mon + 1;
    chen::RedisUtil::Cmd(GetRedisPoolName(), "DEL calendar:%lld:%d:%d", user_id, year, month);
}
