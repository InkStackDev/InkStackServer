#include "BlogModule.hpp"

#include "./Client/HubClient.hpp"
#include "./Config/AppConfig.hpp"
#include "./DB/DBManager.hpp"
#include "./Event/Events.hpp"
#include "./Job/JobRunner.hpp"
#include "./Logic/BlogMsgFromBot.hpp"
#include "./Logic/BlogMsgHttp.hpp"
#include "./Logic/BlogMsgWS.hpp"
#include "./Manager/MessageConfigManager.hpp"
#include "./Manager/WSConnectionManager.hpp"
#include "./Message/MessageRouter.hpp"
#include "./Metrics/MetricsService.hpp"
#include "./Search/SearchSync.hpp"
#include "./Service/ArticleCounterService.hpp"
#include "./Service/ArticleScheduleService.hpp"
#include "./Service/DailyRecommendService.hpp"
#include "./Service/NotificationService.hpp"
#include "./Service/OrganizationRepoPrService.hpp"
#include "./Service/OrganizationRepoService.hpp"
#include "./Service/UserService.hpp"

#include <functional>
#include <vector>

#include <Chen/Application.hpp>
#include <Chen/DB/Redis.hpp>
#include <Chen/Http/SessionData.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

void RunShutdownStep(const char* stage, const std::function<void()>& step)
{
    const auto start_us = chen::TimeUtil::GetCurrentUs();
    INFO(LOG_ROOT()) << "shutdown: " << stage << " begin";
    step();
    INFO(LOG_ROOT()) << "shutdown: " << stage << " done in " << (chen::TimeUtil::GetCurrentUs() - start_us) << "us";
}

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

    const std::string config_path = work_path + "/config";
    if (!MessageConfigMgr::GetInstance()->load(config_path)) {
        ERROR(LOG_ROOT()) << "failed to load message config: " << config_path;
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
    INFO(LOG_ROOT()) << "shutdown: onUnload begin";
    RunShutdownStep("JobRunner::StopAll", []() {
        JobRunner::StopAll();
    });
    RunShutdownStep("ArticleCounterService::FlushDirty", []() {
        ArticleCounterService::FlushDirty();
    });
    RunShutdownStep("HubClient::Shutdown", []() {
        HubClient::Shutdown();
    });
    RunShutdownStep("SearchSyncMgr::stop", []() {
        SearchSyncMgr::GetInstance()->stop();
    });
    RunShutdownStep("MessageRouter::Shutdown", []() {
        MessageRouter::Shutdown();
    });
    RunShutdownStep("RedisMgr::freeAll", []() {
        chen::RedisMgr::GetInstance()->freeAll();
    });
    RunShutdownStep("FoxThreadMgr::stop", []() {
        chen::FoxThreadMgr::GetInstance()->stop();
    });
    INFO(LOG_ROOT()) << "shutdown: onUnload done";
    return true;
}

bool BlogModule::onDeactivate()
{
    INFO(LOG_ROOT()) << "shutdown: onDeactivate begin";
    // 停止接受请求后先把内存脏计数落库，再关闭长连接
    RunShutdownStep("ArticleCounterService::FlushDirty", []() {
        ArticleCounterService::FlushDirty();
    });
    // 热重载/关机：让客户端主动重连到新 worker，避免一直挂到 drain 超时
    RunShutdownStep("WSMgr::closeAllConnections", []() {
        WSMgr::GetInstance()->closeAllConnections(
            R"({"channel":"system","type":"reloading","reason":"server_reloading"})");
    });
    INFO(LOG_ROOT()) << "shutdown: onDeactivate done";
    return true;
}

void BlogModule::onTick()
{
    static uint32_t tick = 0;
    ++tick;

    // 5s 发布一次本进程指标快照（供集群聚合读取）
    if (tick % 50 == 0) {
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
    std::vector<chen::TcpServer::ptr> all_servers;
    chen::Application::GetInstance()->getAllServers(all_servers);

    std::vector<chen::http::HttpServer::ptr> http_servers;
    std::vector<chen::http::WSServer::ptr> ws_servers;
    for (auto& server : all_servers) {
        if (auto hs = std::dynamic_pointer_cast<chen::http::HttpServer>(server)) {
            http_servers.push_back(hs);
            continue;
        }
        if (auto ws = std::dynamic_pointer_cast<chen::http::WSServer>(server)) {
            ws_servers.push_back(ws);
        }
    }

    if (http_servers.empty()) {
        ERROR(LOG_ROOT()) << "No HTTP server found";
        return false;
    }

    if (ws_servers.empty()) {
        ERROR(LOG_ROOT()) << "No WS server found";
        return false;
    }

    for (auto& hs : http_servers) {
        if (!BlogMsgHttpInit(hs)) {
            ERROR(LOG_ROOT()) << "BlogMsgHttpInit failed";
            return false;
        }
    }

    for (auto& ws : ws_servers) {
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

    // 每日推荐快照：10 分钟检查一次，跨实例/跨重启全局 3 小时最多生成一次；
    // 任务体按“当天快照是否已存在”幂等，跨天时第一次检查就会补上当天榜单
    JobRunner::Register(JobRunner::Job{
        .name = "article:daily_recommend",
        .interval_ms = 10 * 60 * 1000,
        .lease_ttl_sec = 10 * 60,
        .run_immediately = true,
        .dedup_interval_ms = 3 * 60 * 60 * 1000,
        .run =
            []() {
                DailyRecommendService::EnsureSnapshot(DailyRecommendService::Today());
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
