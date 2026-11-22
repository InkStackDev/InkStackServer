#include "UserExistsServlet.hpp"

#include "../../Service/UserQueryService.hpp"
#include "../../Util/Util.hpp"

UserExistsServlet::UserExistsServlet()
    : BlogServlet("UserExistsServlet")
{
}

int32_t UserExistsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, auth_id, "auth_id");

        bool exists = false;
        auto rt = UserQueryService::Exists(auth_id, exists);
        if (rt == UserQueryService::ExistsResult::InvalidAuthId) {
            result->setErrno(AUTH_CODE_INVALID);
            break;
        }
        if (rt != UserQueryService::ExistsResult::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->setErrno(SUCCESS);
        result->set("is_exists", exists ? "1" : "0");
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
