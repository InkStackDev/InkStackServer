#include "LabelCreateServlet.hpp"

#include "../../Manager/LabelManager.hpp"
#include "../../Util/Util.hpp"

#include <Chen/Log/Log.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

LabelCreateServlet::LabelCreateServlet()
    : BlogLoginedServlet("LabelCreateServlet")
{
}

int32_t LabelCreateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, name, "name");
        DEFINE_AND_CHECK_STRING(result, color, "color");
        std::string desc = request->getParamAs<std::string>("desc");
        int64_t lid = request->getParamAs<int64_t>("id", 0);

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        DB::LabelInfo::ptr info;
        if (lid) {
            info = LabelMgr::GetInstance()->get(lid);
            if (!info) {
                result->setErrno(PARAM_INVALID, "invalid lid");
                break;
            }
            if (info->getUserId() != uid) {
                result->setErrno(ACCESS_DENIED);
                break;
            }
            info->setName(name);
        } else {
            info = LabelMgr::GetInstance()->getByUserIdName(uid, name);
            if (!info) {
                info.reset(new DB::LabelInfo);
                info->setUserId(uid);
                info->setName(name);
                info->setCreateTime(time(0));
            } else if (info->getIsDeleted()) {
                info->setIsDeleted(0);
            }
        }
        info->setDescription(desc);
        info->setColor(color);
        info->setIsDeleted(0);
        info->setUpdateTime(time(0));

        auto db = GetDB();
        if (!db) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        if (DB::LabelInfoDao::InsertOrUpdate(info, db)) {
            result->setErrno(DB_OPERATION_FAILED, "label operation failed");
            ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }

        LabelMgr::GetInstance()->add(info);

        result->set("id", info->getId());
        result->set("name", info->getName());
        result->set("color", info->getColor());
        result->set("desc", info->getDescription());
        result->set("create_time", info->getCreateTime());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
