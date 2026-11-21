#pragma once

#include "../../SessionData.hpp"

class OrganizationApplyReviewServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<OrganizationApplyReviewServlet> ptr;
    OrganizationApplyReviewServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
