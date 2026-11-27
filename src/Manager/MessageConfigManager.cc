#include "MessageConfigManager.hpp"

#include <vector>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/FSUtil.hpp>
#include <Chen/Util/JsonUtil.hpp>
#include <Chen/Util/StringUtil.hpp>

#include "../I18n/I18n.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

MessageConfigManager::MessageConfigManager()
{
}

bool MessageConfigManager::load(const std::string& dir)
{
    dir_ = dir;
    configs_.clear();

    std::vector<std::string> files;
    chen::FSUtil::ListAllFile(files, dir, ".json");
    if (files.empty()) {
        ERROR(logger) << "MessageConfigManager: no language config found in " << dir;
        return false;
    }

    for (const auto& file : files) {
        const std::string content = chen::FSUtil::ReadFileToString(file);
        if (content.empty()) {
            ERROR(logger) << "MessageConfigManager: failed to read " << file;
            continue;
        }

        Json::Value root;
        if (!chen::JsonUtil::FromString(root, content) || !root.isObject()) {
            ERROR(logger) << "MessageConfigManager: invalid JSON " << file;
            continue;
        }

        const std::string declared_lang = root.get("lang", "").asString();
        const std::string normalized = I18n::NormalizeLanguage(declared_lang);
        if (normalized.empty()) {
            ERROR(logger) << "MessageConfigManager: invalid lang in " << file << " lang=" << declared_lang;
            continue;
        }

        if (!root["errors"].isObject()) {
            root["errors"] = Json::Value(Json::objectValue);
        }
        if (!root["messages"].isObject()) {
            root["messages"] = Json::Value(Json::objectValue);
        }

        configs_[normalized] = root;
        INFO(logger) << "MessageConfigManager: loaded " << file << " lang=" << normalized
                     << " errors=" << root["errors"].size() << " messages=" << root["messages"].size();
    }

    if (configs_.find(DefaultLanguage()) == configs_.end()) {
        ERROR(logger) << "MessageConfigManager: default language " << DefaultLanguage() << " not found in " << dir;
        return false;
    }

    INFO(logger) << "MessageConfigManager: loaded " << configs_.size() << " languages from " << dir;
    return true;
}

bool MessageConfigManager::reload()
{
    if (dir_.empty()) {
        ERROR(logger) << "MessageConfigManager: no config dir, cannot reload";
        return false;
    }
    return load(dir_);
}

std::string MessageConfigManager::DefaultLanguage() const
{
    return I18n::DefaultLanguage();
}

const Json::Value* MessageConfigManager::findConfig(const std::string& lang) const
{
    std::string normalized = I18n::NormalizeLanguage(lang);
    if (normalized.empty()) {
        normalized = DefaultLanguage();
    }

    auto it = configs_.find(normalized);
    if (it != configs_.end()) {
        return &it->second;
    }

    auto def = configs_.find(DefaultLanguage());
    if (def != configs_.end()) {
        return &def->second;
    }
    return nullptr;
}

std::string MessageConfigManager::getError(const std::string& lang, int32_t err_code) const
{
    const std::string key = std::to_string(err_code);
    const Json::Value* config = findConfig(lang);

    if (config) {
        const Json::Value& errors = (*config)["errors"];
        if (errors.isObject() && errors.isMember(key) && errors[key].isString()) {
            return errors[key].asString();
        }
    }

    config = findConfig(DefaultLanguage());
    if (config) {
        const Json::Value& errors = (*config)["errors"];
        if (errors.isObject() && errors.isMember(key) && errors[key].isString()) {
            return errors[key].asString();
        }
    }
    return "Unknown error";
}

Json::Value MessageConfigManager::getErrors(const std::string& lang) const
{
    const Json::Value* config = findConfig(lang);
    if (config && (*config)["errors"].isObject()) {
        return (*config)["errors"];
    }
    return Json::Value(Json::objectValue);
}

Json::Value MessageConfigManager::getMessages(const std::string& lang) const
{
    const Json::Value* config = findConfig(lang);
    if (config && (*config)["messages"].isObject()) {
        return (*config)["messages"];
    }
    return Json::Value(Json::objectValue);
}

Json::Value MessageConfigManager::getConfig(const std::string& lang) const
{
    const Json::Value* config = findConfig(lang);
    if (config) {
        return *config;
    }
    return Json::Value(Json::objectValue);
}

std::string MessageConfigManager::render(const std::string& lang,
    const std::string& key,
    const std::map<std::string, std::string>& params) const
{
    const Json::Value* config = findConfig(lang);
    std::string text;

    if (config) {
        const Json::Value& messages = (*config)["messages"];
        if (messages.isObject() && messages.isMember(key) && messages[key].isString()) {
            text = messages[key].asString();
        }
    }

    if (text.empty() && I18n::NormalizeLanguage(lang) != DefaultLanguage()) {
        config = findConfig(DefaultLanguage());
        if (config) {
            const Json::Value& messages = (*config)["messages"];
            if (messages.isObject() && messages.isMember(key) && messages[key].isString()) {
                text = messages[key].asString();
            }
        }
    }

    if (text.empty()) {
        WARN(logger) << "MessageConfigManager: missing message key=" << key << " lang=" << lang;
        return key;
    }

    for (const auto& [name, value] : params) {
        text = chen::StringUtil::Replace(text, "{{" + name + "}}", value);
    }
    return text;
}

std::string MessageConfigManager::getVersion(const std::string& lang) const
{
    const Json::Value* config = findConfig(lang);
    if (config) {
        return (*config).get("version", "unknown").asString();
    }
    return "unknown";
}
