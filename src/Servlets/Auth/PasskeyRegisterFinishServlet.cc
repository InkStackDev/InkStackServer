#include "PasskeyRegisterFinishServlet.hpp"

#include <Chen/Util/JsonUtil.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../../Auth/WebAuthn/WebAuthn.hpp"
#include "../../Service/PasskeyService.hpp"

PasskeyRegisterFinishServlet::PasskeyRegisterFinishServlet()
    : BlogLoginedServlet("PasskeyRegisterFinishServlet")
{
}

int32_t PasskeyRegisterFinishServlet::handle(
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

        Json::Value credential;
        if (!chen::JsonUtil::FromString(credential, request->getBody()) || !credential.isObject()) {
            result->setErrno(PARAM_INVALID);
            break;
        }

        RegistrationResult registration;
        std::string passkey_name;
        int32_t ret = WebAuthnService::VerifyRegistration(uid, credential, registration, passkey_name);
        if (ret != 0) {
            result->setErrno(ret);
            break;
        }

        std::string body_name;
        if (credential.isMember("name") && credential["name"].isString()) {
            body_name = credential["name"].asString();
        }
        if (!body_name.empty()) {
            passkey_name = body_name;
        }
        if (passkey_name.empty()) {
            passkey_name = "Passkey";
        }
        if (passkey_name.size() > 64) {
            result->setErrno(PARAM_INVALID, "passkey name too long");
            break;
        }

        if (PasskeyService::GetByCredentialId(registration.credential_id)) {
            result->setErrno(PASSKEY_ALREADY_EXISTS);
            break;
        }

        auto info = std::make_shared<DB::UserPasskeyInfo>();
        info->setUserId(uid);
        info->setCredentialId(registration.credential_id);
        info->setPublicKey(registration.public_key);
        info->setAlg(registration.alg);
        info->setSignCount(registration.sign_count);
        info->setTransports(registration.transports);
        info->setAaguid(registration.aaguid);
        info->setName(passkey_name);
        info->setBackupEligible(registration.backup_eligible);
        info->setBackupState(registration.backup_state);
        info->setIsDeleted(0);
        info->setCreatedAt(chen::TimeUtil::GetCurrentSec());
        info->setLastUsedAt(chen::TimeUtil::GetCurrentSec());

        if (PasskeyService::Add(info) != 0) {
            result->setErrno(PASSKEY_REGISTER_FAILED);
            break;
        }

        result->set("id", info->getId());
        result->set("name", info->getName());
        result->set("credential_id", info->getCredentialId());
        result->setErrno(SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
