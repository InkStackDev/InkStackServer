/**
 * @file SearchClient.hpp
 * @brief 搜索服务 RPC 客户端（连接、查询、索引更新）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#include <Chen/Rpc/RpcClient.hpp>

#include "protocol_ss_search.h"

class SearchClient {
public:
    bool EnsureConnected();

    void Close();

    bool CheckStats(tagSearchStats& stats);

    bool Query(const tagSearchQuery& query, tagSearchResult& result);

    int32_t Upsert(const tagSearchDoc& doc);

    int32_t Remove(uint64_t article_id, int64_t update_time);

    int32_t RebuildBegin(uint64_t snapshot_id, int64_t started_at);

    int32_t RebuildDocs(uint64_t snapshot_id, const std::vector<tagSearchDoc>& docs);

    int32_t RebuildCommit(uint64_t snapshot_id, int64_t finished_at);

private:
    chen::rpc::RpcClient::ptr GetClient();

    void Invalidate();

    std::mutex mutex_;
    chen::rpc::RpcClient::ptr client_;
    std::atomic<uint64_t> last_connect_ms_{0};
};
