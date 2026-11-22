/**
 * @file SearchSync.hpp
 * @brief 搜索索引同步器
 * @author Christins
 * @date 2026-11-19
 * @copyright Apache 2.0
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>

#include <Chen/Timer/Timer.hpp>
#include <Chen/Util/Singleton.hpp>

#include "../Client/SearchClient.hpp"
#include "protocol_ss_search.h" // IWYU pragma: keep

/**
 * @brief 搜索索引同步器
 * @details 只负责 outbox、批量调度与全量快照；连接、超时、RPC 调用在 SearchClient。
 *          待同步文章 ID 同时写入 Redis hash（持久化 outbox）和进程内 map，
 *          SearchServer 不可用时不会丢失变更。
 */
class SearchSync {
public:
    enum Op : int32_t {
        OP_UPSERT = 1,
        OP_REMOVE = 2
    };

    SearchSync() = default;

    /**
     * @brief 初始化连接与定时任务
     */
    bool init();

    /**
     * @brief 停止客户端
     */
    void stop();

    /**
     * @brief 标记文章需要同步
     * @param article_id 文章 ID
     * @param op OP_UPSERT / OP_REMOVE
     */
    void markDirty(uint64_t article_id, int32_t op);

    /**
     * @brief 查询文章
     * @return false 表示 RPC 不可用或服务端返回错误
     */
    bool query(const tagSearchQuery& query, tagSearchResult& result);

private:
    struct DirtyEntry {
        int32_t op = 0;
        int64_t ts = 0;
    };

    void wake();

    void syncLoop();

    bool runSnapshot();

    bool flushDirty();

    bool buildDoc(uint64_t article_id, int64_t update_time, tagSearchDoc& doc, bool& missing);

    std::map<uint64_t, DirtyEntry> loadDirty();

    void cacheDirty(uint64_t article_id, const DirtyEntry& entry);

    void ackDirty(uint64_t article_id, const DirtyEntry& entry);

private:
    std::atomic<bool> enabled_{false};
    std::atomic<bool> stopped_{false};
    std::atomic<bool> sync_scheduled_{false};
    std::atomic<bool> syncing_{false};
    std::atomic<uint64_t> last_stats_ms_{0};

    SearchClient client_;

    std::mutex dirty_mutex_;
    std::map<uint64_t, DirtyEntry> local_dirty_;

    chen::Timer::ptr timer_;
};

typedef chen::LeakySingleton<SearchSync> SearchSyncMgr;
