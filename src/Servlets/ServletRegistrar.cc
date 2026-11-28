#include "ServletRegistrar.hpp"

#include <Chen/Http/Http.hpp>
#include <Chen/Http/Servlet.hpp>

#include "BlogServlet.hpp"

ServletRegistrar::ServletRegistrar(chen::http::ServletDispatch::ptr dispatch)
    : dispatch_(std::move(dispatch))
{
}

ServletRegistrar* ServletRegistrar::operator->()
{
    return this;
}

void ServletRegistrar::addServlet(const std::string& uri,
    chen::http::HttpMethod method,
    chen::http::Servlet::ptr servlet)
{
    if (!dispatch_ || !servlet) {
        return;
    }
    auto slt = std::dynamic_pointer_cast<BlogServlet>(servlet);
    if (slt) {
        const char* method_str = chen::http::HttpMethodToString(method);
        slt->setRoute(uri, method_str ? method_str : "UNKNOWN");
    }
    dispatch_->addServlet(uri, method, servlet);
}
