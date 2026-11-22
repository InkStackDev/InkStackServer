#pragma once

#include "../BlogLoginedServlet.hpp"

#include "../../SessionData.hpp"
#include <json/json.h>

class RdRequirementDetailServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<RdRequirementDetailServlet> ptr;
    RdRequirementDetailServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
