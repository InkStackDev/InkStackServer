#include "UserAdminDetailServlet.hpp"

#include "../../Service/AdminUserService.hpp"
#include "../../Util/Util.hpp"

UserAdminDetailServlet::UserAdminDetailServlet()
    : BlogAdminServlet("UserAdminDetailServlet")
{
}

int32_t UserAdminDetailServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, user_id, "user_id");

        AdminUserService::Detail detail;
        if (!AdminUserService::GetDetail(user_id, detail)) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }

        result->set("id", detail.id);
        result->set("account", detail.account);
        result->set("name", detail.name);
        result->set("email", detail.email);
        result->set("avatar", detail.avatar);
        result->set("role", detail.role);
        result->set("state", detail.state);
        result->set("create_time", detail.create_time);
        result->set("login_time", detail.login_time);
        result->set("article_count", detail.article_count);
        result->set("comment_count", detail.comment_count);
        result->set("bio", detail.bio);
        result->set("website", detail.website);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
