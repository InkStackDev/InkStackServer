#include "LabelQueryServlet.hpp"

#include "../../Include/Managers.hpp"
#include "../../Repository/ArticleRelRepository.hpp"
#include "../../Util/Util.hpp"

#include <json/json.h>

#include <map>

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
        std::string ids = request->getParam("ids");
        if (user_id == 0 && ids.empty()) {
            result->setErrno(PARAM_MISSING);
            break;
        }

        std::vector<DB::LabelInfo::ptr> infos;
        if (user_id) {
            LabelMgr::GetInstance()->listByUserId(infos, user_id, true);
        } else {
            for (auto& i : chen::StringUtil::Split(ids, ",")) {
                auto id = chen::TypeUtil::Atoi(i);
                if (id) {
                    auto info = LabelMgr::GetInstance()->get(id);
                    if (info) {
                        infos.push_back(info);
                    }
                }
            }
        }

        std::map<int64_t, int64_t> article_counts;
        if (!infos.empty()) {
            std::vector<int64_t> label_ids;
            label_ids.reserve(infos.size());
            for (auto& i : infos) {
                label_ids.push_back(i->getId());
            }

            std::vector<std::pair<int64_t, int64_t>> rows;
            ArticleLabelRelRepository::CountPublishedByLabels(label_ids, rows, GetDB());
            for (auto& [label_id, count] : rows) {
                article_counts[label_id] = count;
            }
        }

        Json::Value data(Json::arrayValue);
        for (auto& i : infos) {
            Json::Value v;
            v["id"] = i->getId();
            v["name"] = i->getName();
            v["color"] = i->getColor();
            v["desc"] = i->getDescription();
            v["create_time"] = i->getCreateTime();
            v["article_count"] = article_counts[i->getId()];
            data.append(v);
        }
        result->setData(data);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
