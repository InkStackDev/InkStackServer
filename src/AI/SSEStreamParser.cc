#include "SSEStreamParser.hpp"

#include <json/json.h>
#include <json/reader.h>

#include <utility>

SSEStreamParser::SSEStreamParser(AIProvider::ptr provider,
    TextCallback on_chunk,
    ErrorCallback on_error,
    DoneCallback on_done)
    : provider_(std::move(provider))
    , on_chunk_(std::move(on_chunk))
    , on_error_(std::move(on_error))
    , on_done_(std::move(on_done))
{
}

bool SSEStreamParser::operator()(const char* data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        char c = data[i];
        if (c == '\n') {
            if (!line_buf_.empty() && line_buf_.back() == '\r') {
                line_buf_.pop_back();
            }

            if (line_buf_.empty()) {
                if (!current_event_.empty()) {
                    processEvent();
                    current_event_.clear();
                }
            } else if (line_buf_.size() >= 6 && line_buf_.substr(0, 6) == "data: ") {
                current_event_ = line_buf_.substr(6);
            }
            line_buf_.clear();
        } else {
            line_buf_ += c;
        }
    }
    return true;
}

void SSEStreamParser::processEvent()
{
    if (provider_->isDoneMarker(current_event_)) {
        if (on_done_) {
            on_done_();
        }
        return;
    }

    Json::Value parsed;
    Json::Reader reader;
    if (!reader.parse(current_event_, parsed)) {
        return;
    }

    std::string content = provider_->extractContent(parsed);
    if (!content.empty()) {
        if (on_chunk_) {
            on_chunk_(content);
        }
        full_text += content;
    }

    if (provider_->isTerminal(parsed)) {
        if (on_done_) {
            on_done_();
        }
        return;
    }

    std::string err_msg = provider_->getError(parsed);
    if (!err_msg.empty() && on_error_) {
        on_error_(err_msg);
    }
}
