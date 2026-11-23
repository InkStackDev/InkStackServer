#include "PasskeyService.hpp"

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Repository/PasskeyRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

DB::UserPasskeyInfo::ptr PasskeyService::Get(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        return nullptr;
    }
    auto info = PasskeyRepository::Get(id, db);
    if (!info || info->getIsDeleted()) {
        return nullptr;
    }
    return info;
}

DB::UserPasskeyInfo::ptr PasskeyService::GetByCredentialId(const std::string& credential_id)
{
    auto db = GetDB();
    if (!db) {
        return nullptr;
    }
    auto info = PasskeyRepository::GetByCredentialId(credential_id, db);
    if (!info || info->getIsDeleted()) {
        return nullptr;
    }
    return info;
}

int PasskeyService::ListByUser(std::vector<DB::UserPasskeyInfo::ptr>& results, int64_t uid)
{
    results.clear();
    if (!uid) {
        return 0;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return -1;
    }
    std::vector<DB::UserPasskeyInfo::ptr> all;
    if (!PasskeyRepository::ListByUser(all, uid, db)) {
        ERROR(logger) << "query passkeys failed uid=" << uid;
        return -1;
    }
    for (auto& info : all) {
        if (info && !info->getIsDeleted()) {
            results.push_back(info);
        }
    }
    return (int)results.size();
}

int PasskeyService::Add(DB::UserPasskeyInfo::ptr info)
{
    if (!info || !info->getUserId() || info->getCredentialId().empty()) {
        return -1;
    }
    auto db = GetDB();
    if (!db) {
        return -1;
    }

    // credential_id 有唯一索引：同一用户曾软删除同一凭据时复用旧行
    auto existing = PasskeyRepository::GetByCredentialId(info->getCredentialId(), db);
    if (existing) {
        if (existing->getUserId() != info->getUserId()) {
            ERROR(logger) << "credential belongs to another user credential=" << info->getCredentialId();
            return -1;
        }
        info->setId(existing->getId());
        info->setIsDeleted(0);
        if (!PasskeyRepository::Update(info, db)) {
            ERROR(logger) << "revive passkey failed user=" << info->getUserId() << " id=" << info->getId();
            return -1;
        }
        return 0;
    }

    if (!PasskeyRepository::Insert(info, db)) {
        ERROR(logger) << "insert passkey failed user=" << info->getUserId();
        return -1;
    }
    return 0;
}

int PasskeyService::Update(DB::UserPasskeyInfo::ptr info)
{
    if (!info) {
        return -1;
    }
    auto db = GetDB();
    if (!db) {
        return -1;
    }
    if (!PasskeyRepository::Update(info, db)) {
        ERROR(logger) << "update passkey failed id=" << info->getId();
        return -1;
    }
    return 0;
}

int PasskeyService::Remove(int64_t id)
{
    auto info = Get(id);
    if (!info) {
        return -1;
    }
    info->setIsDeleted(1);
    info->setLastUsedAt(chen::TimeUtil::GetCurrentSec());
    return Update(info);
}
