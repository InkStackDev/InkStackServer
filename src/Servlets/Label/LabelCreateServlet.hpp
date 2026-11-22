#pragma once

#include "../BlogLoginedServlet.hpp"

#include "../../SessionData.hpp"

class LabelCreateServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<LabelCreateServlet> ptr;
    LabelCreateServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
