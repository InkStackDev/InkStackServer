/**
 * @file SearchClient.hpp
 * @brief 搜索服务 RPC 调用（使用进程内共享的 hub 连接）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <vector>

#include <Chen/Rpc/RpcClient.hpp>

#include "protocol_ss_search.h"

/**
 * @brief 搜索服务 RPC 调用
 * @details 不再自行建立连接：统一使用 HubClient 中由 BlogModule 维护的那条
 *          RpcClient（同一进程只保留一个 hub 连接）。
 */
class SearchClient {
public:
    /// 共享 hub 连接是否可用
    bool EnsureConnected();

    /// 兼容旧接口：共享连接由 BlogModule 负责关闭，这里不做任何事
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
};
