#include "ExportDeleteServlet.hpp"

#include "../../Service/RecordService.hpp"
#include "../../Domain/UserTypes.hpp"
#include "../../Util/Util.hpp"

ExportDeleteServlet::ExportDeleteServlet()
    : BlogAdminServlet("ExportDeleteServlet")
{
}

int32_t ExportDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {

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

        auto info = ExportRecordService::Get(id);
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
