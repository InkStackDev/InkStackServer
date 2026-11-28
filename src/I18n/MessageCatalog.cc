#include "MessageCatalog.hpp"

#include <utility>
#include <vector>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/FSUtil.hpp>
#include <Chen/Util/JsonUtil.hpp>
#include <Chen/Util/StringUtil.hpp>

#include "I18n.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

MessageCatalog::MessageCatalog()
    : data_()
{
}

bool MessageCatalog::load(const std::string& dir)
{
    std::vector<std::string> files;
    chen::FSUtil::ListAllFile(files, dir, ".json");
    if (files.empty()) {
        ERROR(logger) << "MessageCatalog: no language config found in " << dir;
        return false;
    }

    auto data = std::make_shared<CatalogData>();
    for (const auto& file : files) {
        const std::string content = chen::FSUtil::ReadFileToString(file);
        if (content.empty()) {
            ERROR(logger) << "MessageCatalog: failed to read " << file;
            continue;
        }

        Json::Value root;
        if (!chen::JsonUtil::FromString(root, content) || !root.isObject()) {
            ERROR(logger) << "MessageCatalog: invalid JSON " << file;
            continue;
        }

        const std::string declared_lang = root.get("lang", "").asString();
        const std::string normalized = I18n::NormalizeLanguage(declared_lang);
        if (normalized.empty()) {
            ERROR(logger) << "MessageCatalog: invalid lang in " << file << " lang=" << declared_lang;
            continue;
        }

        if (!root["errors"].isObject()) {
            root["errors"] = Json::Value(Json::objectValue);
        }
        if (!root["messages"].isObject()) {
            root["messages"] = Json::Value(Json::objectValue);
        }

        auto config = std::make_shared<Json::Value>(std::move(root));
        data->configs[normalized] = config;
        INFO(logger) << "MessageCatalog: loaded " << file << " lang=" << normalized
                     << " errors=" << (*config)["errors"].size() << " messages=" << (*config)["messages"].size();
    }

    if (data->configs.find(DefaultLanguage()) == data->configs.end()) {
        ERROR(logger) << "MessageCatalog: default language " << DefaultLanguage() << " not found in " << dir;
        return false;
    }

    const size_t count = data->configs.size();
    data_.store(std::shared_ptr<const CatalogData>(std::move(data)));
    INFO(logger) << "MessageCatalog: loaded " << count << " languages from " << dir;
    return true;
}

std::string MessageCatalog::DefaultLanguage() const
{
    return I18n::DefaultLanguage();
}

MessageCatalog::ConfigPtr MessageCatalog::findConfig(const std::string& lang) const
{
    auto data = data_.load();
    if (!data) {
        return nullptr;
    }

    std::string normalized = I18n::NormalizeLanguage(lang);
    if (normalized.empty()) {
        normalized = DefaultLanguage();
    }

    auto it = data->configs.find(normalized);
    if (it != data->configs.end()) {
        return it->second;
    }

    auto def = data->configs.find(DefaultLanguage());
    if (def != data->configs.end()) {
        return def->second;
    }
    return nullptr;
}

std::string MessageCatalog::getError(const std::string& lang, int32_t err_code) const
{
    const std::string key = std::to_string(err_code);
    auto config = findConfig(lang);

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

Json::Value MessageCatalog::getErrors(const std::string& lang) const
{
    auto config = findConfig(lang);
    if (config && (*config)["errors"].isObject()) {
        return (*config)["errors"];
    }
    return Json::Value(Json::objectValue);
}

Json::Value MessageCatalog::getMessages(const std::string& lang) const
{
    auto config = findConfig(lang);
    if (config && (*config)["messages"].isObject()) {
        return (*config)["messages"];
    }
    return Json::Value(Json::objectValue);
}

Json::Value MessageCatalog::getConfig(const std::string& lang) const
{
    auto config = findConfig(lang);
    if (config) {
        return *config;
    }
    return Json::Value(Json::objectValue);
}

std::string MessageCatalog::render(const std::string& lang,
    const std::string& key,
    const std::map<std::string, std::string>& params) const
{
    auto config = findConfig(lang);
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
        WARN(logger) << "MessageCatalog: missing message key=" << key << " lang=" << lang;
        return key;
    }

    for (const auto& [name, value] : params) {
        text = chen::StringUtil::Replace(text, "{{" + name + "}}", value);
    }
    return text;
}

std::string MessageCatalog::getVersion(const std::string& lang) const
{
    auto config = findConfig(lang);
    if (config) {
        return config->get("version", "unknown").asString();
    }
    return "unknown";
}

size_t MessageCatalog::getLanguageCount() const
{
    auto data = data_.load();
    return data ? data->configs.size() : 0;
}
