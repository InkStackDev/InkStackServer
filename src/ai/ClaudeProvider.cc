#include "ClaudeProvider.hpp"

#include <chen/util/json_util.h>

std::string ClaudeProvider::buildRequest(
    const std::string& model,
    int32_t max_tokens,
    const std::string& system_prompt,
    const std::string& prompt,
    bool stream)
{
    Json::Value body;
    body["model"] = model;
    body["max_tokens"] = max_tokens;
    body["system"] = system_prompt;
    if (stream) {
        body["stream"] = true;
    }

    Json::Value messages(Json::arrayValue);
    Json::Value usr;
    usr["role"] = "user";
    usr["content"] = prompt;
    messages.append(usr);

    body["messages"] = messages;
    return chen::JsonUtil::ToString(body);
}

std::string ClaudeProvider::endpointSuffix() const
{
    return "/v1/messages";
}

std::string ClaudeProvider::extractContent(const Json::Value& parsed)
{
    std::string evType = parsed["type"].asString();
    if (evType == "content_block_delta") {
        auto deltaType = parsed["delta"]["type"].asString();
        if (deltaType == "text_delta") {
            return parsed["delta"]["text"].asString();
        }
    }
    return "";
}

bool ClaudeProvider::isTerminal(const Json::Value& parsed)
{
    return parsed["type"].asString() == "message_stop";
}

std::string ClaudeProvider::getError(const Json::Value& parsed)
{
    if (parsed["type"].asString() == "error") {
        return parsed["error"]["message"].asString();
    }
    return "";
}

std::string ClaudeProvider::extractNonStreamingContent(const Json::Value& parsed)
{
    auto& content = parsed["content"];
    if (content.isArray()) {
        for (const auto& block : content) {
            if (block["type"].asString() == "text") {
                return block["text"].asString();
            }
        }
    }
    return "";
}

void ClaudeProvider::authHeaders(const std::string& api_key, std::map<std::string, std::string>& out_headers)
{
    out_headers["x-api-key"] = api_key;
    out_headers["anthropic-version"] = "2023-06-01";
}
