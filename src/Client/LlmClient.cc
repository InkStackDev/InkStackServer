#include "LlmClient.hpp"

#include "../AI/AIProvider.hpp"
#include "../AI/SSEStreamParser.hpp"

#include <Chen/Http/HttpConnection.hpp>
#include <Chen/Http/Uri.hpp>
#include <Chen/Log/Log.hpp>

#include <unistd.h>

static chen::Logger::ptr logger = LOG_ROOT();

namespace {

ClientError MapStatus(int status)
{
    if (status == 401 || status == 403) {
        return ClientError::Unauthorized;
    }
    if (status == 404) {
        return ClientError::NotFound;
    }
    if (status == 429) {
        return ClientError::RateLimited;
    }
    if (status >= 500) {
        return ClientError::ServerError;
    }
    return ClientError::Unknown;
}

void Backoff(const LlmConfig& config, int attempt)
{
    int64_t backoff = config.backoff_ms * (attempt + 1);
    if (backoff > 5000) {
        backoff = 5000;
    }
    if (backoff > 0) {
        usleep((useconds_t)(backoff * 1000));
    }
}

} // namespace

ClientError LlmClient::StreamChat(const LlmConfig& config,
    const std::string& system_prompt,
    const std::string& prompt,
    const ChunkCallback& on_chunk,
    const ErrorCallback& on_error,
    std::string& full_text)
{
    full_text.clear();

    if (config.type.empty() || config.api_key.empty() || config.model.empty()) {
        return ClientError::MalformedResponse;
    }

    auto provider = CreateProvider(config.type);
    if (!provider) {
        return ClientError::UnsupportedProvider;
    }

    std::string base_url = config.base_url;
    if (base_url.empty()) {
        if (config.type == "claude") {
            base_url = "https://api.anthropic.com";
        } else if (config.type == "openai") {
            base_url = "https://api.openai.com";
        } else {
            base_url = "https://open.bigmodel.cn";
        }
    }

    auto uri = chen::Uri::Create(base_url + provider->endpointSuffix());
    if (!uri) {
        return ClientError::MalformedResponse;
    }

    std::string body = provider->buildRequest(config.model, config.max_tokens, system_prompt, prompt, true);
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    provider->authHeaders(config.api_key, headers);

    int attempts = config.max_attempts > 0 ? config.max_attempts : 1;
    ClientError last_error = ClientError::Unknown;

    for (int attempt = 0; attempt < attempts; ++attempt) {
        bool emitted = false;
        auto chunk_cb = [&](const std::string& chunk) {
            emitted = true;
            if (on_chunk) {
                on_chunk(chunk);
            }
        };

        SSEStreamParser parser(provider, chunk_cb, on_error, nullptr);
        auto http_result = chen::http::HttpConnection::DoRequestStreaming(
            chen::http::HttpMethod::POST, uri, (uint64_t)config.timeout_ms,
            [&parser](const char* data, size_t len) {
                return parser(data, len);
            },
            headers, body);

        if (!http_result || http_result->result != 0) {
            last_error = (!http_result || http_result->result == (int)chen::http::HttpResult::Error::TIMEOUT)
                ? ClientError::Timeout
                : ClientError::ConnectFailed;
            if (!emitted && attempt + 1 < attempts) {
                WARN(logger) << "llm request retry " << (attempt + 1) << "/" << attempts;
                Backoff(config, attempt);
                continue;
            }
            return last_error;
        }

        int status = http_result->response ? (int)http_result->response->getStatus() : 0;
        if (status == 200) {
            full_text = parser.full_text;
            return ClientError::Ok;
        }

        last_error = MapStatus(status);
        if ((last_error == ClientError::ServerError || last_error == ClientError::RateLimited) && !emitted
            && attempt + 1 < attempts) {
            WARN(logger) << "llm request retry " << (attempt + 1) << "/" << attempts << " status=" << status;
            Backoff(config, attempt);
            continue;
        }
        return last_error;
    }

    return last_error;
}
