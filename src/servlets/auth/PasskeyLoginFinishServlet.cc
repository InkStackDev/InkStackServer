#include "PasskeyLoginFinishServlet.hpp"

#include <Chen/Util/JsonUtil.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../../Auth/AuthService.hpp"
#include "../../Auth/WebAuthn/WebAuthn.hpp"
#include "../../Service/PasskeyService.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Domain/UserTypes.hpp"

PasskeyLoginFinishServlet::PasskeyLoginFinishServlet()
    : BlogServlet("PasskeyLoginFinishServlet")
{
}

int32_t PasskeyLoginFinishServlet::handle(
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

        Json::Value credential;
        if (!chen::JsonUtil::FromString(credential, request->getBody()) || !credential.isObject()) {
            result->setErrno(PARAM_INVALID);
            break;
        }

        std::string credential_id;
        if (!WebAuthnService::ExtractCredentialId(credential, credential_id)) {
            result->setErrno(WEBAUTHN_INVALID_CREDENTIAL);
            break;
        }

        auto passkey = PasskeyService::GetByCredentialId(credential_id);
        if (!passkey) {
            result->setErrno(PASSKEY_NOT_FOUND);
            break;
        }

        auto user = UserRepository::Get(passkey->getUserId(), GetDB());
        if (!user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        if (user->getState() != UserState::ACTIVE || user->getIsDeleted()) {
            result->setErrno(ACCOUNT_INVALID);
            break;
        }

        AuthenticationResult auth_result;
        int32_t ret = WebAuthnService::VerifyAuthentication(
            passkey->getUserId(), passkey->getPublicKey(), passkey->getSignCount(), credential, auth_result);
        if (ret != 0) {
            result->setErrno(ret);
            break;
        }
        if (auth_result.credential_id != passkey->getCredentialId()) {
            result->setErrno(WEBAUTHN_INVALID_CREDENTIAL);
            break;
        }

        passkey->setSignCount(auth_result.sign_count);
        passkey->setLastUsedAt(chen::TimeUtil::GetCurrentSec());
        passkey->setBackupState(auth_result.backup_state);
        if (PasskeyService::Update(passkey) != 0) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        // Passkey 本身已具备持有因子 + 本地用户验证，登录时不再叠加 TOTP。
        IssueOptions options;
        ret = AuthService::IssueSession(request, response, session, user, options);
        if (ret != 0) {
            result->setErrno(ret);
            break;
        }

        result->set("id", user->getId());
        result->set("account", user->getAccount());
        result->set("name", user->getName());
        result->set("avatar", user->getAvatar());
        result->set("role", user->getRole());
        result->set("user_verified", auth_result.user_verified);
        result->setErrno(SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
