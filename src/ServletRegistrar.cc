#include "ServletRegistrar.hpp"

#include <chen/http/servlet.h>

#include "BlogServlet.hpp"

ServletRegistrar::ServletRegistrar(chen::http::ServletDispatch::ptr dispatch)
    : dispatch_(std::move(dispatch))
{
}

ServletRegistrar* ServletRegistrar::operator->()
{
    return this;
}

void ServletRegistrar::addServlet(const std::string& uri, chen::http::Servlet::ptr servlet)
{
    if (!dispatch_ || !servlet) {
        return;
    }
    auto slt = std::dynamic_pointer_cast<BlogServlet>(servlet);
    if (slt) {
        slt->setRoute(uri);
    }
    dispatch_->addServlet(uri, servlet);
}
