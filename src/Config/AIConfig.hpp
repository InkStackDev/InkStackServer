/**
 * @file AIConfig.hpp
 * @brief 系统默认 AI 配置（用户未配置个人 AI 设置时的兜底）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <Chen/Config/Config.hpp>

#include <sstream>
#include <string>

/**
 * @brief 系统默认 AI 配置（ai 节点）
 */
struct AIConf {
    /// 厂商类型，如 glm / openai / claude
    std::string type;
    /// 默认模型
    std::string model;
    /// 默认 API key（明文）
    std::string api_key;
    /// 流式请求超时（毫秒）
    int64_t timeout_ms = 60000;
    /// 首个文本块前最大尝试次数
    int32_t max_attempts = 3;

    bool operator==(const AIConf& other) const
    {
        return type == other.type && model == other.model && api_key == other.api_key && timeout_ms == other.timeout_ms
            && max_attempts == other.max_attempts;
    }
};

namespace chen {

template <>
class LexicalCast<std::string, AIConf> {
public:
    AIConf operator()(const std::string& value)
    {
        AIConf conf;
        const YAML::Node node = YAML::Load(value);
        if (!node.IsMap()) {
            return conf;
        }
        if (node["type"].IsScalar()) {
            conf.type = node["type"].Scalar();
        }
        if (node["model"].IsScalar()) {
            conf.model = node["model"].Scalar();
        }
        if (node["api_key"].IsScalar()) {
            conf.api_key = node["api_key"].Scalar();
        }
        if (node["timeout_ms"].IsScalar()) {
            conf.timeout_ms = LexicalCast<std::string, int64_t>()(node["timeout_ms"].Scalar());
        }
        if (node["max_attempts"].IsScalar()) {
            conf.max_attempts = LexicalCast<std::string, int32_t>()(node["max_attempts"].Scalar());
        }
        return conf;
    }
};

template <>
class LexicalCast<AIConf, std::string> {
public:
    std::string operator()(const AIConf& conf)
    {
        YAML::Node node;
        node["type"] = conf.type;
        node["model"] = conf.model;
        node["api_key"] = conf.api_key;
        node["timeout_ms"] = conf.timeout_ms;
        node["max_attempts"] = conf.max_attempts;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

} // namespace chen

/**
 * @brief 系统默认 AI 配置快照（ai）
 * @note 每次调用都会读取当前值；热加载后下一次读取即生效
 */
AIConf GetAIConf();
