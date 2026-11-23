#include "UserAchievementsServlet.hpp"

#include "../../Service/UserStatsService.hpp"
#include "../../Util/Util.hpp"

#include <Chen/Log/Log.hpp>

#include <json/json.h>

static chen::Logger::ptr logger = LOG_ROOT();

UserAchievementsServlet::UserAchievementsServlet()
    : BlogLoginedServlet("UserAchievementsServlet")
{
}

int32_t UserAchievementsServlet::handle(
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

        UserStatsService::Achievements data;
        if (!UserStatsService::GetAchievements(uid, data)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value heatmap(Json::arrayValue);
        for (const auto& week : data.heatmap) {
            Json::Value week_json(Json::arrayValue);
            for (auto level : week) {
                week_json.append(level);
            }
            heatmap.append(week_json);
        }

        Json::Value badges_json(Json::arrayValue);
        for (const auto& badge : data.badges) {
            Json::Value item;
            item["id"] = std::to_string(badge.id);
            item["name"] = badge.name;
            item["desc"] = badge.description;
            item["icon"] = badge.icon;
            item["color"] = badge.color;
            item["unlocked"] = badge.unlocked;
            if (!badge.unlocked) {
                item["progress"] = std::to_string(badge.current) + " / " + std::to_string(badge.target);
            }
            badges_json.append(item);
        }

        Json::Value stats;
        stats["total_contributions"] = data.total_contributions;
        stats["published_articles"] = data.published_articles;
        stats["total_likes"] = data.total_likes;
        stats["unlocked_badges"] = data.unlocked_badges;
        stats["total_badges"] = data.total_badges;

        result->set("stats", stats);
        result->set("badges", badges_json);
        result->set("heatmap", heatmap);
        result->setErrno(SUCCESS);
    } while (0);

    DEBUG(logger) << "UserAchievementsServlet handle result: " << result->toJsonString();
    response->setBody(result->toJsonString());
    return 0;
}
