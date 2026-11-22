#include "DashboardTrendServlet.hpp"

#include "../../Service/DashboardService.hpp"
#include "../../Util/Util.hpp"

#include <ctime>
#include <json/json.h>

DashboardTrendServlet::DashboardTrendServlet()
    : BlogLoginedServlet("DashboardTrendServlet")
{
}

static std::string FormatBucketDate(int64_t ts)
{
    struct tm t;
    localtime_r(&ts, &t);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d-%02d", t.tm_mon + 1, t.tm_mday);
    return buf;
}

static const char* WeekdayName(int64_t ts)
{
    static const char* names[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
    struct tm t;
    localtime_r(&ts, &t);
    return names[t.tm_wday];
}

int32_t DashboardTrendServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        if (!viewer.is_platform_admin) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        int32_t period = request->getParamAs<int32_t>("period", 7);
        if (period != 7 && period != 30 && period != 90) {
            period = 7;
        }
        DashboardService::Trend trend;
        if (!DashboardService::GetTrend(period, trend)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        int64_t now = time(0);
        int64_t total_visits = 0;
        int64_t max_visits = 0;
        Json::Value list(Json::arrayValue);

        for (int32_t i = 0; i < trend.num_buckets; i++) {
            int64_t bucket_ts = trend.period_start + i * trend.bucket_sec;
            int64_t visits = trend.buckets[i];
            total_visits += visits;
            if (visits > max_visits) {
                max_visits = visits;
            }

            Json::Value item;
            if (period == 7) {
                item["label"] = WeekdayName(bucket_ts);
                item["date"] = FormatBucketDate(bucket_ts);
            } else if (period == 30) {
                item["label"] = FormatBucketDate(bucket_ts);
                item["date"] = FormatBucketDate(bucket_ts);
            } else {
                int64_t week_end = bucket_ts + trend.bucket_sec - 1;
                if (week_end > now) {
                    week_end = now;
                }
                item["label"] = "第" + std::to_string(i + 1) + "周";
                item["date"] = FormatBucketDate(bucket_ts) + " ~ " + FormatBucketDate(week_end);
            }
            item["visits"] = visits;
            list.append(item);
        }

        result->set("list", list);
        result->set("total_visits", total_visits);
        result->set("avg_visits", trend.num_buckets > 0 ? (double)total_visits / trend.num_buckets : 0);
        result->set("max_visits", max_visits);
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
