#include "FaqsServlet.hpp"

#include <json/json.h>

#include <Chen/Log/Log.hpp>
#include "../../Service/FaqService.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

FaqsServlet::FaqsServlet()
    : BlogServlet("FaqsServlet")
{
}

int32_t FaqsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        std::string keyword = request->getParam("keyword");

        std::vector<DB::HelpFaqsInfo::ptr> infos;
        if (keyword.empty()) {
            FaqService::ListAll(infos);
        } else {
            FaqService::Search(keyword, infos);
        }

        Json::Value data;
        for (auto& i : infos) {
            Json::Value v;
            v["id"] = i->getId();
            v["cat"] = i->getCat();
            v["q"] = i->getQ();
            v["a"] = i->getA();
            data.append(v);
        }
        result->setData(data);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
