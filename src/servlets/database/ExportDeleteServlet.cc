#include "ExportDeleteServlet.hpp"

#include "../../Repository/RecordRepository.hpp"
#include "../../Service/RecordService.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Domain/UserTypes.hpp"

ExportDeleteServlet::ExportDeleteServlet()
    : BlogLoginedServlet("ExportDeleteServlet")
{
}

int32_t ExportDeleteServlet::handle(
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
        if (current_user->getRole() != UserRole::ADMIN) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        std::string id_str = request->getParam("id");
        if (id_str.empty()) {
            result->setErrno(PARAM_MISSING, "param id is required");
            break;
        }
        int64_t id = chen::TypeUtil::Atoi(id_str);
        if (!id) {
            result->setErrno(PARAM_INVALID, "invalid id");
            break;
        }

        auto info = ExportRecordRepository::Get(id, GetDB());
        if (!info) {
            result->setErrno(DB_OPERATION_FAILED, "export record not found");
            break;
        }

        if (!ExportRecordService::Remove(id)) {
            result->setErrno(DB_OPERATION_FAILED, "delete export failed");
            break;
        }

        result->setErrno(SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
