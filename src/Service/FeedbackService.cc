#include "FeedbackService.hpp"

#include <memory>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Repository/FeedbackRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

bool FeedbackService::Submit(int64_t uid,
    const std::string& type,
    const std::string& content,
    const std::string& contact,
    DB::UserFeedbackInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto info = std::make_shared<DB::UserFeedbackInfo>();
    info->setType(type);
    info->setContent(content);
    info->setContact(contact);
    info->setUserId(uid);
    int64_t now = chen::TimeUtil::GetCurrentSec();
    info->setCreateTime(now);
    info->setUpdateTime(now);
    if (!FeedbackRepository::Insert(info, db)) {
        ERROR(logger) << "FeedbackService::Submit insert failed, uid=" << uid;
        return false;
    }
    out = info;
    return true;
}

bool FeedbackService::Remove(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto info = FeedbackRepository::Get(id, db);
    if (!info) {
        ERROR(logger) << "feedback not found: " << id;
        return false;
    }
    info->setIsDeleted(1);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!FeedbackRepository::Update(info, db)) {
        ERROR(logger) << "FeedbackService::Remove failed, id=" << id;
        return false;
    }
    return true;
}
