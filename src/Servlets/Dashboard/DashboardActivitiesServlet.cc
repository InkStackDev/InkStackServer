#include "DashboardActivitiesServlet.hpp"

#include "../../Service/DashboardService.hpp"
#include "../../Util/Util.hpp"

#include <ctime>
#include <json/json.h>

DashboardActivitiesServlet::DashboardActivitiesServlet()
    : BlogLoginedServlet("DashboardActivitiesServlet")
{
}

static std::string TimeAgo(int64_t now, int64_t ts)
{
    int64_t diff = now - ts;
    if (diff < 0) {
        diff = 0;
    }
    if (diff < 60) {
        return "just now";
    }
    if (diff < 3600) {
        return std::to_string(diff / 60) + " minutes ago";
    }
    if (diff < 86400) {
        return std::to_string(diff / 3600) + " hours ago";
    }
    return std::to_string(diff / 86400) + " days ago";
}

int32_t DashboardActivitiesServlet::handle(
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

        int32_t limit = request->getParamAs<int32_t>("limit", 5);
        std::vector<DashboardService::Activity> activities;
        if (!DashboardService::GetActivities(limit, activities)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        int64_t now = time(0);
        Json::Value list(Json::arrayValue);
        for (const auto& activity : activities) {
            Json::Value item;
            item["type"] = activity.type;
            item["title"] = activity.title;
            item["timestamp"] = activity.create_time;
            item["time"] = TimeAgo(now, activity.create_time);
            list.append(item);
        }

        result->set("list", list);
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
