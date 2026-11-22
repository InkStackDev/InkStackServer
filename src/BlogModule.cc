#include "BlogModule.hpp"

#include "./Logic/BlogMsgFromBot.hpp"
#include "./Logic/BlogMsgHttp.hpp"
#include "./Logic/BlogMsgWS.hpp"
#include "Config/RpcConfig.hpp"

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

    return true;
}

bool BlogModule::onDeactivate()
{

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
