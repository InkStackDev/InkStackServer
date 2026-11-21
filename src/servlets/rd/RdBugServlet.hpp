#pragma once

#include "../../SessionData.hpp"
#include <json/json.h>

class RdBugServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<RdBugServlet> ptr;
    RdBugServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
