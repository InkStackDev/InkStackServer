/**
 * @file AdminFaqDeleteServlet.cc
 * @brief 管理端 - 删除常见问题实现
 * @author Christins
 * @date 2026-07-16
 * @copyright Apache 2.0
 */
#include "AdminFaqDeleteServlet.hpp"

#include "../../Include/Managers.hpp"
#include "../../Util/Util.hpp"

#include <Chen/Log/Log.hpp>
#include "../../Service/FaqService.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Domain/UserTypes.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

AdminFaqDeleteServlet::AdminFaqDeleteServlet()
    : BlogLoginedServlet("AdminFaqDeleteServlet")
{
}

int32_t AdminFaqDeleteServlet::handle(
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

        auto current_user = UserRepository::Get(uid, GetDB());
        if (!current_user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }

        int32_t role = current_user->getRole();
        if (role != UserRole::ADMIN) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        DEFINE_AND_CHECK_STRING(result, id_str, "id");

        int64_t id = 0;
        try {
            id = chen::TypeUtil::Atoi(id_str);
        } catch (...) {
            result->setErrno(PARAM_INVALID, "invalid id");
            break;
        }

        if (!FaqService::Remove(id)) {
            result->setErrno(FAQ_NOT_FOUND);
            break;
        }

        result->setErrno(SUCCESS);
    } while (0);

    DEBUG(logger) << "AdminFaqDeleteServlet handle result: " << result->toJsonString();
    response->setBody(result->toJsonString());
    return 0;
}
