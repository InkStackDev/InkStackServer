#include "BlogModule.hpp"

#include "./Client/HubClient.hpp"
#include "./Config/AppConfig.hpp"
#include "./DB/DBManager.hpp"
#include "./Event/Events.hpp"
#include "./Job/JobRunner.hpp"
#include "./Logic/BlogMsgFromBot.hpp"
#include "./Logic/BlogMsgHttp.hpp"
#include "./Logic/BlogMsgWS.hpp"
#include "./Manager/ErrorCodeManager.hpp"
#include "./Manager/WSConnectionManager.hpp"
#include "./Message/MessageRouter.hpp"
#include "./Metrics/MetricsService.hpp"
#include "./Search/SearchSync.hpp"
#include "./Service/ArticleCounterService.hpp"
#include "./Service/ArticleScheduleService.hpp"
#include "./Service/NotificationService.hpp"
#include "./Service/OrganizationRepoPrService.hpp"
#include "./Service/OrganizationRepoService.hpp"
#include "./Service/UserService.hpp"

#include <vector>

#include <Chen/DB/Redis.hpp>
#include <Chen/Http/SessionData.hpp>
#include <Chen/Log/Log.hpp>

BlogModule::BlogModule()
    : Module("Blog", "1.0.8", "blog_module")
{
}

bool BlogModule::onLoad()
{
    if (!DBManager::CreateAllTables()) {
        ERROR(LOG_ROOT()) << "DBManager::CreateAllTables failed";
        return false;
    }

    if (!DBManager::MigrateAllTables()) {
        ERROR(LOG_ROOT()) << "DBManager::MigrateAllTables failed";
        return false;
    }

    const std::string work_path = GetServerWorkPath();

    const std::string errors_path = work_path + "/errors.json";
    if (!ErrorCodeMgr::GetInstance()->load(errors_path)) {
        ERROR(LOG_ROOT()) << "failed to load error codes: " << errors_path;
        return false;
    }

    // 首次部署引导：admin.passwd 为空则跳过（不改动已存在的管理员）
    if (!UserService::EnsureSuperAdmin()) {
        ERROR(LOG_ROOT()) << "super admin bootstrap failed";
    }

    chen::FoxThreadMgr::GetInstance()->init();
    chen::FoxThreadMgr::GetInstance()->start();
    chen::RedisMgr::GetInstance()->init();

    auto rds = chen::RedisMgr::GetInstance()->get(GetRedisPoolName());
    if (!rds) {
        ERROR(LOG_ROOT()) << "redis pool unavailable: " << GetRedisPoolName();
    } else {
        INFO(LOG_ROOT()) << "redis pool ready: " << GetRedisPoolName();
    }
    return true;
}

bool BlogModule::onUnload()
{
    JobRunner::StopAll();
    ArticleCounterService::FlushDirty();
    HubClient::Shutdown();
    SearchSyncMgr::GetInstance()->stop();
    MessageRouter::Shutdown();

    chen::RedisMgr::GetInstance()->freeAll();
    chen::FoxThreadMgr::GetInstance()->stop();
    return true;
}

bool BlogModule::onDeactivate()
{
    // 停止接受请求后先把内存脏计数落库，再关闭长连接
    ArticleCounterService::FlushDirty();
    // 热重载/关机：让客户端主动重连到新 worker，避免一直挂到 drain 超时
    WSMgr::GetInstance()->closeAllConnections(R"({"channel":"system","type":"reloading","reason":"server_reloading"})");
    return true;
}

void BlogModule::onTick()
{
    static uint32_t tick = 0;
    ++tick;

    // 5s 采样一次路由 QPS
    if (tick % 50 == 0) {
        SampleRates();
        PublishSnapshot();
    }

    // 60s 清理一次进程内会话缓存，避免长跑内存只增不减
    if (tick % 600 == 0) {
        chen::http::SessionDataMgr::GetInstance()->check(3600);
    }

    HubClient::Tick();
}

uint64_t BlogModule::getTickIntervalMs()
{
    return 100;
}

bool BlogModule::onServerReady()
{
    std::vector<chen::http::HttpServer::ptr> http_servers;
    getAllHttpServer(http_servers);

    if (http_servers.empty()) {
        ERROR(LOG_ROOT()) << "No HTTP server found";
        return false;
    }

    std::vector<chen::http::WSServer::ptr> ws_servers;
    getAllWSServer(ws_servers);

    if (ws_servers.empty()) {
        ERROR(LOG_ROOT()) << "No WS server found";
        return false;
    }

    for (auto& i : http_servers) {
        const auto hs = std::dynamic_pointer_cast<chen::http::HttpServer>(i);
        ASSERT_RET(hs, false);

        if (!BlogMsgHttpInit(hs)) {
            ERROR(LOG_ROOT()) << "BlogMsgHttpInit failed";
            return false;
        }
    }

    for (auto& i : ws_servers) {
        auto ws = std::dynamic_pointer_cast<chen::http::WSServer>(i);
        ASSERT_RET(ws, false);

        if (!BlogMsgWSInit(ws)) {
            ERROR(LOG_ROOT()) << "BlogMsgWSInit failed";
            return false;
        }
    }

    Init(getName(), getVersion(), http_servers, ws_servers);
    PublishSnapshot();

    // 全进程只维护一条 hub 连接；重连成功后重新注册本地 handler
    HubClient::SetConnectedCallback([](const chen::rpc::RpcClient::ptr& client) {
        if (!BlogMsgFromBotInit(client)) {
            ERROR(LOG_ROOT()) << "BlogMsgFromBotInit failed";
        }
    });
    HubClient::Init();

    EventMsgsInit();

    return true;
}

bool BlogModule::onServerUp()
{
    MessageRouter::Init();

    if (!SearchSyncMgr::GetInstance()->init()) {
        WARN(LOG_ROOT()) << "SearchSync init failed, search sync disabled";
    }

    // 文章定时发布：多实例只需要一个执行者，保留租约
    JobRunner::Register(JobRunner::Job{
        .name = "article:publish",
        .interval_ms = 5000,
        .lease_ttl_sec = 30,
        .run_immediately = true,
        .run =
            []() {
                ArticleScheduleService::PublishDue();
            },
    });

    // 计数刷盘：每个实例只刷自己进程内的增量，不能用租约（否则其它实例的增量要等抢到租约）
    JobRunner::Register(JobRunner::Job{
        .name = "article:flush",
        .interval_ms = 5000,
        .run_immediately = true,
        .use_lease = false,
        .run =
            []() {
                ArticleCounterService::FlushDirty();
            },
    });

    // 组织仓库元数据同步：5 分钟检查一次，跨实例/跨重启全局 12 小时最多成功执行一次
    JobRunner::Register(JobRunner::Job{
        .name = "org:repo_sync",
        .interval_ms = 5 * 60 * 1000,
        .lease_ttl_sec = 20 * 60,
        .run_immediately = true,
        .dedup_interval_ms = 12 * 60 * 60 * 1000,
        .run =
            []() {
                OrganizationRepoService::SyncAll();
            },
    });

    // 组织 PR 同步（webhook 之外的低频兜底，注意 GitHub API 限流）
    // 1 分钟检查一次，跨实例/跨重启全局 15 分钟最多成功执行一次
    JobRunner::Register(JobRunner::Job{
        .name = "org:pr_sync",
        .interval_ms = 60 * 1000,
        .lease_ttl_sec = 10 * 60,
        .run_immediately = true,
        .dedup_interval_ms = 90 * 60 * 1000,
        .run =
            []() {
                OrganizationRepoPrService::SyncAll();
            },
    });

    // 过期广播清理
    JobRunner::Register(JobRunner::Job{
        .name = "notify:broadcast_cleanup",
        .interval_ms = 5 * 60 * 1000,
        .lease_ttl_sec = 4 * 60,
        .run_immediately = false,
        .run =
            []() {
                NotificationService::CleanupExpiredBroadcasts();
            },
    });

    JobRunner::StartAll();
    return true;
}

extern "C" {

chen::Module* CreateModule()
{
    chen::Module* module = new BlogModule;
    INFO(LOG_ROOT()) << "BlogModule::CreateModule";
    return module;
}

void DestroyModule(const chen::Module* module)
{
    INFO(LOG_ROOT()) << "BlogModule::DestroyModule";
    delete module;
}
}
