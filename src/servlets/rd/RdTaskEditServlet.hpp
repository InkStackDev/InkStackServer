#pragma once

#include "../BlogLoginedServlet.hpp"

#include "../../SessionData.hpp"
#include <json/json.h>

#include "DB/TaskInfo.hpp"

class RdTaskEditServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<RdTaskEditServlet> ptr;
    RdTaskEditServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;

private:
    void notifyAssignee(int64_t assignee_id, DB::TaskInfo::ptr task);
};
