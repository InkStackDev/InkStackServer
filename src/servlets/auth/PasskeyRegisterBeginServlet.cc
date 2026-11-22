#include "PasskeyRegisterBeginServlet.hpp"

#include "../../Auth/WebAuthn/WebAuthn.hpp"
#include "../../Util/UaParser.hpp"
#include "../../Service/PasskeyService.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Util/Util.hpp"

PasskeyRegisterBeginServlet::PasskeyRegisterBeginServlet()
    : BlogLoginedServlet("PasskeyRegisterBeginServlet")
{
}

int32_t PasskeyRegisterBeginServlet::handle(
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

        auto user = UserRepository::Get(uid, GetDB());
        if (!user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }

        std::string passkey_name = request->getParam("name");
        if (passkey_name.empty()) {
            passkey_name = ParseDeviceName(request->getHeader("User-Agent"));
        }
        if (passkey_name.empty()) {
            passkey_name = "Passkey";
        }
        if (passkey_name.size() > 64) {
            result->setErrno(PARAM_INVALID, "passkey name too long");
            break;
        }

        std::vector<DB::UserPasskeyInfo::ptr> passkeys;
        if (PasskeyService::ListByUser(passkeys, uid) < 0) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        std::vector<std::string> exclude_ids;
        exclude_ids.reserve(passkeys.size());
        for (const auto& passkey : passkeys) {
            if (passkey) {
                exclude_ids.push_back(passkey->getCredentialId());
            }
        }

        Json::Value options;
        int32_t ret = WebAuthnService::CreateRegistrationOptions(
            uid, user->getAccount(), user->getName(), passkey_name, exclude_ids, options);
        if (ret != 0) {
            result->setErrno(ret);
            break;
        }

        result->setData(options);
        result->set("name", passkey_name);
        result->setErrno(SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
