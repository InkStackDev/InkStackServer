#include "PasskeyDeleteServlet.hpp"

#include "../../Manager/PasskeyManager.hpp"

PasskeyDeleteServlet::PasskeyDeleteServlet()
    : BlogLoginedServlet("PasskeyDeleteServlet")
{
}

int32_t PasskeyDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        int64_t id = request->getParamAs<int64_t>("id", 0);
        if (id <= 0) {
            result->setErrno(PARAM_INVALID);
            break;
        }

        auto passkey = PasskeyMgr::GetInstance()->get(id);
        if (!passkey || passkey->getUserId() != uid) {
            result->setErrno(PASSKEY_NOT_FOUND);
            break;
        }

        if (PasskeyMgr::GetInstance()->remove(id) != 0) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->setErrno(SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
