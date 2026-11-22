#pragma once

#include "../BlogLoginedServlet.hpp"

#include "../../SessionData.hpp"
#include <json/json.h>

#include "DB/BugInfo.hpp"

class RdBugEditServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<RdBugEditServlet> ptr;
    RdBugEditServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;

private:
    void notifyAssignee(int64_t assignee_id, DB::BugInfo::ptr bug);
};
