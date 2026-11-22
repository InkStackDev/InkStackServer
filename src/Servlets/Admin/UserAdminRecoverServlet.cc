#include "UserAdminRecoverServlet.hpp"

#include "../../Event/EventDefine.hpp"
#include "../../Service/UserService.hpp"
#include "../../Util/Util.hpp"

#include <set>
#include <utility>
#include <vector>

UserAdminRecoverServlet::UserAdminRecoverServlet()
    : BlogAdminServlet("UserAdminRecoverServlet")
{
}

int32_t UserAdminRecoverServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, ids, "ids");

        std::set<int64_t> unique_ids;
        auto tmp = chen::StringUtil::Split(ids, ',');
        for (auto& i : tmp) {
            unique_ids.insert(chen::TypeUtil::Atoi(i));
        }
        std::vector<int64_t> user_ids(unique_ids.begin(), unique_ids.end());

        std::vector<int64_t> recovered;
        if (!UserService::RecoverUsers(user_ids, recovered)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }
        if (!recovered.empty()) {
            Json::Value json_ids;
            for (auto id : recovered) {
                json_ids.append(id);

                EventUserAdminData data = {};
                data.type = "account_recovered";
                data.user_id = id;
                chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_USER_ADMIN, std::move(data));
            }
            result->set("ids", json_ids);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
