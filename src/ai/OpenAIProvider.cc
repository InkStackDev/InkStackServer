#include "OpenAIProvider.hpp"

#include <Chen/Util/JsonUtil.hpp>

std::string OpenAIProvider::buildRequest(
    const std::string& model,
    int32_t max_tokens,
    const std::string& system_prompt,
    const std::string& prompt,
    bool stream)
{
    Json::Value body;
    body["model"] = model;
    body["max_tokens"] = max_tokens;
    if (stream) {
        body["stream"] = true;
    }

    Json::Value messages(Json::arrayValue);
    Json::Value sys;
    sys["role"] = "system";
    sys["content"] = system_prompt;
    messages.append(sys);

    Json::Value usr;
    usr["role"] = "user";
    usr["content"] = prompt;
    messages.append(usr);

    body["messages"] = messages;
    return chen::JsonUtil::ToString(body);
}

std::string OpenAIProvider::endpointSuffix() const
{
    return "/v1/chat/completions";
}

std::string OpenAIProvider::extractContent(const Json::Value& parsed)
{
    auto& choices = parsed["choices"];
    if (choices.isArray() && choices.size() > 0) {
        return choices[0]["delta"]["content"].asString();
    }
    return "";
}

bool OpenAIProvider::isTerminal(const Json::Value& parsed)
{
    auto& choices = parsed["choices"];
    if (choices.isArray() && choices.size() > 0) {
        auto finish = choices[0]["finish_reason"].asString();
        return !finish.empty() && finish != "null";
    }
    return false;
}

std::string OpenAIProvider::getError(const Json::Value& /*parsed*/)
{
    return "";
}

std::string OpenAIProvider::extractNonStreamingContent(const Json::Value& parsed)
{
    auto& choices = parsed["choices"];
    if (choices.isArray() && choices.size() > 0) {
        return choices[0]["message"]["content"].asString();
    }
    auto& err = parsed["error"];
    if (!err.isNull()) {
        return "";
    }
    return "";
}

void OpenAIProvider::authHeaders(const std::string& api_key, std::map<std::string, std::string>& out_headers)
{
    out_headers["Authorization"] = "Bearer " + api_key;
}

bool OpenAIProvider::isDoneMarker(const std::string& raw_data)
{
    return raw_data == "[DONE]";
}
