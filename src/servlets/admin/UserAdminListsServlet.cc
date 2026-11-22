#include "UserAdminListsServlet.hpp"

#include "../../Service/AdminUserService.hpp"
#include "../../Util/Util.hpp"

UserAdminListsServlet::UserAdminListsServlet()
    : BlogAdminServlet("UserAdminListsServlet")
{
}

int32_t UserAdminListsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");
        int32_t role = request->getParamAs<int32_t>("role", -1);
        int32_t state = request->getParamAs<int32_t>("state", -1);
        int32_t days = request->getParamAs<int32_t>("days", 0);

        uint64_t offset = (page_num - 1) * page_size;
        std::vector<AdminUserService::ListItem> users;
        uint64_t total = 0;
        if (!AdminUserService::List(offset, page_size, role, state, days, users, total)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total", total);
        Json::Value list;
        for (const auto& user : users) {
            Json::Value item;
            item["id"] = user.id;
            item["name"] = user.name;
            item["email"] = user.email;
            item["avatar"] = user.avatar;
            item["role"] = user.role;
            item["state"] = user.state;
            item["create_time"] = user.create_time;
            item["login_time"] = user.login_time;
            item["article_count"] = user.article_count;
            item["comment_count"] = user.comment_count;
            list.append(item);
        }
        result->set("list", list);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
