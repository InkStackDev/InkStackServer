#include "BlogModule.hpp"

#include "./Job/JobRunner.hpp"
#include "./Logic/BlogMsgFromBot.hpp"
#include "./Logic/BlogMsgHttp.hpp"
#include "./Logic/BlogMsgWS.hpp"
#include "./Manager/NotificationManager.hpp"
#include "./Manager/WSConnectionManager.hpp"
#include "./Message/MessageRouter.hpp"
#include "./Search/SearchSync.hpp"
#include "Config/RpcConfig.hpp"
#include "Service/OrganizationRepoService.hpp"
#include "Service/OrganizationRepoPrService.hpp"
#include "Service/ArticleCounterService.hpp"
#include "Service/ArticleScheduleService.hpp"

BlogModule::BlogModule()
    : Module("Blog", "1.0.8", "blog_module")
{
}

bool BlogModule::onLoad()
{

    return true;
}

bool BlogModule::onUnload()
{
    JobRunner::StopAll();
    ArticleCounterService::FlushDirty();
    SearchSyncMgr::GetInstance()->stop();
    MessageRouter::Shutdown();
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
    if (client_->isConnected()) {
        return;
    }

    std::string hub_ip = GetRpcConf().getHubIP();
    uint16_t hub_port = GetRpcConf().getHubPort();
    chen::Address::ptr address = chen::IPv4Address::Create(hub_ip.c_str(), hub_port);
    ASSERT_RET(address);

    uint64_t conn_timeout = GetRpcConf().connect_timeout_ms;
    if (!client_->connect(address, conn_timeout)) {
        ERROR(LOG_ROOT()) << "Failed to connect to hub server";
    }
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

    client_ = std::make_shared<chen::rpc::RpcClient>();
    ASSERT_RET(client_, false);

    uint64_t hb_interval_ms = GetRpcConf().heartbeat_interval_ms;
    client_->setHeartbeatInterval(hb_interval_ms);

    std::string hub_ip = GetRpcConf().getHubIP();
    uint16_t hub_port = GetRpcConf().getHubPort();
    chen::Address::ptr address = chen::IPv4Address::Create(hub_ip.c_str(), hub_port);
    ASSERT_RET(address, false);

    uint64_t conn_timeout = GetRpcConf().connect_timeout_ms;
    if (!client_->connect(address, conn_timeout)) {
        ERROR(LOG_ROOT()) << "Failed to connect to hub server";
        return false;
    }

    if (!BlogMsgFromBotInit(client_)) {
        ERROR(LOG_ROOT()) << "BlogMsgFromBotInit failed";
        return false;
    }

    return true;
}

bool BlogModule::onServerUp()
{
    MessageRouter::Init();

    if (!SearchSyncMgr::GetInstance()->init()) {
        WARN(LOG_ROOT()) << "SearchSync init failed, search sync disabled";
    }

    // 文章定时发布 + 计数刷盘（原先 onTick 未被调用）
    JobRunner::Register(JobRunner::Job{
        .name = "article:tick",
        .interval_ms = 5000,
        .lease_ttl_sec = 30,
        .run_immediately = true,
        .run =
            []() {
                ArticleScheduleService::PublishDue();
                ArticleCounterService::FlushDirty();
            },
    });

    // 组织仓库元数据同步（低频，避免启动时集中请求 GitHub）
    JobRunner::Register(JobRunner::Job{
        .name = "org:repo_sync",
        .interval_ms = 60 * 60 * 1000,
        .lease_ttl_sec = 20 * 60,
        .run_immediately = false,
        .run =
            []() {
                OrganizationRepoService::SyncAll();
            },
    });

    // 组织 PR 同步（webhook 之外的低频兜底，注意 GitHub API 限流）
    JobRunner::Register(JobRunner::Job{
        .name = "org:pr_sync",
        .interval_ms = 15 * 60 * 1000,
        .lease_ttl_sec = 10 * 60,
        .run_immediately = false,
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
                NotificationMgr::GetInstance()->cleanupExpiredBroadcasts();
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
