#include "ArticleAIService.hpp"

#include "../Config/AIConfig.hpp"
#include "../Repository/ArticleRepository.hpp"
#include "../Repository/UserAIConfigRepository.hpp"
#include "../Util/Util.hpp"

#include <Chen/Log/Log.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

static const char* kSystemPrompt = "你是一个专业的文章总结助手。";

ArticleAIService::Result ArticleAIService::Summarize(const Viewer& viewer,
    int64_t article_id,
    const LlmClient::ChunkCallback& on_chunk,
    const LlmClient::ErrorCallback& on_error,
    std::string& full_text)
{
    full_text.clear();
    if (!viewer.valid()) {
        return Result::NotLogin;
    }

    auto db = GetDB();
    if (!db) {
        return Result::DbError;
    }

    auto article = ArticleRepository::Get(article_id, db);
    if (!article || article->getIsDeleted()) {
        return Result::ArticleNotFound;
    }

    std::string title = article->getTitle();
    std::string content = article->getContent();
    constexpr size_t kMaxContentLen = 12000;
    if (content.size() > kMaxContentLen) {
        content = content.substr(0, kMaxContentLen) + "...";
    }

    AIConf system_conf = GetAIConf();
    LlmConfig config;
    config.timeout_ms = system_conf.timeout_ms;
    config.max_attempts = system_conf.max_attempts;

    auto user_conf = UserAIConfigRepository::GetByUserId(viewer.user_id, db);
    if (user_conf) {
        config.type = user_conf->getType();
        config.base_url = user_conf->getUrl();
        config.model = user_conf->getModel();
        config.max_tokens = user_conf->getMaxTokens();
        config.api_key = DecryptApiKey(user_conf->getApiKey());
    } else {
        config.type = system_conf.type;
        config.model = system_conf.model;
        config.api_key = system_conf.api_key;
    }

    if (config.type.empty() || config.model.empty() || config.api_key.empty()) {
        return Result::ConfigNotFound;
    }

    std::string prompt = "请用简洁的中文总结以下文章的核心内容，控制在200字以内：\n\n"
                         "标题："
        + title + "\n\n" + content;

    auto error = LlmClient::StreamChat(config, kSystemPrompt, prompt, on_chunk, on_error, full_text);
    if (error == ClientError::Ok) {
        return Result::Ok;
    }
    if (error == ClientError::UnsupportedProvider) {
        return Result::UnsupportedProvider;
    }
    ERROR(logger) << "article ai summary failed, article_id=" << article_id << " error=" << (int)error;
    return Result::ClientError;
}
