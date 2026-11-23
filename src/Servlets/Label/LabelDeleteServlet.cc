#include "LabelDeleteServlet.hpp"

#include <json/json.h>

#include <set>

#include "../../Service/LabelService.hpp"
#include "../../Util/Util.hpp"

LabelDeleteServlet::LabelDeleteServlet()
    : BlogLoginedServlet("LabelDeleteServlet")
{
}

int32_t LabelDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, ids, "ids");
        std::set<int64_t> label_ids;
        for (auto& i : chen::StringUtil::Split(ids, ",")) {
            if (!i.empty()) {
                label_ids.insert(chen::TypeUtil::Atoi(i));
            }
        }

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        std::vector<DB::LabelInfo::ptr> deleted;
        if (!LabelService::DeleteMany(uid, label_ids, deleted)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }
        if (!deleted.empty()) {
            Json::Value json_ids;
            for (auto& info : deleted) {
                json_ids.append(info->getId());
            }
            result->set("ids", json_ids);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
