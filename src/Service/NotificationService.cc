#include "NotificationService.hpp"

#include <memory>

#include <json/json.h>

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/JsonUtil.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../I18n/MessageCatalog.hpp"
#include "../Repository/NotificationQueryRepository.hpp"
#include "../Repository/NotificationRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

static bool UpdateReadState(const std::vector<DB::NotificationInfo::ptr>& infos, int64_t now, chen::IDB::ptr conn)
{
    for (const auto& info : infos) {
        info->setIsRead(1);
        info->setReadTime(now);
        if (!NotificationRepository::Update(info, conn)) {
            return false;
        }
        NotificationRepository::Put(info);
    }
    return true;
}

DB::NotificationInfo::ptr NotificationService::AddNotification(const NotificationData& data)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "AddNotification: get db failed";
        return nullptr;
    }

    auto info = std::make_shared<DB::NotificationInfo>();
    info->setUserId(data.user_id);

    std::string title = data.title;
    std::string content = data.content;
    const std::string default_lang = MessageCatalogRegistry::GetInstance()->DefaultLanguage();
    if (!data.title_id.empty()) {
        title = MessageCatalogRegistry::GetInstance()->render(default_lang, data.title_id, data.params);
    }
    if (!data.content_id.empty()) {
        content = MessageCatalogRegistry::GetInstance()->render(default_lang, data.content_id, data.params);
    }

    Json::Value params(Json::objectValue);
    for (const auto& [key, value] : data.params) {
        params[key] = value;
    }

    info->setTitle(title);
    info->setContent(content);
    info->setTitleId(data.title_id);
    info->setContentId(data.content_id);
    info->setParams(chen::JsonUtil::ToString(params));
    info->setType(data.type);
    info->setSenderId(data.sender_id);
    info->setArticleId(data.article_id);
    info->setCommentId(data.comment_id);
    info->setIsRead(0);
    info->setReadTime(0);
    info->setIsDeleted(0);
    info->setIsBroadcast(data.is_broadcast);
    info->setLevel(data.level);
    info->setStartTime(data.start_time);
    info->setEndTime(data.end_time);
    int64_t now = chen::TimeUtil::GetCurrentSec();
    info->setCreateTime(now);
    info->setUpdateTime(now);

    if (!NotificationRepository::Insert(info, db)) {
        ERROR(logger) << "AddNotification insert failed, user_id=" << data.user_id;
        return nullptr;
    }
    NotificationRepository::Put(info);
    NotificationQueryRepository::InvalidateUser(data.user_id);
    return info;
}

bool NotificationService::MarkRead(int64_t uid, int64_t notification_id)
{
    if (uid <= 0 || notification_id <= 0) {
        return false;
    }
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto info = NotificationRepository::Get(notification_id, db);
    if (!info || info->getIsDeleted() || info->getUserId() != uid) {
        return false;
    }
    if (info->getIsRead()) {
        return true;
    }

    info->setIsRead(1);
    info->setReadTime(chen::TimeUtil::GetCurrentSec());
    if (!NotificationRepository::Update(info, db)) {
        ERROR(logger) << "MarkRead update failed, id=" << notification_id;
        return false;
    }
    NotificationRepository::Put(info);
    NotificationQueryRepository::InvalidateUser(uid);
    return true;
}

int64_t NotificationService::MarkRead(int64_t uid, const std::vector<int64_t>& ids)
{
    if (uid <= 0 || ids.empty()) {
        return 0;
    }
    auto db = GetDB();
    if (!db) {
        return 0;
    }

    std::vector<DB::NotificationInfo::ptr> infos;
    infos.reserve(ids.size());
    for (auto id : ids) {
        auto info = NotificationRepository::Get(id, db);
        if (info && !info->getIsDeleted() && info->getUserId() == uid && !info->getIsRead()) {
            infos.push_back(info);
        }
    }
    if (infos.empty()) {
        return 0;
    }
    if (!UpdateReadState(infos, chen::TimeUtil::GetCurrentSec(), db)) {
        return 0;
    }
    NotificationQueryRepository::InvalidateUser(uid);
    return (int64_t)infos.size();
}

int64_t NotificationService::MarkAllRead(int64_t uid)
{
    if (uid <= 0) {
        return 0;
    }
    auto db = GetDB();
    if (!db) {
        return 0;
    }

    std::vector<DB::NotificationInfo::ptr> unread;
    if (!NotificationRepository::ListUnreadByUser(unread, uid, db)) {
        ERROR(logger) << "MarkAllRead query failed, user_id=" << uid;
        return 0;
    }
    if (unread.empty()) {
        return 0;
    }
    if (!UpdateReadState(unread, chen::TimeUtil::GetCurrentSec(), db)) {
        return 0;
    }
    NotificationQueryRepository::InvalidateUser(uid);
    return (int64_t)unread.size();
}

int64_t NotificationService::MarkReadByType(int64_t uid, const std::string& type)
{
    if (uid <= 0 || type.empty()) {
        return 0;
    }
    auto db = GetDB();
    if (!db) {
        return 0;
    }

    std::vector<DB::NotificationInfo::ptr> unread;
    if (!NotificationRepository::ListUnreadByUserType(unread, uid, type, db)) {
        ERROR(logger) << "MarkReadByType query failed, user_id=" << uid << " type=" << type;
        return 0;
    }
    if (unread.empty()) {
        return 0;
    }
    if (!UpdateReadState(unread, chen::TimeUtil::GetCurrentSec(), db)) {
        return 0;
    }
    NotificationQueryRepository::InvalidateUser(uid);
    return (int64_t)unread.size();
}

bool NotificationService::CloseBroadcast(int64_t notification_id)
{
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto info = NotificationRepository::Get(notification_id, db);
    if (!info) {
        return false;
    }
    info->setIsBroadcast(0);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!NotificationRepository::Update(info, db)) {
        ERROR(logger) << "CloseBroadcast update failed, id=" << notification_id;
        return false;
    }
    NotificationRepository::Put(info);
    return true;
}

bool NotificationService::CleanupExpiredBroadcasts()
{
    auto db = GetDB();
    if (!db) {
        return false;
    }
    int64_t now = chen::TimeUtil::GetCurrentSec();
    if (!NotificationRepository::ClearExpiredBroadcasts(now, db)) {
        ERROR(logger) << "CleanupExpiredBroadcasts execute failed";
        return false;
    }
    return true;
}

bool NotificationService::ListByUser(std::vector<DB::NotificationInfo::ptr>& out,
    int64_t uid,
    int32_t offset,
    int32_t size,
    const std::string& type)
{
    out.clear();
    auto db = GetDB();
    if (!db) {
        return false;
    }
    NotificationQueryRepository::ListByUser(out, uid, offset, size, type, db);
    return true;
}

int64_t NotificationService::CountByUser(int64_t uid, const std::string& type)
{
    auto db = GetDB();
    if (!db) {
        return 0;
    }
    return NotificationQueryRepository::CountByUser(uid, type, db);
}

int64_t NotificationService::UnreadCount(int64_t uid)
{
    auto db = GetDB();
    if (!db) {
        return 0;
    }
    return NotificationQueryRepository::UnreadCount(uid, db);
}

bool NotificationService::ListBroadcasts(std::vector<DB::NotificationInfo::ptr>& out, int64_t offset, int64_t size)
{
    out.clear();
    auto db = GetDB();
    if (!db) {
        return false;
    }
    NotificationQueryRepository::ListBroadcasts(out, offset, size, db);
    return true;
}
