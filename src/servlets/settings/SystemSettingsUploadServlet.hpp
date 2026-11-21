#pragma once

#include "../../SessionData.hpp"

class SystemSettingsUploadServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<SystemSettingsUploadServlet> ptr;
    SystemSettingsUploadServlet();

protected:
    virtual int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
