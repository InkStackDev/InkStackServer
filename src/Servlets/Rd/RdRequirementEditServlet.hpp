#pragma once

#include "../BlogLoginedServlet.hpp"

#include "../../SessionData.hpp"
#include "DB/RequirementInfo.hpp"
#include <json/json.h>

class RdRequirementEditServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<RdRequirementEditServlet> ptr;
    RdRequirementEditServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;

private:
    void notifyAssignee(int64_t assignee_id, DB::RequirementInfo::ptr requirement);
};
