/**
 * @file OpenAIProvider.hpp
 * @brief OpenAI / OpenAI 兼容 协议适配器
 * @author Christins
 * @date 2026-06-10
 * @copyright Apache 2.0
 */
#pragma once

#include "AIProvider.hpp"

class OpenAIProvider : public AIProvider {
public:
    typedef std::shared_ptr<OpenAIProvider> ptr;

    std::string buildRequest(
        const std::string& model,
        int32_t max_tokens,
        const std::string& system_prompt,
        const std::string& prompt,
        bool stream = true) override;

    std::string endpointSuffix() const override;

    std::string extractContent(const Json::Value& parsed) override;

    std::string extractNonStreamingContent(const Json::Value& parsed) override;

    bool isTerminal(const Json::Value& parsed) override;

    std::string getError(const Json::Value& parsed) override;

    bool isDoneMarker(const std::string& raw_data) override;

    void authHeaders(const std::string& api_key, std::map<std::string, std::string>& out_headers) override;
};
