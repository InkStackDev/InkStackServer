/**
 * @file SSEStreamParser.hpp
 * @brief SSE 流解析器 — 逐行解析 AI 返回的 SSE，通过 AIProvider 提取文本并回调
 * @author Christins
 * @date 2026-06-10
 * @copyright Apache 2.0
 */
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "AIProvider.hpp"

/// SSE 流式回调解析器，每收到一块 body 数据就逐行解析、提取文本并回调
class SSEStreamParser {
public:
    typedef std::shared_ptr<SSEStreamParser> ptr;
    typedef std::function<void(const std::string&)> TextCallback;
    typedef std::function<void(const std::string&)> ErrorCallback;
    typedef std::function<void()> DoneCallback;

    SSEStreamParser(AIProvider::ptr provider, TextCallback on_chunk, ErrorCallback on_error, DoneCallback on_done);

    /// HttpStreamCallback 接口
    bool operator()(const char* data, size_t len);

    /// 累积的完整总结文本（流结束后读取，用于缓存）
    std::string full_text;

private:
    void processEvent();

    AIProvider::ptr provider_;
    TextCallback on_chunk_;
    ErrorCallback on_error_;
    DoneCallback on_done_;
    std::string line_buf_;
    std::string current_event_;
};
