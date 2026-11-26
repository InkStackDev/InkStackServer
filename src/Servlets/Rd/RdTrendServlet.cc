#include "RdTrendServlet.hpp"

#include "../../Service/RdAnalyticsService.hpp"
#include "../../Util/Util.hpp"

#include <algorithm>
#include <cmath>

RdTrendServlet::RdTrendServlet()
    : BlogLoginedServlet("RdTrendServlet")
{
}

int32_t RdTrendServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        RdAnalyticsService::TrendQuery query;
        query.org_id = request->getParamAs<int64_t>("org_id", 0);
        query.period_days = request->getParamAs<int64_t>("period_days", 30);
        query.start_date = request->getParam("start_date");
        query.end_date = request->getParam("end_date");
        query.granularity = request->getParam("granularity", "day");

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        RdAnalyticsService::TrendResult data;
        RdAnalyticsService::Result stats_result = RdAnalyticsService::GetTrend(viewer, query, data);
        if (stats_result == RdAnalyticsService::Result::InvalidRange) {
            result->setErrno(PARAM_INVALID);
            break;
        }
        if (stats_result == RdAnalyticsService::Result::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (stats_result == RdAnalyticsService::Result::NoAccessibleOrg) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (stats_result != RdAnalyticsService::Result::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value summary;
        summary["completed_total"] = data.completed_total;
        summary["bug_total"] = data.bug_total;
        summary["avg_review_pass_rate"] = data.review_pass_rate;
        summary["avg_cycle_time"] = std::round(data.avg_cycle_time * 10.0) / 10.0;
        summary["task_delta"] = data.task_delta;
        summary["cycle_delta"] = std::round(data.cycle_delta * 10.0) / 10.0;
        result->set("summary", summary);

        Json::Value series(Json::arrayValue);
        for (const auto& point : data.series) {
            Json::Value item;
            item["date"] = point.date;
            item["requirements"] = point.requirements;
            item["bugs"] = point.bugs;
            item["tasks"] = point.tasks;
            item["completed"] = point.completed;
            item["reopened"] = point.reopened;
            item["review_pass_rate"] = point.review_pass_rate;
            item["cycle_time"] = point.cycle_time;
            series.append(item);
        }
        result->set("series", series);

        int64_t total_items = data.new_requirements + data.bug_total + data.new_tasks;
        Json::Value dist;
        int64_t dist_total = total_items + data.pr_count;
        if (dist_total > 0) {
            dist["requirement_delivery"] = std::round(data.new_requirements * 100.0 / dist_total);
            dist["bug_fix"] = std::round(data.bug_total * 100.0 / dist_total);
            dist["rd_task"] = std::round(data.new_tasks * 100.0 / dist_total);
            dist["code_review"] = std::round(data.pr_count * 100.0 / dist_total);
        } else {
            dist["requirement_delivery"] = 0;
            dist["bug_fix"] = 0;
            dist["rd_task"] = 0;
            dist["code_review"] = 0;
        }
        result->set("work_distribution", dist);

        Json::Value health;
        health["throughput"] = std::min((int64_t)100, data.completed_total);
        if (total_items > 0) {
            health["bug_pressure"] = 100 - std::min(data.bug_total * 100 / total_items, (int64_t)80);
        } else {
            health["bug_pressure"] = 100;
        }
        health["review_efficiency"] = data.review_pass_rate;
        health["rework_risk"] = 0;
        result->set("team_health", health);

        result->set("insights", Json::Value(Json::arrayValue));
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
