#pragma once

#include "../BlogLoginedServlet.hpp"

#include "../../SessionData.hpp"
#include <json/json.h>

class RdBugDetailServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<RdBugDetailServlet> ptr;
    RdBugDetailServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
