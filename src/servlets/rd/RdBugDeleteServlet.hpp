#pragma once

#include "../BlogLoginedServlet.hpp"

#include "../../SessionData.hpp"
#include <json/json.h>

class RdBugDeleteServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<RdBugDeleteServlet> ptr;
    RdBugDeleteServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
