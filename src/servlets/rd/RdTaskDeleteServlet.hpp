#pragma once

#include "../../SessionData.hpp"
#include <json/json.h>

class RdTaskDeleteServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<RdTaskDeleteServlet> ptr;
    RdTaskDeleteServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
