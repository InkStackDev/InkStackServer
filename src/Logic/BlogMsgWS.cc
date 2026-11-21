#include "BlogMsgWS.hpp"

#include "../Servlets/Servlets.hpp"

bool BlogMsgWSInit(chen::http::WSServer::ptr server)
{
    ASSERT_RET(server, false);

    chen::http::ServletDispatch::ptr dp = server->getWSServletDispatch();
    ASSERT_RET(dp, false);

    dp->addServlet("/ws/v1/session", std::make_shared<WSGatewayServlet>());

    return true;
}