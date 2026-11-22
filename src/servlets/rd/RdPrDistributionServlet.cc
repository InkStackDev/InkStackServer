#include "RdPrDistributionServlet.hpp"

#include "../../Service/RdAnalyticsService.hpp"
#include "../../Util/Util.hpp"

RdPrDistributionServlet::RdPrDistributionServlet()
    : BlogLoginedServlet("RdPrDistributionServlet")
{
}

int32_t RdPrDistributionServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        std::string range = request->getParam("range");
        std::string granularity = request->getParam("granularity");
        int64_t org_id = request->getParamAs<int64_t>("org_id", 0);

        if (range.empty()) {
            result->setErrno(PARAM_MISSING, "range is required");
            break;
        }
        if (granularity.empty()) {
            result->setErrno(PARAM_MISSING, "granularity is required");
            break;
        }
        if (org_id < 0) {
            result->setErrno(PARAM_INVALID, "invalid org_id");
            break;
        }

        bool is_15d = (range == "15d");
        if (!is_15d && range != "3m") {
            result->setErrno(PARAM_INVALID, "range must be 15d or 3m");
            break;
        }
        if ((is_15d && granularity != "day") || (!is_15d && granularity != "week")) {
            result->setErrno(PARAM_INVALID, "granularity does not match range");
            break;
        }

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        RdAnalyticsService::PrDistributionQuery query;
        query.org_id = org_id;
        query.range = range;
        query.granularity = granularity;

        RdAnalyticsService::PrDistribution data;
        RdAnalyticsService::Result stats_result = RdAnalyticsService::GetPrDistribution(viewer, query, data);
        if (stats_result == RdAnalyticsService::Result::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (stats_result == RdAnalyticsService::Result::NoAccessibleOrg) {
            result->setErrno(ACCESS_DENIED, "no accessible organization");
            break;
        }
        if (stats_result == RdAnalyticsService::Result::InvalidParam) {
            result->setErrno(PARAM_INVALID);
            break;
        }
        if (stats_result != RdAnalyticsService::Result::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("range", range);
        result->set("granularity", granularity);

        Json::Value summary;
        summary["total"] = data.total;
        summary["merged"] = data.merged;
        summary["merge_rate"] = data.merge_rate;
        result->set("summary", summary);

        Json::Value points(Json::arrayValue);
        for (const auto& point : data.points) {
            Json::Value item;
            item["date"] = point.date;
            item["end_date"] = point.end_date;
            item["total"] = point.total;
            item["merged"] = point.merged;
            points.append(item);
        }
        result->set("points", points);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
