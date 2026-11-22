#include "PasskeyListServlet.hpp"

#include <vector>

#include "../../Service/PasskeyService.hpp"

PasskeyListServlet::PasskeyListServlet()
    : BlogLoginedServlet("PasskeyListServlet")
{
}

int32_t PasskeyListServlet::handle(
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

        std::vector<DB::UserPasskeyInfo::ptr> passkeys;
        if (PasskeyService::ListByUser(passkeys, uid) < 0) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->setErrno(SUCCESS);
        result->set("total", passkeys.size());
        Json::Value list;
        for (const auto& passkey : passkeys) {
            if (!passkey) {
                continue;
            }
            Json::Value item;
            item["id"] = passkey->getId();
            item["name"] = passkey->getName();
            item["alg"] = passkey->getAlg();
            item["aaguid"] = passkey->getAaguid();
            item["transports"] = passkey->getTransports();
            item["backup_eligible"] = passkey->getBackupEligible();
            item["backup_state"] = passkey->getBackupState();
            item["created_at"] = passkey->getCreatedAt();
            item["last_used_at"] = passkey->getLastUsedAt();
            list.append(item);
        }
        result->set("list", list);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
