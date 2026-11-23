/**
 * @file ArticleAIService.hpp
 * @brief 文章 AI 总结用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "../Client/LlmClient.hpp"
#include "../Domain/AccessTypes.hpp"

#include <cstdint>
#include <string>

class ArticleAIService {
public:
    enum class Result {
        Ok,
        NotLogin,
        ArticleNotFound,
        ConfigNotFound,
        UnsupportedProvider,
        ClientError,
        DbError
    };

    static Result Summarize(const Viewer& viewer,
        int64_t article_id,
        const LlmClient::ChunkCallback& on_chunk,
        const LlmClient::ErrorCallback& on_error,
        std::string& full_text);
};
