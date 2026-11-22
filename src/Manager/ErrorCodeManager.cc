#include "ErrorCodeManager.hpp"

#include <fstream>

#include <Chen/Config/Config.hpp>
#include <Chen/Log/Log.hpp>
#include <json/json.h>

#include "../I18n/I18n.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

ErrorCodeManager::ErrorCodeManager()
{
}

bool ErrorCodeManager::load(const std::string& config_path)
{
    config_path_ = config_path;

    std::ifstream ifs(config_path);
    if (!ifs.is_open()) {
        ERROR(logger) << "Failed to open error codes config: " << config_path;
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, ifs, &root, &errs)) {
        ERROR(logger) << "Failed to parse error codes JSON: " << errs;
        return false;
    }
    ifs.close();

    version_ = root.get("version", "unknown").asString();

    keys_.clear();
    zh_messages_.clear();
    en_messages_.clear();

    const Json::Value& errors = root["errors"];
    if (!errors.isArray()) {
        ERROR(logger) << "errors.json: 'errors' field is not an array";
        return false;
    }

    for (const auto& item : errors) {
        int32_t code = item["errno"].asInt();
        std::string key = item.get("key", "").asString();
        std::string zh = item.get("zh", "").asString();
        std::string en = item.get("en", "").asString();

        if (!key.empty()) {
            keys_[code] = key;
        }
        zh_messages_[code] = zh;
        en_messages_[code] = en;
    }

    INFO(logger) << "ErrorCodeManager loaded " << zh_messages_.size() << " error codes from " << config_path
                 << " (version: " << version_ << ")";
    return true;
}

bool ErrorCodeManager::reload()
{
    if (config_path_.empty()) {
        ERROR(logger) << "ErrorCodeManager: no config path, cannot reload";
        return false;
    }
    return load(config_path_);
}

std::string ErrorCodeManager::getMessage(int32_t err_code) const
{
    // 默认使用中文
    return getMessage(err_code, "zh");
}

std::string ErrorCodeManager::getMessage(int32_t err_code, const std::string& lang) const
{
    const std::string normalized = I18n::NormalizeLanguage(lang);
    const bool english = normalized == "en";

    auto keyIt = keys_.find(err_code);
    if (keyIt != keys_.end() && !keyIt->second.empty()) {
        const std::string translated = I18n::Translate(keyIt->second, normalized.empty() ? I18n::DefaultLanguage() : normalized);
        if (!translated.empty() && translated != keyIt->second) {
            return translated;
        }
    }

    const auto& messages = english ? en_messages_ : zh_messages_;
    auto it = messages.find(err_code);
    if (it != messages.end()) {
        return it->second;
    }
    return english ? "Unknown error" : "未知错误";
}

std::map<int32_t, std::string> ErrorCodeManager::getAllMessages(const std::string& lang) const
{
    std::map<int32_t, std::string> result;
    for (const auto& entry : zh_messages_) {
        result[entry.first] = getMessage(entry.first, lang);
    }
    return result;
}

std::string ErrorCodeManager::toJson(const std::string& lang) const
{
    Json::Value result;
    const auto& messages = getAllMessages(lang);
    for (const auto& pair : messages) {
        result[std::to_string(pair.first)] = pair.second;
    }
    return chen::JsonUtil::ToString(result);
}
