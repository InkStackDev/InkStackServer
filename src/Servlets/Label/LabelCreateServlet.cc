#include "LabelCreateServlet.hpp"

#include "../../Service/LabelService.hpp"
#include "../../Util/Util.hpp"

LabelCreateServlet::LabelCreateServlet()
    : BlogLoginedServlet("LabelCreateServlet")
{
}

int32_t LabelCreateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, name, "name");
        DEFINE_AND_CHECK_STRING(result, color, "color");
        std::string desc = request->getParamAs<std::string>("desc");
        int64_t lid = request->getParamAs<int64_t>("id", 0);

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        DB::LabelInfo::ptr info;
        auto rt = LabelService::CreateOrUpdate(uid, lid, name, color, desc, info);
        if (rt == LabelService::Result::NotFound) {
            result->setErrno(PARAM_INVALID, "invalid lid");
            break;
        }
        if (rt == LabelService::Result::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt != LabelService::Result::Ok) {
            result->setErrno(DB_OPERATION_FAILED, "label operation failed");
            break;
        }

        result->set("id", info->getId());
        result->set("name", info->getName());
        result->set("color", info->getColor());
        result->set("desc", info->getDescription());
        result->set("create_time", info->getCreateTime());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
