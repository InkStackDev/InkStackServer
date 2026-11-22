#include "UserExistsServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Util/Util.hpp"
#include "../../Repository/UserRepository.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

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
        DB::UserInfo::ptr info;
        if (IsEmail(auth_id)) {
            info = UserRepository::GetByEmail(auth_id, GetDB());
        } else if (IsValidAccount(auth_id)) {
            info = UserRepository::GetByAccount(auth_id, GetDB());
        } else {
            result->setErrno(AUTH_CODE_INVALID);
            break;
        }
        result->setErrno(SUCCESS);
        result->set("is_exists", info ? "1" : "0");
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
