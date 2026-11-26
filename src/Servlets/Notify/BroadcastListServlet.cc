#include "BroadcastListServlet.hpp"

#include "../../Service/NotificationService.hpp"
#include "../../Util/Util.hpp"

#include <json/json.h>

#include <Chen/Util/JsonUtil.hpp>

BroadcastListServlet::BroadcastListServlet()
    : BlogServlet("BroadcastListServlet")
{
}

int32_t BroadcastListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t offset = request->getParamAs<int64_t>("offset", 0);
        int64_t size = request->getParamAs<int64_t>("size", 20);
        if (size > 100) {
            size = 100;
        }

        std::vector<DB::NotificationInfo::ptr> notifs;
        if (!NotificationService::ListBroadcasts(notifs, offset, size)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value list(Json::arrayValue);
        for (const auto& info : notifs) {
            if (!info) {
                continue;
            }
            Json::Value item;
            item["id"] = info->getId();
            item["title"] = info->getTitle();
            item["content"] = info->getContent();
            item["title_id"] = info->getTitleId();
            item["content_id"] = info->getContentId();
            Json::Value params(Json::objectValue);
            if (!info->getParams().empty()) {
                chen::JsonUtil::FromString(params, info->getParams());
            }
            item["params"] = params;
            item["type"] = info->getType();
            item["level"] = info->getLevel();
            item["start_time"] = info->getStartTime();
            item["end_time"] = info->getEndTime();
            item["create_time"] = info->getCreateTime();
            list.append(item);
        }
        result->set("list", list);
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
