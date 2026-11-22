/**
 * @file LlmClient.hpp
 * @brief LLM 客户端（请求构建、流式解析、重试）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "ClientError.hpp"

#include <cstdint>
#include <functional>
#include <string>

struct LlmConfig {
    std::string type;
    std::string base_url;
    std::string api_key;
    std::string model;
    int32_t max_tokens = 4096;
    int64_t timeout_ms = 60000;
    int32_t max_attempts = 3;
    int64_t backoff_ms = 300;
};

class LlmClient {
public:
    typedef std::function<void(const std::string&)> ChunkCallback;
    typedef std::function<void(const std::string&)> ErrorCallback;

    /**
     * @brief 流式对话
     * @param on_chunk 文本块回调
     * @param on_error 厂商错误回调
     * @param[out] full_text 完整文本
     * @note 只在收到第一个文本块之前重试
     */
    static ClientError StreamChat(const LlmConfig& config,
        const std::string& system_prompt,
        const std::string& prompt,
        const ChunkCallback& on_chunk,
        const ErrorCallback& on_error,
        std::string& full_text);
};
