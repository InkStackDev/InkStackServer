#include "DashboardActivitiesServlet.hpp"

#include "../../I18n/MessageCatalog.hpp"
#include "../../Service/DashboardService.hpp"
#include "../../Util/Util.hpp"

#include <ctime>
#include <map>

#include <json/json.h>

DashboardActivitiesServlet::DashboardActivitiesServlet()
    : BlogLoginedServlet("DashboardActivitiesServlet")
{
}

struct TimeAgoResult {
    std::string id;
    int32_t count = -1;
};

static TimeAgoResult TimeAgo(int64_t now, int64_t ts)
{
    int64_t diff = now - ts;
    if (diff < 0) {
        diff = 0;
    }
    if (diff < 60) {
        return {"dashboard.time.just_now", -1};
    }
    if (diff < 3600) {
        return {"dashboard.time.minutes_ago", static_cast<int32_t>(diff / 60)};
    }
    if (diff < 86400) {
        return {"dashboard.time.hours_ago", static_cast<int32_t>(diff / 3600)};
    }
    return {"dashboard.time.days_ago", static_cast<int32_t>(diff / 86400)};
}

static Json::Value ParamsToJson(const std::map<std::string, std::string>& params)
{
    Json::Value obj(Json::objectValue);
    for (const auto& [key, value] : params) {
        obj[key] = value;
    }
    return obj;
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

        const std::string default_lang = MessageCatalogRegistry::GetInstance()->DefaultLanguage();
        int64_t now = time(0);
        Json::Value list(Json::arrayValue);
        for (const auto& activity : activities) {
            Json::Value item;
            item["type"] = activity.type;
            item["title"] = activity.title;
            item["title_id"] = activity.title_id;
            item["params"] = ParamsToJson(activity.params);
            item["timestamp"] = activity.create_time;

            TimeAgoResult ago = TimeAgo(now, activity.create_time);
            std::map<std::string, std::string> time_params;
            if (ago.count >= 0) {
                time_params["count"] = std::to_string(ago.count);
            }
            item["time_id"] = ago.id;
            item["time_params"] = ParamsToJson(time_params);
            item["time"] = MessageCatalogRegistry::GetInstance()->render(default_lang, ago.id, time_params);
            list.append(item);
        }

        result->set("list", list);
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
