#include "ServletRegistrar.hpp"

#include <chen/http/servlet.h>

#include "BlogServlet.hpp"

ServletRegistrar::ServletRegistrar(chen::http::ServletDispatch::ptr dispatch)
    : m_dispatch(std::move(dispatch))
{
}

ServletRegistrar* ServletRegistrar::operator->()
{
    return this;
}

void ServletRegistrar::addServlet(const std::string& uri, chen::http::Servlet::ptr servlet)
{
    if (!m_dispatch || !servlet) {
        return;
    }
    auto slt = std::dynamic_pointer_cast<BlogServlet>(servlet);
    if (slt) {
        slt->setRoute(uri);
    }
    m_dispatch->addServlet(uri, servlet);
}
