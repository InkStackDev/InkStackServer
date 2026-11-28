#include "DailyRecommendService.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <set>

#include <Chen/DB/Redis.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/JsonUtil.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Config/AppConfig.hpp"
#include "../Config/RecommendConfig.hpp"
#include "../Repository/ArticleRelRepository.hpp"
#include "../Repository/ArticleRepository.hpp"
#include "../Repository/CommentQueryRepository.hpp"
#include "../Repository/DailyRecommendRepository.hpp"
#include "../Repository/UserFollowRelRepository.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Util/Util.hpp"
#include "ArticleCounterService.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

/// 已发布状态（ArticleStatus::PUBLISHED）
constexpr int32_t kStatePublished = 2;
/// 拉取关注列表的上限
constexpr int32_t kFollowLimit = 500;
/// 推导兴趣标签时参考的点赞文章上限
constexpr size_t kInterestArticleLimit = 50;
/// 兴趣标签保留个数
constexpr int32_t kInterestLabelLimit = 5;
/// 新鲜度阈值（秒），命中时附加 fresh 理由
constexpr int64_t kFreshWindowSec = 7 * 86400;
/// 摘要截断长度
constexpr size_t kSummaryLimit = 100;
/// article 表的全列：ArticleInfoDao::ParseRow 按固定列序解析，少选列会错位
static const char* kArticleColumns =
    "a.id, a.user_id, a.title, a.content, a.type, a.state, a.channel, a.is_deleted, a.publish_time, "
    "a.weight, a.views, a.praise, a.favorites, a.create_time, a.update_time";

struct DailyRecommendServiceScored {
    DB::ArticleInfo::ptr article;
    double score = 0.0;
    int64_t comments = 0;
};

/// FNV-1a：把日期与文章 ID 映射为 [0,1) 的稳定扰动系数
static double HashRatio(const std::string& key)
{
    uint64_t hash = 1469598103934665603ULL;
    for (char c : key) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= 1099511628211ULL;
    }
    return static_cast<double>(hash % 1000000ULL) / 1000000.0;
}

/// 不含评论数的热度分
static double HeatScore(const DB::ArticleInfo::ptr& article, const RecommendConf& conf)
{
    return conf.weight_views * std::log1p(static_cast<double>(article->getViews()))
        + conf.weight_praise * std::log1p(static_cast<double>(article->getPraise()))
        + conf.weight_favorites * std::log1p(static_cast<double>(article->getFavorites()))
        + conf.weight_editor * static_cast<double>(article->getWeight());
}

/// 时效衰减：按半衰期指数衰减
static double DecayFactor(int64_t publish_time, int64_t now, const RecommendConf& conf)
{
    const int32_t half_life = conf.half_life_days > 0 ? conf.half_life_days : 7;
    const double age_days = static_cast<double>(now > publish_time ? now - publish_time : 0) / 86400.0;
    // 兜底场景下池子里可能全是老文章（甚至 publish_time 为 0），衰减会趋近 0 让热度
    // 完全失效，因此保留 5% 的下限，保证「越热越靠前」仍然成立
    const double decay = std::exp(-0.6931471805599453 * age_days / half_life);
    return decay < 0.05 ? 0.05 : decay;
}

/// 浮点得分转成放大 1000 倍的整数
static int64_t ToMilli(double score)
{
    const double scaled = score * 1000.0;
    if (scaled <= 0.0) {
        return 0;
    }
    if (scaled >= 9.0e15) {
        return 9000000000000000LL;
    }
    return static_cast<int64_t>(scaled + 0.5);
}

/// 取贡献最大的热度来源作为推荐理由
static std::string BuildReason(const DB::ArticleInfo::ptr& article,
    int64_t comments,
    const RecommendConf& conf,
    int64_t now)
{
    struct Term {
        const char* name;
        double value;
    };
    const Term terms[] = {
        {"views", conf.weight_views * std::log1p(static_cast<double>(article->getViews()))},
        {"praise", conf.weight_praise * std::log1p(static_cast<double>(article->getPraise()))},
        {"favorites", conf.weight_favorites * std::log1p(static_cast<double>(article->getFavorites()))},
        {"comments", conf.weight_comments * std::log1p(static_cast<double>(comments))},
        {"editor", conf.weight_editor * static_cast<double>(article->getWeight())},
    };
    const Term* best = &terms[0];
    for (const auto& term : terms) {
        if (term.value > best->value) {
            best = &term;
        }
    }
    std::string reason = best->name;
    if (now - article->getPublishTime() <= kFreshWindowSec) {
        reason += ",fresh";
    }
    return reason;
}

/// 批量读取文章标签，返回 article_id -> label_ids
static void LoadArticleLabels(const std::vector<int64_t>& article_ids,
    std::map<int64_t, std::vector<int64_t>>& labels,
    chen::IDB::ptr conn)
{
    labels.clear();
    if (article_ids.empty() || !conn) {
        return;
    }
    std::vector<DB::ArticleLabelRelInfo::ptr> rels;
    if (!ArticleLabelRelRepository::ListByArticles(rels, article_ids, conn)) {
        return;
    }
    for (const auto& rel : rels) {
        if (!rel) {
            continue;
        }
        labels[rel->getArticleId()].push_back(rel->getLabelId());
    }
}

std::string DailyRecommendService::Today()
{
    return chen::TimeUtil::Time2StrSec(chen::TimeUtil::GetCurrentSec(), "%Y-%m-%d");
}

bool DailyRecommendService::EnsureSnapshot(const std::string& recommend_date)
{
    const RecommendConf conf = GetRecommendConf();
    if (!conf.enabled || recommend_date.empty()) {
        return false;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "daily recommend: db unavailable";
        return false;
    }
    if (DailyRecommendRepository::HasDate(recommend_date, db)) {
        return true;
    }

    // 每次请求都可能触发兜底生成，用 Redis 锁保证跨实例只有一个写入者
    const std::string lock_key = conf.cache_key + ":lock:" + recommend_date;
    auto lock = chen::RedisUtil::Cmd(GetRedisPoolName(), {"set", lock_key, "1", "nx", "ex", "600"});
    if (!lock || !lock->str) {
        // 没抢到锁：可能是别的实例正在生成，也可能是锁残留，直接看快照是否已就绪
        return DailyRecommendRepository::HasDate(recommend_date, db);
    }
    const bool ok = BuildSnapshot(recommend_date);
    chen::RedisUtil::Cmd(GetRedisPoolName(), {"del", lock_key});
    return ok;
}

bool DailyRecommendService::BuildSnapshot(const std::string& recommend_date)
{
    const RecommendConf conf = GetRecommendConf();
    auto db = GetDB();
    if (!db) {
        return false;
    }
    const int64_t now = static_cast<int64_t>(chen::TimeUtil::GetCurrentSec());
    const int32_t candidate_days = conf.candidate_days > 0 ? conf.candidate_days : 30;
    const int32_t candidate_pool = conf.candidate_pool > 0 ? conf.candidate_pool : 300;
    const int32_t buffer_size = conf.buffer_size > 0 ? conf.buffer_size : 40;
    // publish_time 是 DATETIME 列，必须用同格式的时间字符串比较：直接传 unix 秒会被
    // MySQL 当成数字与日期比较，条件永远不成立，候选池会一直为空
    const int64_t cutoff = now - static_cast<int64_t>(candidate_days) * 86400;

    auto qb = DB::ArticleInfoDao::newQuery("a");
    if (!qb) {
        return false;
    }
    // 全列 select，列顺序必须和 ArticleInfoDao::ParseRow 一致
    qb->select(kArticleColumns);
    qb->where("a.state", "=", static_cast<int64_t>(kStatePublished));
    qb->where("a.is_deleted", "=", static_cast<int64_t>(0));
    qb->where("a.publish_time", ">=", chen::TimeUtil::Time2StrSec(static_cast<time_t>(cutoff)));
    qb->orderBy("a.views", "DESC");
    qb->limit(candidate_pool);

    std::vector<DB::ArticleInfo::ptr> candidates;
    if (DB::ArticleInfoDao::QueryByBuilder(candidates, qb, db) != 0) {
        ERROR(logger) << "daily recommend: query candidates failed, date=" << recommend_date;
        return false;
    }
    if (candidates.empty()) {
        // 兜底：窗口内没有内容（冷启动、长期停更）时放开时间限制，
        // 从全量已发布文章里按热度挑，避免面板永远空白
        auto fallback_qb = DB::ArticleInfoDao::newQuery("a");
        if (!fallback_qb) {
            return false;
        }
        fallback_qb->select(kArticleColumns);
        fallback_qb->where("a.state", "=", static_cast<int64_t>(kStatePublished));
        fallback_qb->where("a.is_deleted", "=", static_cast<int64_t>(0));
        fallback_qb->orderBy("a.views", "DESC");
        fallback_qb->limit(candidate_pool);
        if (DB::ArticleInfoDao::QueryByBuilder(candidates, fallback_qb, db) != 0) {
            ERROR(logger) << "daily recommend: fallback query failed, date=" << recommend_date;
            return false;
        }
        if (candidates.empty()) {
            // 一篇已发布文章都没有：不是故障，接口返回空列表
            WARN(logger) << "daily recommend: no published article at all, date=" << recommend_date
                         << " cutoff=" << chen::TimeUtil::Time2StrSec(static_cast<time_t>(cutoff))
                         << " candidate_days=" << candidate_days;
            return false;
        }
        WARN(logger) << "daily recommend: no article in last " << candidate_days
                     << " days, fallback to all published, date=" << recommend_date << " count=" << candidates.size();
    }

    // 先按不含评论的热度排序，只给头部候选补评论数，避免全量 N+1
    std::sort(candidates.begin(), candidates.end(),
        [&conf, now](const DB::ArticleInfo::ptr& lhs, const DB::ArticleInfo::ptr& rhs) {
            const double l = HeatScore(lhs, conf) * DecayFactor(lhs->getPublishTime(), now, conf);
            const double r = HeatScore(rhs, conf) * DecayFactor(rhs->getPublishTime(), now, conf);
            if (l != r) {
                return l > r;
            }
            return lhs->getId() < rhs->getId();
        });

    const size_t enrich = std::min(candidates.size(), static_cast<size_t>(buffer_size) * 3);
    std::vector<DailyRecommendServiceScored> scored;
    scored.reserve(enrich);
    for (size_t i = 0; i < enrich; ++i) {
        auto article = candidates[i];
        if (!article) {
            continue;
        }
        const int64_t comments = CommentQueryRepository::CountByArticle(article->getId(), db);
        double score = HeatScore(article, conf) + conf.weight_comments * std::log1p(static_cast<double>(comments));
        score *= DecayFactor(article->getPublishTime(), now, conf);
        // 同一天内稳定、次日整体轮换
        score *= 0.925 + 0.15 * HashRatio(recommend_date + "#" + std::to_string(article->getId()));

        DailyRecommendServiceScored row;
        row.article = article;
        row.score = score;
        row.comments = comments;
        scored.push_back(row);
    }
    std::sort(scored.begin(), scored.end(),
        [](const DailyRecommendServiceScored& lhs, const DailyRecommendServiceScored& rhs) {
            if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }
        return lhs.article->getId() < rhs.article->getId();
    });

    std::vector<int64_t> article_ids;
    for (const auto& row : scored) {
        article_ids.push_back(row.article->getId());
    }
    std::map<int64_t, std::vector<int64_t>> labels;
    LoadArticleLabels(article_ids, labels, db);

    const int32_t max_per_author = conf.max_per_author > 0 ? conf.max_per_author : 2;
    const int32_t max_per_label = conf.max_per_label > 0 ? conf.max_per_label : 3;
    std::map<int64_t, int32_t> author_count;
    std::map<int64_t, int32_t> label_count;

    std::vector<DB::ArticleDailyRecommendInfo::ptr> rows;
    for (const auto& row : scored) {
        if (rows.size() >= static_cast<size_t>(buffer_size)) {
            break;
        }
        const int64_t author_id = row.article->getUserId();
        if (author_count[author_id] >= max_per_author) {
            continue;
        }
        bool label_blocked = false;
        auto label_it = labels.find(row.article->getId());
        if (label_it != labels.end()) {
            for (int64_t label_id : label_it->second) {
                if (label_count[label_id] >= max_per_label) {
                    label_blocked = true;
                    break;
                }
            }
        }
        if (label_blocked) {
            continue;
        }

        ++author_count[author_id];
        if (label_it != labels.end()) {
            for (int64_t label_id : label_it->second) {
                ++label_count[label_id];
            }
        }

        auto info = std::make_shared<DB::ArticleDailyRecommendInfo>();
        info->setRecommendDate(recommend_date);
        info->setArticleId(row.article->getId());
        info->setRankNo(static_cast<int32_t>(rows.size()) + 1);
        info->setScore(ToMilli(row.score));
        info->setReason(BuildReason(row.article, row.comments, conf, now));
        info->setCreateTime(now);
        info->setUpdateTime(now);
        rows.push_back(info);
    }

    if (rows.empty()) {
        WARN(logger) << "daily recommend: empty snapshot, date=" << recommend_date;
        return false;
    }
    if (!DailyRecommendRepository::ReplaceDate(recommend_date, rows, db)) {
        ERROR(logger) << "daily recommend: write snapshot failed, date=" << recommend_date;
        return false;
    }
    INFO(logger) << "daily recommend: snapshot built, date=" << recommend_date << " size=" << rows.size();
    return true;
}

bool DailyRecommendService::LoadSnapshot(const std::string& recommend_date, std::vector<Item>& rows)
{
    rows.clear();
    auto db = GetDB();
    if (!db) {
        return false;
    }
    std::vector<DB::ArticleDailyRecommendInfo::ptr> infos;
    if (!DailyRecommendRepository::ListByDate(recommend_date, infos, db) || infos.empty()) {
        return false;
    }
    rows.reserve(infos.size());
    for (const auto& info : infos) {
        if (!info) {
            continue;
        }
        Item item;
        item.article_id = info->getArticleId();
        item.score = info->getScore();
        item.rank_no = info->getRankNo();
        item.reason = info->getReason();
        rows.push_back(item);
    }
    return !rows.empty();
}

void DailyRecommendService::Personalize(int64_t user_id, std::vector<Item>& rows)
{
    if (user_id <= 0 || rows.empty()) {
        return;
    }
    const RecommendConf conf = GetRecommendConf();
    if (!conf.personalize) {
        return;
    }
    auto db = GetDB();
    if (!db) {
        return;
    }

    // 关注作者
    std::set<int64_t> followed;
    std::vector<DB::UserFollowRelInfo::ptr> follows;
    UserFollowRelRepository::ListFollowing(follows, user_id, 0, kFollowLimit, db);
    for (const auto& rel : follows) {
        if (rel) {
            followed.insert(rel->getFollowingId());
        }
    }

    // 已点赞/已收藏（Redis 计数服务维护的 hash）
    std::map<int64_t, int64_t> favorites;
    std::map<int64_t, int64_t> praises;
    ArticleCounterService::ListUserFav(user_id, favorites);
    ArticleCounterService::ListUserPra(user_id, praises);

    // 兴趣标签：用户点赞过的文章标签频次前 N
    std::set<int64_t> interests;
    if (!praises.empty()) {
        std::vector<int64_t> praised_ids;
        praised_ids.reserve(praises.size());
        for (const auto& kv : praises) {
            praised_ids.push_back(kv.first);
            if (praised_ids.size() >= kInterestArticleLimit) {
                break;
            }
        }
        std::vector<DB::ArticleLabelRelInfo::ptr> rels;
        if (ArticleLabelRelRepository::ListByArticles(rels, praised_ids, db)) {
            std::map<int64_t, int32_t> freq;
            for (const auto& rel : rels) {
                if (rel) {
                    ++freq[rel->getLabelId()];
                }
            }
            std::vector<std::pair<int32_t, int64_t>> ranked;
            ranked.reserve(freq.size());
            for (const auto& kv : freq) {
                ranked.emplace_back(kv.second, kv.first);
            }
            std::sort(ranked.begin(), ranked.end(), [](const auto& lhs, const auto& rhs) {
                if (lhs.first != rhs.first) {
                    return lhs.first > rhs.first;
                }
                return lhs.second < rhs.second;
            });
            for (size_t i = 0; i < ranked.size() && i < static_cast<size_t>(kInterestLabelLimit); ++i) {
                interests.insert(ranked[i].second);
            }
        }
    }

    std::vector<int64_t> row_ids;
    row_ids.reserve(rows.size());
    for (const auto& row : rows) {
        row_ids.push_back(row.article_id);
    }
    std::map<int64_t, std::vector<int64_t>> labels;
    LoadArticleLabels(row_ids, labels, db);

    std::vector<Item> adjusted;
    adjusted.reserve(rows.size());
    for (auto row : rows) {
        const bool praised = praises.find(row.article_id) != praises.end();
        const bool favorited = favorites.find(row.article_id) != favorites.end();
        if (conf.exclude_interacted && (praised || favorited)) {
            continue;
        }
        double score = static_cast<double>(row.score);
        int32_t hits = 0;
        auto label_it = labels.find(row.article_id);
        if (label_it != labels.end()) {
            for (int64_t label_id : label_it->second) {
                if (interests.count(label_id) > 0) {
                    ++hits;
                }
            }
        }
        if (followed.count(row.user_id) > 0) {
            score += static_cast<double>(conf.follow_boost);
            row.reason += ",follow";
        }
        if (hits > 0) {
            score += static_cast<double>(conf.label_boost) * hits;
            row.reason += ",interest";
        }
        row.score = static_cast<int64_t>(score);
        adjusted.push_back(row);
    }

    std::sort(adjusted.begin(), adjusted.end(), [](const Item& lhs, const Item& rhs) {
        if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }
        return lhs.article_id < rhs.article_id;
    });
    rows.swap(adjusted);
}

bool DailyRecommendService::FillDetails(std::vector<Item>& items)
{
    auto db = GetDB();
    if (!db) {
        return false;
    }
    std::vector<Item> filled;
    filled.reserve(items.size());
    for (auto item : items) {
        auto article = ArticleRepository::Get(item.article_id, db);
        if (!article) {
            continue;
        }
        if (article->getState() != kStatePublished || article->getIsDeleted() != 0) {
            continue;
        }
        item.user_id = article->getUserId();
        item.title = article->getTitle();
        const std::string& content = article->getContent();
        item.summary = content.size() > kSummaryLimit ? content.substr(0, kSummaryLimit) : content;
        item.views = article->getViews();
        item.praise = article->getPraise();
        item.favorites = article->getFavorites();
        item.publish_time = article->getPublishTime();

        auto author = UserRepository::Get(item.user_id, db);
        item.author = author ? author->getName() : "";
        item.author_avatar = author ? author->getAvatar() : "";
        filled.push_back(item);
    }
    items.swap(filled);
    return !items.empty();
}

bool DailyRecommendService::ListDaily(std::vector<Item>& items, int64_t user_id)
{
    items.clear();
    const RecommendConf conf = GetRecommendConf();
    if (!conf.enabled) {
        return false;
    }
    const std::string date = Today();
    const bool personalized = conf.personalize && user_id > 0;
    const std::string user_key = conf.cache_key + ":" + date + ":u:" + std::to_string(user_id);
    if (personalized && LoadCache(user_key, items) && !items.empty()) {
        return true;
    }

    std::vector<Item> rows;
    if (!LoadSnapshot(date, rows)) {
        if (!EnsureSnapshot(date) || !LoadSnapshot(date, rows)) {
            // 候选池为空（站点还没有落在窗口内的已发布文章）不算故障：返回空列表让前端
            // 展示“今天的推荐还在生成中”，真正的查询失败已在 BuildSnapshot 里打过 ERROR
            WARN(logger) << "daily recommend: snapshot unavailable, date=" << date;
            return true;
        }
    }
    if (rows.empty()) {
        return true;
    }

    // 个性化需要作者 ID 与标签，先补全条目再重排，避免个人化阶段重复查库
    if (personalized) {
        FillDetails(rows);
        if (rows.empty()) {
            return true;
        }
        Personalize(user_id, rows);
    }
    const size_t daily_count = conf.daily_count > 0 ? static_cast<size_t>(conf.daily_count) : 3;
    if (rows.size() > daily_count) {
        rows.resize(daily_count);
    }
    if (!personalized) {
        FillDetails(rows);
        if (rows.empty()) {
            return true;
        }
    }
    for (size_t i = 0; i < rows.size(); ++i) {
        rows[i].rank_no = static_cast<int32_t>(i) + 1;
    }
    items = rows;
    if (personalized) {
        SaveCache(user_key, items, conf.cache_ttl_sec > 0 ? conf.cache_ttl_sec : 300);
    }
    return true;
}

bool DailyRecommendService::LoadCache(const std::string& key, std::vector<Item>& items)
{
    items.clear();
    auto reply = chen::RedisUtil::Cmd(GetRedisPoolName(), {"get", key});
    if (!reply || !reply->str) {
        return false;
    }
    Json::Value root;
    if (!chen::JsonUtil::FromString(root, std::string(reply->str, reply->len)) || !root.isArray()) {
        return false;
    }
    items.reserve(root.size());
    for (const auto& node : root) {
        Item item;
        item.article_id = node["article_id"].asInt64();
        item.user_id = node["user_id"].asInt64();
        item.author = node["author"].asString();
        item.author_avatar = node["author_avatar"].asString();
        item.title = node["title"].asString();
        item.summary = node["summary"].asString();
        item.publish_time = node["publish_time"].asInt64();
        item.views = node["views"].asInt64();
        item.praise = node["praise"].asInt64();
        item.favorites = node["favorites"].asInt64();
        item.score = node["score"].asInt64();
        item.rank_no = node["rank_no"].asInt();
        item.reason = node["reason"].asString();
        items.push_back(item);
    }
    return true;
}

void DailyRecommendService::SaveCache(const std::string& key, const std::vector<Item>& items, int32_t ttl_sec)
{
    Json::Value root(Json::arrayValue);
    for (const auto& item : items) {
        Json::Value node;
        node["article_id"] = item.article_id;
        node["user_id"] = item.user_id;
        node["author"] = item.author;
        node["author_avatar"] = item.author_avatar;
        node["title"] = item.title;
        node["summary"] = item.summary;
        node["publish_time"] = item.publish_time;
        node["views"] = item.views;
        node["praise"] = item.praise;
        node["favorites"] = item.favorites;
        node["score"] = item.score;
        node["rank_no"] = item.rank_no;
        node["reason"] = item.reason;
        root.append(node);
    }
    const std::string payload = chen::JsonUtil::ToString(root);
    chen::RedisUtil::Cmd(GetRedisPoolName(), {"setex", key, std::to_string(ttl_sec > 0 ? ttl_sec : 300), payload});
}
