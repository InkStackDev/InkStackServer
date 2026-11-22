/**
 * @file AdminFeedbackDeleteServlet.hpp
 * @brief 管理端 - 删除反馈
 * @author Christins
 * @date 2026-07-16
 * @copyright Apache 2.0
 */
#pragma once

#include "../BlogLoginedServlet.hpp"

#include "../../SessionData.hpp"

class AdminFeedbackDeleteServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<AdminFeedbackDeleteServlet> ptr;
    AdminFeedbackDeleteServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
