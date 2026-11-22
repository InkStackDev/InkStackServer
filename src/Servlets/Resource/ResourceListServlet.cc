#include "ResourceListServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

ResourceListServlet::ResourceListServlet()
    : BlogLoginedServlet("ResourceListServlet")
{
}

int32_t ResourceListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, type, "type");
        DEFINE_AND_CHECK_STRING(result, folder, "folder");

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        if (!viewer.is_platform_admin) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        // TODO: 查询资源列表
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
