#include "MetricsServlet.hpp"

#include "../../Metrics/MetricsService.hpp"

MetricsAdminServlet::MetricsAdminServlet()
    : BlogAdminServlet("MetricsAdminServlet")
{
}

int32_t MetricsAdminServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        Json::Value data;
        if (!CollectClusterMetrics(data)) {
            result->setErrno(INTERNAL_ERROR);
            break;
        }

        result->setData(data);
        result->setErrno(SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
