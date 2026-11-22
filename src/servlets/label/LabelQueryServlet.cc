#include "LabelQueryServlet.hpp"

#include "../../Service/LabelService.hpp"
#include "../../Util/Util.hpp"

LabelQueryServlet::LabelQueryServlet()
    : BlogLoginedServlet("LabelQueryServlet")
{
}

int32_t LabelQueryServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t user_id = request->getParamAs<int64_t>("user_id");
        std::string ids_str = request->getParam("ids");
        if (user_id == 0 && ids_str.empty()) {
            result->setErrno(PARAM_MISSING);
            break;
        }

        std::vector<int64_t> ids;
        for (auto& id : chen::StringUtil::Split(ids_str, ',')) {
            if (!id.empty()) {
                ids.push_back(chen::TypeUtil::Atoi(id));
            }
        }

        std::vector<LabelService::LabelView> labels;
        if (!LabelService::List(user_id, ids, labels)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value data(Json::arrayValue);
        for (const auto& label : labels) {
            Json::Value v;
            v["id"] = label.id;
            v["name"] = label.name;
            v["color"] = label.color;
            v["desc"] = label.description;
            v["create_time"] = label.create_time;
            v["article_count"] = label.article_count;
            data.append(v);
        }
        result->setData(data);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
