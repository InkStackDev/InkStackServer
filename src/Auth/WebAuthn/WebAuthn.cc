#include "WebAuthn.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/DB/Redis.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/EncryptorUtil.hpp>
#include <Chen/Util/JsonUtil.hpp>
#include <Chen/Util/StringUtil.hpp>
#include <Chen/Util/TypeUtil.hpp>

#include "../../ErrorCodes.hpp"
#include "Cbor.hpp"
#include "Cose.hpp"
#include "../../Config/AppConfig.hpp"
#include "../../Config/AuthConfig.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

constexpr const char* kRegistrationChallengePrefix = "webauthn:reg:";
constexpr const char* kAuthenticationChallengePrefix = "webauthn:auth:";

constexpr uint8_t kFlagUp = 0x01;
constexpr uint8_t kFlagUv = 0x04;
constexpr uint8_t kFlagBe = 0x08;
constexpr uint8_t kFlagBs = 0x10;
constexpr uint8_t kFlagAt = 0x40;
constexpr size_t kMaxCredentialIdB64 = 512;
constexpr const char* kTrimChars = "/ \t\r\n";

struct WebAuthnClientData {
    std::string type;
    std::string challenge;
    std::string origin;
    bool cross_origin = false;
};

struct WebAuthnAuthData {
    std::string rp_id_hash;
    uint8_t flags = 0;
    uint32_t sign_count = 0;
    std::string aaguid;
    std::string credential_id;
    std::string cose_key;
};

static std::string ChallengeKey(const std::string& prefix, int64_t uid, const std::string& challenge)
{
    std::string scope = uid ? chen::TypeUtil::ToString(uid) : "anon";
    return prefix + scope + ":" + challenge;
}

static bool StoreChallenge(const std::string& prefix,
    int64_t uid,
    const std::string& challenge,
    const std::string& value)
{
    const WebAuthnConf conf = GetWebAuthnConf();

    int64_t ttl = conf.challenge_ttl;
    if (ttl <= 0) {
        ttl = 300;
    }
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "setex %s %lld %s",
        ChallengeKey(prefix, uid, challenge).c_str(), (long long)ttl, value.c_str());
    return rpy != nullptr;
}

static bool ConsumeChallenge(const std::string& prefix, int64_t uid, const std::string& challenge, std::string& value)
{
    value.clear();
    if (challenge.empty()) {
        return false;
    }

    std::string key = ChallengeKey(prefix, uid, challenge);
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get %s", key.c_str());
    bool found = rpy && rpy->type == REDIS_REPLY_STRING && rpy->str;
    if (found) {
        value.assign(rpy->str, rpy->len);
    }
    chen::RedisUtil::Cmd(GetRedisPoolName(), "del %s", key.c_str());
    return found;
}

static bool ParseClientData(const std::string& json, WebAuthnClientData& out)
{
    Json::Value root;
    if (!chen::JsonUtil::FromString(root, json) || !root.isObject()) {
        return false;
    }
    if (!root.isMember("type") || !root["type"].isString() || !root.isMember("challenge") || !root["challenge"].isString() || !root.isMember("origin")
        || !root["origin"].isString()) {
        return false;
    }

    out.type = root["type"].asString();
    out.challenge = root["challenge"].asString();
    out.origin = root["origin"].asString();
    if (root.isMember("crossOrigin") && root["crossOrigin"].isBool()) {
        out.cross_origin = root["crossOrigin"].asBool();
    }
    return true;
}

static bool ParseAuthDataHeader(const std::string& data, WebAuthnAuthData& out)
{
    if (data.size() < 37) {
        return false;
    }

    out.rp_id_hash = data.substr(0, 32);
    out.flags = static_cast<uint8_t>(data[32]);
    out.sign_count = (static_cast<uint32_t>(static_cast<uint8_t>(data[33])) << 24) | (static_cast<uint32_t>(static_cast<uint8_t>(data[34])) << 16)
        | (static_cast<uint32_t>(static_cast<uint8_t>(data[35])) << 8) | static_cast<uint32_t>(static_cast<uint8_t>(data[36]));
    return true;
}

static bool ParseRegistrationData(const std::string& data, WebAuthnAuthData& out)
{
    if (!ParseAuthDataHeader(data, out) || !(out.flags & kFlagAt)) {
        return false;
    }

    size_t pos = 37;
    if (pos + 18 > data.size()) {
        return false;
    }
    out.aaguid = data.substr(pos, 16);
    pos += 16;

    uint16_t credential_len =
        (static_cast<uint16_t>(static_cast<uint8_t>(data[pos])) << 8) | static_cast<uint16_t>(static_cast<uint8_t>(data[pos + 1]));
    pos += 2;
    if (credential_len == 0 || pos + credential_len > data.size()) {
        return false;
    }
    out.credential_id = data.substr(pos, credential_len);
    pos += credential_len;

    std::string remaining = data.substr(pos);
    CborValue cose;
    size_t consumed = 0;
    if (!DecodeCbor(remaining, cose, consumed) || cose.type != CborType::Map || consumed == 0) {
        return false;
    }
    out.cose_key = remaining.substr(0, consumed);
    return true;
}

bool WebAuthnService::IsConfigured()
{
    const WebAuthnConf conf = GetWebAuthnConf();
    return !chen::StringUtil::Trim(conf.rp_id, kTrimChars).empty() && !chen::StringUtil::Trim(conf.origin, kTrimChars).empty();
}

std::string WebAuthnService::RpId()
{
    const WebAuthnConf conf = GetWebAuthnConf();
    return chen::StringUtil::Trim(conf.rp_id, kTrimChars);
}

std::string WebAuthnService::RpName()
{
    const WebAuthnConf conf = GetWebAuthnConf();
    return conf.rp_name.empty() ? "InkStack" : conf.rp_name;
}

std::string WebAuthnService::Origin()
{
    const WebAuthnConf conf = GetWebAuthnConf();
    return chen::StringUtil::Trim(conf.origin, kTrimChars);
}

std::string WebAuthnService::UserHandle(int64_t uid)
{
    if (!uid) {
        return "";
    }

    const WebAuthnConf conf = GetWebAuthnConf();
    std::string secret = conf.user_handle_secret;
    if (secret.empty()) {
        secret = "inkstack-webauthn-default";
    }
    std::string raw = chen::EncryptorUtil::HMAC_SHA256(chen::TypeUtil::ToString(uid), secret);
    if (raw.size() < 16) {
        return "";
    }
    return chen::StringUtil::Base64UrlEncode(raw.substr(0, 16));
}

int32_t WebAuthnService::CreateRegistrationOptions(
    int64_t uid,
    const std::string& account,
    const std::string& display_name,
    const std::string& passkey_name,
    const std::vector<std::string>& exclude_credential_ids,
    Json::Value& options)
{
    const WebAuthnConf conf = GetWebAuthnConf();

    if (!IsConfigured()) {
        return WEBAUTHN_NOT_CONFIGURED;
    }

    std::string challenge;
    try {
        challenge = chen::StringUtil::Base64UrlEncode(chen::EncryptorUtil::GenerateRandomKey(32));
    } catch (...) {
        return INTERNAL_ERROR;
    }
    if (challenge.empty()) {
        return INTERNAL_ERROR;
    }

    std::string user_handle = UserHandle(uid);
    if (user_handle.empty()) {
        return INTERNAL_ERROR;
    }

    std::string name = passkey_name.empty() ? "Passkey" : passkey_name;
    if (!StoreChallenge(kRegistrationChallengePrefix, uid, challenge, name)) {
        return REDIS_OPERATION_FAILED;
    }

    options = Json::Value(Json::objectValue);
    options["challenge"] = challenge;

    Json::Value rp(Json::objectValue);
    rp["id"] = RpId();
    rp["name"] = RpName();
    options["rp"] = rp;

    Json::Value user(Json::objectValue);
    user["id"] = user_handle;
    user["name"] = account;
    user["displayName"] = display_name.empty() ? account : display_name;
    options["user"] = user;

    Json::Value params(Json::arrayValue);
    Json::Value es256(Json::objectValue);
    es256["type"] = "public-key";
    es256["alg"] = -7;
    params.append(es256);
    Json::Value rs256(Json::objectValue);
    rs256["type"] = "public-key";
    rs256["alg"] = -257;
    params.append(rs256);
    Json::Value eddsa(Json::objectValue);
    eddsa["type"] = "public-key";
    eddsa["alg"] = -8;
    params.append(eddsa);
    options["pubKeyCredParams"] = params;

    options["timeout"] = conf.timeout_ms;
    options["attestation"] = conf.attestation;

    Json::Value selection(Json::objectValue);
    selection["residentKey"] = "preferred";
    selection["userVerification"] = conf.user_verification;
    options["authenticatorSelection"] = selection;

    if (!exclude_credential_ids.empty()) {
        Json::Value excludes(Json::arrayValue);
        for (const auto& id : exclude_credential_ids) {
            Json::Value item(Json::objectValue);
            item["type"] = "public-key";
            item["id"] = id;
            excludes.append(item);
        }
        options["excludeCredentials"] = excludes;
    }

    return 0;
}

int32_t WebAuthnService::VerifyRegistration(int64_t uid, const Json::Value& credential, RegistrationResult& result, std::string& passkey_name)
{
    if (!IsConfigured()) {
        return WEBAUTHN_NOT_CONFIGURED;
    }
    if (!credential.isObject()) {
        return PARAM_INVALID;
    }
    if (!credential.isMember("type") || !credential["type"].isString() || credential["type"].asString() != "public-key") {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }

    const Json::Value& response = credential["response"];
    if (!response.isObject()) {
        return PARAM_INVALID;
    }
    if (!response.isMember("clientDataJSON") || !response["clientDataJSON"].isString() || !response.isMember("attestationObject")
        || !response["attestationObject"].isString()) {
        return PARAM_MISSING;
    }

    std::string client_data_b64 = response["clientDataJSON"].asString();
    std::string attestation_b64 = response["attestationObject"].asString();
    if (client_data_b64.empty() || attestation_b64.empty()) {
        return PARAM_MISSING;
    }

    std::string client_data_json = chen::StringUtil::Base64UrlDecode(client_data_b64);
    if (client_data_json.empty()) {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }

    WebAuthnClientData client_data;
    if (!ParseClientData(client_data_json, client_data)) {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }
    if (client_data.type != "webauthn.create") {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }
    if (client_data.cross_origin) {
        return WEBAUTHN_ORIGIN_MISMATCH;
    }
    if (chen::StringUtil::Trim(client_data.origin, kTrimChars) != Origin()) {
        WARN(logger) << "webauthn origin mismatch expected=" << Origin() << " got=" << client_data.origin;
        return WEBAUTHN_ORIGIN_MISMATCH;
    }

    if (!ConsumeChallenge(kRegistrationChallengePrefix, uid, client_data.challenge, passkey_name)) {
        return WEBAUTHN_CHALLENGE_INVALID;
    }

    std::string attestation = chen::StringUtil::Base64UrlDecode(attestation_b64);
    if (attestation.empty()) {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }

    CborValue attestation_obj;
    if (!DecodeCbor(attestation, attestation_obj) || attestation_obj.type != CborType::Map) {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }

    const CborValue* fmt = attestation_obj.Get("fmt");
    const CborValue* auth_data_value = attestation_obj.Get("authData");
    if (!fmt || fmt->type != CborType::TextString || !auth_data_value || auth_data_value->type != CborType::ByteString) {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }
    if (fmt->bytes != "none") {
        // 请求的是 attestation=none；这里不建立证书链信任，只记录实际格式。
        INFO(logger) << "webauthn registration fmt=" << fmt->bytes << " uid=" << uid;
    }

    WebAuthnAuthData auth_data;
    if (!ParseRegistrationData(auth_data_value->bytes, auth_data)) {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }
    if (!(auth_data.flags & kFlagUp)) {
        return WEBAUTHN_VERIFY_FAILED;
    }

    const WebAuthnConf conf = GetWebAuthnConf();

    if (conf.user_verification == "required" && !(auth_data.flags & kFlagUv)) {
        return WEBAUTHN_VERIFY_FAILED;
    }
    if (chen::StringUtil::HexEncode(auth_data.rp_id_hash, false) != chen::EncryptorUtil::SHA256(RpId())) {
        return WEBAUTHN_RP_ID_MISMATCH;
    }

    if (!credential.isMember("rawId") || !credential["rawId"].isString()) {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }
    std::string raw_id = credential["rawId"].asString();
    std::string raw_id_bytes = chen::StringUtil::Base64UrlDecode(raw_id);
    if (raw_id_bytes.empty() || raw_id_bytes != auth_data.credential_id) {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }

    if (credential.isMember("id") && !credential["id"].isString()) {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }
    std::string id = credential.isMember("id") ? credential["id"].asString() : "";
    if (!id.empty()) {
        std::string id_bytes = chen::StringUtil::Base64UrlDecode(id);
        if (id_bytes.empty() || id_bytes != auth_data.credential_id) {
            return WEBAUTHN_INVALID_CREDENTIAL;
        }
    }

    if (auth_data.credential_id.size() > 1023) {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }

    CoseKey cose_key;
    if (!ParseCoseKey(auth_data.cose_key, cose_key)) {
        return WEBAUTHN_UNSUPPORTED_ALG;
    }
    if (cose_key.alg != -7 && cose_key.alg != -257 && cose_key.alg != -8) {
        return WEBAUTHN_UNSUPPORTED_ALG;
    }

    result.credential_id = chen::StringUtil::Base64UrlEncode(auth_data.credential_id);
    if (result.credential_id.size() > kMaxCredentialIdB64) {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }
    result.public_key = chen::StringUtil::Base64UrlEncode(auth_data.cose_key);
    result.alg = cose_key.alg;
    result.sign_count = auth_data.sign_count;
    result.aaguid = chen::StringUtil::HexEncode(auth_data.aaguid, false);
    result.backup_eligible = (auth_data.flags & kFlagBe) ? 1 : 0;
    result.backup_state = (auth_data.flags & kFlagBs) ? 1 : 0;

    const Json::Value& transports = response["transports"];
    if (transports.isArray()) {
        for (Json::ArrayIndex i = 0; i < transports.size(); ++i) {
            if (!transports[i].isString()) {
                continue;
            }
            if (!result.transports.empty()) {
                result.transports += ",";
            }
            result.transports += transports[i].asString();
        }
    } else if (transports.isString()) {
        result.transports = transports.asString();
    }

    if (passkey_name.empty()) {
        passkey_name = "Passkey";
    }

    INFO(logger) << "webauthn registration verified uid=" << uid << " alg=" << result.alg << " credential=" << result.credential_id;
    return 0;
}

int32_t WebAuthnService::CreateAuthenticationOptions(int64_t uid, const std::vector<std::string>& allow_credential_ids, Json::Value& options)
{
    if (!IsConfigured()) {
        return WEBAUTHN_NOT_CONFIGURED;
    }

    std::string challenge;
    try {
        challenge = chen::StringUtil::Base64UrlEncode(chen::EncryptorUtil::GenerateRandomKey(32));
    } catch (...) {
        return INTERNAL_ERROR;
    }
    if (challenge.empty()) {
        return INTERNAL_ERROR;
    }

    if (!StoreChallenge(kAuthenticationChallengePrefix, uid, challenge, "1")) {
        return REDIS_OPERATION_FAILED;
    }

    const WebAuthnConf conf = GetWebAuthnConf();

    options = Json::Value(Json::objectValue);
    options["challenge"] = challenge;
    options["rpId"] = RpId();
    options["timeout"] = conf.timeout_ms;
    options["userVerification"] = conf.user_verification;

    if (!allow_credential_ids.empty()) {
        Json::Value allow(Json::arrayValue);
        for (const auto& id : allow_credential_ids) {
            Json::Value item(Json::objectValue);
            item["type"] = "public-key";
            item["id"] = id;
            allow.append(item);
        }
        options["allowCredentials"] = allow;
    }

    return 0;
}

bool WebAuthnService::ExtractCredentialId(const Json::Value& credential, std::string& credential_id)
{
    credential_id.clear();
    if (!credential.isObject()) {
        return false;
    }
    if (!credential.isMember("type") || !credential["type"].isString() || credential["type"].asString() != "public-key") {
        return false;
    }
    if (!credential.isMember("rawId") || !credential["rawId"].isString()) {
        return false;
    }

    std::string raw_id = credential["rawId"].asString();
    std::string raw_id_bytes = chen::StringUtil::Base64UrlDecode(raw_id);
    if (raw_id.empty() || raw_id_bytes.empty()) {
        return false;
    }

    credential_id = chen::StringUtil::Base64UrlEncode(raw_id_bytes);
    if (credential_id.empty() || credential_id.size() > kMaxCredentialIdB64) {
        credential_id.clear();
        return false;
    }
    return true;
}

int32_t WebAuthnService::VerifyAuthentication(
    int64_t uid,
    const std::string& stored_public_key,
    int64_t stored_sign_count,
    const Json::Value& credential,
    AuthenticationResult& result)
{
    if (!IsConfigured()) {
        return WEBAUTHN_NOT_CONFIGURED;
    }
    if (!uid || !credential.isObject()) {
        return PARAM_INVALID;
    }
    if (!credential.isMember("type") || !credential["type"].isString() || credential["type"].asString() != "public-key") {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }

    const Json::Value& response = credential["response"];
    if (!response.isObject()) {
        return PARAM_INVALID;
    }
    if (!response.isMember("clientDataJSON") || !response["clientDataJSON"].isString() || !response.isMember("authenticatorData")
        || !response["authenticatorData"].isString() || !response.isMember("signature") || !response["signature"].isString()) {
        return PARAM_MISSING;
    }

    std::string client_data_b64 = response["clientDataJSON"].asString();
    std::string authent_data_b64 = response["authenticatorData"].asString();
    std::string signature_b64 = response["signature"].asString();
    if (client_data_b64.empty() || authent_data_b64.empty() || signature_b64.empty()) {
        return PARAM_MISSING;
    }

    std::string client_data_json = chen::StringUtil::Base64UrlDecode(client_data_b64);
    if (client_data_json.empty()) {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }

    WebAuthnClientData client_data;
    if (!ParseClientData(client_data_json, client_data)) {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }
    if (client_data.type != "webauthn.get") {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }
    if (client_data.cross_origin) {
        return WEBAUTHN_ORIGIN_MISMATCH;
    }
    if (chen::StringUtil::Trim(client_data.origin, kTrimChars) != Origin()) {
        WARN(logger) << "webauthn assertion origin mismatch expected=" << Origin() << " got=" << client_data.origin;
        return WEBAUTHN_ORIGIN_MISMATCH;
    }

    // challenge 可能绑定到指定用户，也可能是 discoverable credential 的匿名 challenge。
    std::string challenge_value;
    bool challenge_ok = ConsumeChallenge(kAuthenticationChallengePrefix, uid, client_data.challenge, challenge_value);
    if (!challenge_ok) {
        challenge_ok = ConsumeChallenge(kAuthenticationChallengePrefix, 0, client_data.challenge, challenge_value);
    }
    if (!challenge_ok) {
        return WEBAUTHN_CHALLENGE_INVALID;
    }

    std::string auth_data_raw = chen::StringUtil::Base64UrlDecode(authent_data_b64);
    if (auth_data_raw.empty()) {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }

    WebAuthnAuthData auth_data;
    if (!ParseAuthDataHeader(auth_data_raw, auth_data)) {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }
    if (!(auth_data.flags & kFlagUp)) {
        return WEBAUTHN_VERIFY_FAILED;
    }

    const WebAuthnConf conf = GetWebAuthnConf();
    if (conf.user_verification == "required" && !(auth_data.flags & kFlagUv)) {
        return WEBAUTHN_VERIFY_FAILED;
    }
    if (chen::StringUtil::HexEncode(auth_data.rp_id_hash, false) != chen::EncryptorUtil::SHA256(RpId())) {
        return WEBAUTHN_RP_ID_MISMATCH;
    }

    std::string credential_id;
    if (!ExtractCredentialId(credential, credential_id)) {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }

    // id 与 rawId 必须指向同一个 credential。
    if (credential.isMember("id")) {
        if (!credential["id"].isString()) {
            return WEBAUTHN_INVALID_CREDENTIAL;
        }
        std::string id_bytes = chen::StringUtil::Base64UrlDecode(credential["id"].asString());
        std::string raw_id_bytes = chen::StringUtil::Base64UrlDecode(credential["rawId"].asString());
        if (id_bytes.empty() || raw_id_bytes.empty() || id_bytes != raw_id_bytes) {
            return WEBAUTHN_INVALID_CREDENTIAL;
        }
    }

    // discoverable credential 会返回 userHandle，必须与当前用户的稳定 handle 一致。
    if (response.isMember("userHandle") && !response["userHandle"].isNull()) {
        if (!response["userHandle"].isString()) {
            return WEBAUTHN_INVALID_CREDENTIAL;
        }
        std::string user_handle = response["userHandle"].asString();
        if (!user_handle.empty()) {
            std::string expected_handle = UserHandle(uid);
            std::string provided_bytes = chen::StringUtil::Base64UrlDecode(user_handle);
            std::string expected_bytes = chen::StringUtil::Base64UrlDecode(expected_handle);
            if (expected_handle.empty() || provided_bytes.empty() || expected_bytes.empty() || provided_bytes != expected_bytes) {
                return WEBAUTHN_INVALID_CREDENTIAL;
            }
        }
    }

    std::string signature = chen::StringUtil::Base64UrlDecode(signature_b64);
    if (signature.empty()) {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }

    std::string cose_bytes = chen::StringUtil::Base64UrlDecode(stored_public_key);
    if (cose_bytes.empty()) {
        return WEBAUTHN_INVALID_CREDENTIAL;
    }
    CoseKey cose_key;
    if (!ParseCoseKey(cose_bytes, cose_key)) {
        return WEBAUTHN_UNSUPPORTED_ALG;
    }

    std::string client_data_hash = chen::EncryptorUtil::SHA256Raw(client_data_json);
    if (client_data_hash.empty()) {
        return INTERNAL_ERROR;
    }
    std::string signed_data = auth_data_raw + client_data_hash;
    if (!VerifySignature(cose_key, signed_data, signature)) {
        WARN(logger) << "webauthn signature verify failed uid=" << uid << " credential=" << credential_id;
        return WEBAUTHN_VERIFY_FAILED;
    }

    if (stored_sign_count > 0 && auth_data.sign_count > 0 && static_cast<int64_t>(auth_data.sign_count) <= stored_sign_count) {
        WARN(logger) << "webauthn sign counter regression uid=" << uid << " stored=" << stored_sign_count << " got=" << auth_data.sign_count;
        return WEBAUTHN_VERIFY_FAILED;
    }

    result.credential_id = credential_id;
    result.sign_count = auth_data.sign_count;
    result.user_verified = (auth_data.flags & kFlagUv) != 0;
    result.backup_state = (auth_data.flags & kFlagBs) ? 1 : 0;

    INFO(logger) << "webauthn assertion verified uid=" << uid << " credential=" << result.credential_id;
    return 0;
}
