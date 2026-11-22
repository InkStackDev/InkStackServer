#include "UserQueryServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Util/Util.hpp"
#include "DB/UserInfo.hpp"
#include "../../Repository/UserRepository.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

UserQueryServlet::UserQueryServlet()
    : BlogServlet("UserQueryServlet")
{
}

int32_t UserQueryServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, user_ids, "user_ids");

        auto ids = chen::StringUtil::Split(user_ids, ',');
        std::vector<DB::UserInfo::ptr> infos;

        for (auto& i : ids) {
            DB::UserInfo::ptr info = UserRepository::Get(chen::TypeUtil::Atoi(i), GetDB());
            if (info) {
                infos.push_back(info);
            }
        }

        Json::Value data;
        for (auto& i : infos) {
            Json::Value v;
            v["id"] = i->getId();
            v["name"] = i->getName();
            data.append(v);
        }
        result->set("data", data);
        INFO(logger) << "infos.size=" << infos.size() << " - " << chen::JsonUtil::ToString(data);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
