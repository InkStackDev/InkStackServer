#pragma once

#include <chen/http/servlet.h>

class ServletRegistrar {
public:
    explicit ServletRegistrar(chen::http::ServletDispatch::ptr dispatch);

    ServletRegistrar* operator->();

    void addServlet(const std::string& uri, chen::http::Servlet::ptr servlet);

private:
    chen::http::ServletDispatch::ptr m_dispatch;
};
