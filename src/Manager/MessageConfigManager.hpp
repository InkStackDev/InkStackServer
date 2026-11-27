/**
 * @file MessageConfigManager.hpp
 * @brief 语言配置管理：加载 config/<lang>.json，提供错误码与消息模板
 * @author Christins
 * @date 2026-11-26
 * @copyright Apache 2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include <json/json.h>

#include <Chen/Util/Singleton.hpp>

class MessageConfigManager {
public:
    typedef std::shared_ptr<MessageConfigManager> ptr;

    MessageConfigManager();

    /**
     * @brief 加载 config 目录下的所有语言 JSON
     * @param dir config 目录绝对路径
     * @return 是否加载成功
     */
    bool load(const std::string& dir);

    /**
     * @brief 重新加载（用于热更新）
     */
    bool reload();

    /**
     * @brief 获取错误码文案
     */
    std::string getError(const std::string& lang, int32_t err_code) const;

    /**
     * @brief 获取错误码映射（errno 字符串 -> 文案）
     */
    Json::Value getErrors(const std::string& lang) const;

    /**
     * @brief 获取消息模板表
     */
    Json::Value getMessages(const std::string& lang) const;

    /**
     * @brief 获取完整语言配置
     */
    Json::Value getConfig(const std::string& lang) const;

    /**
     * @brief 使用 {{param}} 模板渲染消息
     */
    std::string render(const std::string& lang,
        const std::string& key,
        const std::map<std::string, std::string>& params = {}) const;

    /**
     * @brief 获取配置版本号
     */
    std::string getVersion(const std::string& lang) const;

    /**
     * @brief 已加载语言数量
     */
    size_t getLanguageCount() const { return configs_.size(); }

    /**
     * @brief 默认语言
     */
    std::string DefaultLanguage() const;

private:
    const Json::Value* findConfig(const std::string& lang) const;

    std::string dir_;
    std::map<std::string, Json::Value> configs_;
};

typedef chen::Singleton<MessageConfigManager> MessageConfigMgr;
