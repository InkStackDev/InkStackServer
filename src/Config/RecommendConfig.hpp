/**
 * @file RecommendConfig.hpp
 * @brief 每日推荐规则配置（recommend 节点）
 * @author Christins
 * @date 2026-11-28
 * @copyright Apache 2.0
 */
#pragma once

#include <Chen/Config/Config.hpp>

#include <cstdint>
#include <sstream>
#include <string>

/**
 * @brief 每日推荐规则配置
 * @note 打分公式：hot = w_views*log1p(views) + w_praise*log1p(praise)
 *       + w_favorites*log1p(favorites) + w_comments*log1p(comments) + w_editor*weight，
 *       score = hot * exp(-ln2 * age_days / half_life_days) * jitter，
 *       jitter 由 recommend_date 与 article_id 的稳定哈希派生，保证同一天内稳定、次日轮换。
 */
struct RecommendConf {
    /// 是否启用每日推荐接口与快照任务
    bool enabled = true;
    /// 是否对登录用户做个性化重排
    bool personalize = true;
    /// 个性化时是否过滤用户已点赞/已收藏的文章
    bool exclude_interacted = true;
    /// 每日返回条数
    int32_t daily_count = 6;
    /// 每天落库的候选条数，要大于 daily_count，给个性化重排留出空间
    int32_t buffer_size = 40;
    /// 候选池时间窗口（天）
    int32_t candidate_days = 30;
    /// 候选池上限，按浏览量倒序取
    int32_t candidate_pool = 300;
    /// 时效衰减半衰期（天）
    int32_t half_life_days = 7;
    /// 浏览量权重
    int32_t weight_views = 10;
    /// 点赞权重
    int32_t weight_praise = 30;
    /// 收藏权重
    int32_t weight_favorites = 50;
    /// 评论权重
    int32_t weight_comments = 40;
    /// 编辑加权（article.weight）权重
    int32_t weight_editor = 20;
    /// 同一作者最多入选篇数
    int32_t max_per_author = 2;
    /// 同一标签最多入选篇数
    int32_t max_per_label = 3;
    /// 个性化重排时，关注作者带来的加分（与 score 同单位，score 为放大 1000 倍的整数）
    int32_t follow_boost = 30000;
    /// 个性化重排时，每命中一个兴趣标签带来的加分
    int32_t label_boost = 5000;
    /// 单个用户推荐结果的缓存时间（秒）
    int32_t cache_ttl_sec = 300;
    /// 推荐结果缓存 key 前缀，真实 key 为 "<prefix>:<date>" 与 "<prefix>:<date>:u:<uid>"
    std::string cache_key = "blog:daily_recommend";

    bool operator==(const RecommendConf& other) const
    {
        return enabled == other.enabled && personalize == other.personalize
            && exclude_interacted == other.exclude_interacted && daily_count == other.daily_count
            && buffer_size == other.buffer_size && candidate_days == other.candidate_days
            && candidate_pool == other.candidate_pool && half_life_days == other.half_life_days
            && weight_views == other.weight_views && weight_praise == other.weight_praise
            && weight_favorites == other.weight_favorites && weight_comments == other.weight_comments
            && weight_editor == other.weight_editor && max_per_author == other.max_per_author
            && max_per_label == other.max_per_label && follow_boost == other.follow_boost
            && label_boost == other.label_boost && cache_ttl_sec == other.cache_ttl_sec && cache_key == other.cache_key;
    }
};

/**
 * @brief 读取可选的标量配置项，缺失时保留结构体里的默认值
 * @note 不能直接写 node[key].IsScalar()：yaml-cpp 0.8 在 key 缺失时返回 invalid node，
 *       任何 Type()/IsXxx() 调用都会抛 InvalidNode（invalid node; first invalid key: "xxx"），
 *       进而让 ConfigVar::fromString 打印异常并放弃整个节点的赋值。
 */
template <typename T>
void ReadRecommendScalar(const YAML::Node& node, const char* key, T& value)
{
    const YAML::Node item = node[key];
    if (!item.IsDefined() || !item.IsScalar()) {
        return;
    }
    value = chen::LexicalCast<std::string, T>()(item.Scalar());
}

namespace chen {

template <>
class LexicalCast<std::string, RecommendConf> {
public:
    RecommendConf operator()(const std::string& value)
    {
        RecommendConf conf;
        const YAML::Node node = YAML::Load(value);
        if (!node.IsMap()) {
            return conf;
        }
        ReadRecommendScalar(node, "enabled", conf.enabled);
        ReadRecommendScalar(node, "personalize", conf.personalize);
        ReadRecommendScalar(node, "exclude_interacted", conf.exclude_interacted);
        ReadRecommendScalar(node, "daily_count", conf.daily_count);
        ReadRecommendScalar(node, "buffer_size", conf.buffer_size);
        ReadRecommendScalar(node, "candidate_days", conf.candidate_days);
        ReadRecommendScalar(node, "candidate_pool", conf.candidate_pool);
        ReadRecommendScalar(node, "half_life_days", conf.half_life_days);
        ReadRecommendScalar(node, "weight_views", conf.weight_views);
        ReadRecommendScalar(node, "weight_praise", conf.weight_praise);
        ReadRecommendScalar(node, "weight_favorites", conf.weight_favorites);
        ReadRecommendScalar(node, "weight_comments", conf.weight_comments);
        ReadRecommendScalar(node, "weight_editor", conf.weight_editor);
        ReadRecommendScalar(node, "max_per_author", conf.max_per_author);
        ReadRecommendScalar(node, "max_per_label", conf.max_per_label);
        ReadRecommendScalar(node, "follow_boost", conf.follow_boost);
        ReadRecommendScalar(node, "label_boost", conf.label_boost);
        ReadRecommendScalar(node, "cache_ttl_sec", conf.cache_ttl_sec);
        ReadRecommendScalar(node, "cache_key", conf.cache_key);
        return conf;
    }
};

template <>
class LexicalCast<RecommendConf, std::string> {
public:
    std::string operator()(const RecommendConf& conf)
    {
        YAML::Node node;
        node["enabled"] = conf.enabled;
        node["personalize"] = conf.personalize;
        node["exclude_interacted"] = conf.exclude_interacted;
        node["daily_count"] = conf.daily_count;
        node["buffer_size"] = conf.buffer_size;
        node["candidate_days"] = conf.candidate_days;
        node["candidate_pool"] = conf.candidate_pool;
        node["half_life_days"] = conf.half_life_days;
        node["weight_views"] = conf.weight_views;
        node["weight_praise"] = conf.weight_praise;
        node["weight_favorites"] = conf.weight_favorites;
        node["weight_comments"] = conf.weight_comments;
        node["weight_editor"] = conf.weight_editor;
        node["max_per_author"] = conf.max_per_author;
        node["max_per_label"] = conf.max_per_label;
        node["follow_boost"] = conf.follow_boost;
        node["label_boost"] = conf.label_boost;
        node["cache_ttl_sec"] = conf.cache_ttl_sec;
        node["cache_key"] = conf.cache_key;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

} // namespace chen

/**
 * @brief 每日推荐规则配置快照（recommend）
 * @note 每次调用都会读取当前值；热加载后下一次读取即生效
 */
RecommendConf GetRecommendConf();
