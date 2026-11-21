#include "BlogModule.hpp"

#include "./Logic/BlogMsgFromBot.hpp"
#include "./Logic/BlogMsgHttp.hpp"
#include "./Logic/BlogMsgWS.hpp"

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
}

uint64_t BlogModule::getTickIntervalMs()
{
    // 设置定时器间隔为1分钟
    return 60 * 1000;
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

    chen::rpc::RpcClient::ptr client = std::make_shared<chen::rpc::RpcClient>();
    ASSERT_RET(client, false);

    chen::Address::ptr address = std::make_shared<chen::IPv4Address>();

    // client->connect()

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
