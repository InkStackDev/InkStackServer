#include "PasskeyLoginBeginServlet.hpp"

#include <vector>

#include "../../Auth/AuthService.hpp"
#include "../../Auth/WebAuthn/WebAuthn.hpp"
#include "../../Util/Util.hpp"
#include "../../Service/PasskeyService.hpp"
#include "../../Service/UserService.hpp"
#include "../../Domain/UserTypes.hpp"

PasskeyLoginBeginServlet::PasskeyLoginBeginServlet()
    : BlogServlet("PasskeyLoginBeginServlet")
{
}

int32_t PasskeyLoginBeginServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        AuthContext ctx = AuthService::Resolve(request, response, false);
        if (ctx.valid()) {
            result->setErrno(USER_ALREADY_LOGIN);
            break;
        }

        int64_t uid = 0;
        std::vector<std::string> credential_ids;
        std::string auth_id = request->getParam("auth_id");
        if (!auth_id.empty()) {
            DB::UserInfo::ptr user;
            if (IsEmail(auth_id)) {
                user = UserService::GetByEmail(auth_id);
            } else if (IsValidAccount(auth_id)) {
                user = UserService::GetByAccount(auth_id);
            } else {
                result->setErrno(USER_INVALID_ACCOUNT);
                break;
            }

            if (!user) {
                result->setErrno(USER_NOT_FOUND);
                break;
            }
            if (user->getState() != UserState::ACTIVE || user->getIsDeleted()) {
                result->setErrno(ACCOUNT_INVALID);
                break;
            }

            std::vector<DB::UserPasskeyInfo::ptr> passkeys;
            if (PasskeyService::ListByUser(passkeys, user->getId()) < 0) {
                result->setErrno(DB_OPERATION_FAILED);
                break;
            }
            if (passkeys.empty()) {
                result->setErrno(PASSKEY_NOT_FOUND);
                break;
            }

            uid = user->getId();
            for (const auto& passkey : passkeys) {
                if (passkey && !passkey->getCredentialId().empty()) {
                    credential_ids.push_back(passkey->getCredentialId());
                }
            }
            if (credential_ids.empty()) {
                result->setErrno(PASSKEY_NOT_FOUND);
                break;
            }
        }

        Json::Value options;
        int32_t ret = WebAuthnService::CreateAuthenticationOptions(uid, credential_ids, options);
        if (ret != 0) {
            result->setErrno(ret);
            break;
        }

        result->setData(options);
        result->setErrno(SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
