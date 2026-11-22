#include "ArticleService.hpp"

#include <memory>
#include <set>
#include <utility>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "DB/ArticleCategoryRelInfo.hpp"
#include "DB/ArticleLabelRelInfo.hpp"
#include "DB/CategoryInfo.hpp"
#include "DB/LabelInfo.hpp"

#include "../Manager/ArticleManager.hpp"
#include "../Manager/UserManager.hpp"
#include "../Permission.hpp"
#include "../Repository/ArticleRelRepository.hpp"
#include "../Repository/ArticleRepository.hpp"
#include "../Repository/CategoryRepository.hpp"
#include "../Repository/LabelRepository.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Search/SearchSync.hpp"
#include "../Util/Util.hpp"
#include "../Event/EventDefine.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

namespace {

bool SyncCategoryRels(int64_t article_id,
    int64_t uid,
    const std::vector<int64_t>& category_ids,
    chen::IDB::ptr db,
    int64_t now)
{
    std::set<int64_t> new_ids(category_ids.begin(), category_ids.end());
    for (auto it = new_ids.begin(); it != new_ids.end();) {
        auto category = CategoryRepository::Get(*it, db);
        if (!category || category->getUserId() != uid || category->getIsDeleted()) {
            it = new_ids.erase(it);
        } else {
            ++it;
        }
    }

    std::set<int64_t> old_ids;
    {
        std::vector<DB::ArticleCategoryRelInfo::ptr> old_rels;
        if (!ArticleCategoryRelRepository::ListByArticle(old_rels, article_id, true, db)) {
            return false;
        }
        for (const auto& rel : old_rels) {
            old_ids.insert(rel->getCategoryId());
        }
    }

    for (auto cid : new_ids) {
        if (old_ids.count(cid)) {
            continue;
        }
        auto existing = ArticleCategoryRelRepository::GetByArticleAndCategory(article_id, cid, db);
        if (existing && existing->getIsDeleted()) {
            existing->setIsDeleted(0);
            existing->setUpdateTime(now);
            if (ArticleCategoryRelRepository::Update(existing, db)) {
                return false;
            }
        } else if (!existing) {
            auto rel = std::make_shared<DB::ArticleCategoryRelInfo>();
            rel->setArticleId(article_id);
            rel->setCategoryId(cid);
            rel->setUpdateTime(now);
            if (ArticleCategoryRelRepository::Insert(rel, db)) {
                return false;
            }
        }
    }

    for (auto cid : old_ids) {
        if (new_ids.count(cid)) {
            continue;
        }
        auto existing = ArticleCategoryRelRepository::GetByArticleAndCategory(article_id, cid, db);
        if (existing && !existing->getIsDeleted()) {
            existing->setIsDeleted(1);
            existing->setUpdateTime(now);
            if (ArticleCategoryRelRepository::Update(existing, db)) {
                return false;
            }
        }
    }
    return true;
}

bool SyncLabelRels(int64_t article_id, const std::vector<int64_t>& label_ids, chen::IDB::ptr db, int64_t now)
{
    std::set<int64_t> new_ids(label_ids.begin(), label_ids.end());
    for (auto it = new_ids.begin(); it != new_ids.end();) {
        auto label = LabelRepository::Get(*it, db);
        if (!label || label->getIsDeleted()) {
            it = new_ids.erase(it);
        } else {
            ++it;
        }
    }

    std::set<int64_t> old_ids;
    {
        std::vector<DB::ArticleLabelRelInfo::ptr> old_rels;
        if (!ArticleLabelRelRepository::ListByArticle(old_rels, article_id, true, db)) {
            return false;
        }
        for (const auto& rel : old_rels) {
            old_ids.insert(rel->getLabelId());
        }
    }

    for (auto lid : new_ids) {
        if (old_ids.count(lid)) {
            continue;
        }
        auto existing = ArticleLabelRelRepository::GetByArticleAndLabel(article_id, lid, db);
        if (existing && existing->getIsDeleted()) {
            existing->setIsDeleted(0);
            existing->setUpdateTime(now);
            if (ArticleLabelRelRepository::Update(existing, db)) {
                return false;
            }
        } else if (!existing) {
            auto rel = std::make_shared<DB::ArticleLabelRelInfo>();
            rel->setArticleId(article_id);
            rel->setLabelId(lid);
            rel->setUpdateTime(now);
            if (ArticleLabelRelRepository::Insert(rel, db)) {
                return false;
            }
        }
    }

    for (auto lid : old_ids) {
        if (new_ids.count(lid)) {
            continue;
        }
        auto existing = ArticleLabelRelRepository::GetByArticleAndLabel(article_id, lid, db);
        if (existing && !existing->getIsDeleted()) {
            existing->setIsDeleted(1);
            existing->setUpdateTime(now);
            if (ArticleLabelRelRepository::Update(existing, db)) {
                return false;
            }
        }
    }
    return true;
}

bool SyncArticleRelations(int64_t article_id,
    int64_t uid,
    const std::vector<int64_t>& category_ids,
    const std::vector<int64_t>& label_ids,
    chen::IDB::ptr db,
    int64_t now)
{
    return SyncCategoryRels(article_id, uid, category_ids, db, now) && SyncLabelRels(article_id, label_ids, db, now);
}

} // namespace

ArticleService::DeleteArticlesResult ArticleService::DeleteArticles(int64_t uid,
    const std::vector<int64_t>& ids,
    std::vector<int64_t>& deleted)
{
    deleted.clear();
    if (ids.empty()) {
        return DeleteArticlesResult::Ok;
    }

    auto db = GetDB();
    if (!db) {
        return DeleteArticlesResult::DbError;
    }

    std::vector<DB::ArticleInfo::ptr> targets;
    for (auto id : ids) {
        auto info = ArticleRepository::Get(id, db);
        if (!info || info->getUserId() != uid || info->getIsDeleted()) {
            continue;
        }
        targets.push_back(info);
    }
    if (targets.empty()) {
        return DeleteArticlesResult::Ok;
    }

    auto trans = db->openTransaction();
    if (!trans) {
        return DeleteArticlesResult::DbError;
    }
    int64_t now = chen::TimeUtil::GetCurrentSec();
    for (auto& info : targets) {
        info->setIsDeleted(1);
        info->setUpdateTime(now);
        if (!ArticleRepository::Update(info, db)) {
            ERROR(logger) << "DeleteArticles update failed, id=" << info->getId();
            return DeleteArticlesResult::DbError;
        }
    }
    if (!trans->commit()) {
        ERROR(logger) << "DeleteArticles commit failed";
        return DeleteArticlesResult::DbError;
    }

    for (auto& info : targets) {
        ArticleRepository::Put(info);
        deleted.push_back(info->getId());

        SearchSyncMgr::GetInstance()->markDirty(info->getId(), SearchSync::OP_REMOVE);
        ArticleMgr::GetInstance()->unscheduleArticle(info->getId());
        if (info->getState() == ArticleManager::Status::PUBLISHED && info->getPublishTime() > 0) {
            ArticleMgr::GetInstance()->clearCalendarCache(info->getUserId(), info->getPublishTime());
        }
    }
    return DeleteArticlesResult::Ok;
}

ArticleService::SwitchStateResult ArticleService::SwitchState(int64_t uid, int64_t article_id, int32_t new_state)
{
    auto db = GetDB();
    if (!db) {
        return SwitchStateResult::DbError;
    }

    auto article = ArticleRepository::Get(article_id, db);
    if (!article) {
        return SwitchStateResult::ArticleNotFound;
    }
    auto user = UserRepository::Get(uid, db);
    if (!user) {
        return SwitchStateResult::UserNotFound;
    }

    int32_t role = user->getRole();
    if (role != UserManager::Role::ADMIN) {
        if (article->getUserId() != uid) {
            return SwitchStateResult::AccessDenied;
        }
        if (new_state != ArticleManager::Status::PRIVATE) {
            return SwitchStateResult::InvalidState;
        }
    }

    int64_t old_publish_time = article->getPublishTime();
    bool notify_author = role == UserManager::Role::ADMIN && article->getUserId() != uid;

    article->setState(new_state);
    article->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    article->setPublishTime(0);

    if (!ArticleRepository::Update(article, db)) {
        ERROR(logger) << "SwitchState update failed, id=" << article_id;
        return SwitchStateResult::DbError;
    }
    ArticleRepository::Put(article);

    SearchSyncMgr::GetInstance()->markDirty(article->getId(), SearchSync::OP_UPSERT);
    ArticleMgr::GetInstance()->unscheduleArticle(article->getId());
    if (old_publish_time > 0) {
        ArticleMgr::GetInstance()->clearCalendarCache(article->getUserId(), old_publish_time);
    }

    if (notify_author) {
        EventArticleStateChangedData data = {};
        data.author_id = article->getUserId();
        data.article_id = article->getId();
        data.article_title = article->getTitle();
        data.new_state = new_state;
        chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ARTICLE_STATE_CHANGED, std::move(data));
    }
    return SwitchStateResult::Ok;
}

ArticleService::CreateArticleResult ArticleService::CreateArticle(const CreateArticleInput& input,
    DB::ArticleInfo::ptr& out)
{
    out.reset();
    if (input.type != ArticleManager::Type::ORIGINAL && input.type != ArticleManager::Type::REPRINT) {
        return CreateArticleResult::InvalidType;
    }

    auto db = GetDB();
    if (!db) {
        return CreateArticleResult::DbError;
    }
    auto trans = db->openTransaction();
    if (!trans) {
        return CreateArticleResult::DbError;
    }

    auto info = std::make_shared<DB::ArticleInfo>();
    info->setTitle(input.title);
    info->setContent(input.content);
    info->setType(input.type);
    info->setUserId(input.user_id);
    info->setState(ArticleManager::Status::PRIVATE);
    int64_t now = chen::TimeUtil::GetCurrentSec();
    info->setCreateTime(now);
    info->setUpdateTime(now);

    if (!ArticleRepository::Insert(info, db)) {
        ERROR(logger) << "CreateArticle insert failed, user_id=" << input.user_id;
        return CreateArticleResult::DbError;
    }
    if (!SyncArticleRelations(info->getId(), input.user_id, input.category_ids, input.label_ids, db, now)) {
        ERROR(logger) << "CreateArticle sync relations failed, id=" << info->getId();
        return CreateArticleResult::DbError;
    }
    if (!trans->commit()) {
        ERROR(logger) << "CreateArticle commit failed, id=" << info->getId();
        return CreateArticleResult::DbError;
    }

    ArticleRepository::Put(info);
    SearchSyncMgr::GetInstance()->markDirty(info->getId(), SearchSync::OP_UPSERT);
    out = info;
    return CreateArticleResult::Ok;
}

ArticleService::UpdateArticleResult ArticleService::UpdateArticle(const UpdateArticleInput& input,
    DB::ArticleInfo::ptr& out)
{
    out.reset();

    auto db = GetDB();
    if (!db) {
        return UpdateArticleResult::DbError;
    }
    auto article = ArticleRepository::Get(input.article_id, db);
    if (!article) {
        return UpdateArticleResult::NotFound;
    }
    if (article->getUserId() != input.user_id) {
        return UpdateArticleResult::AccessDenied;
    }

    int32_t old_state = article->getState();
    article->setTitle(input.title);
    article->setContent(input.content);
    if (input.has_type) {
        if (input.type != ArticleManager::Type::ORIGINAL && input.type != ArticleManager::Type::REPRINT) {
            return UpdateArticleResult::InvalidType;
        }
        article->setType(input.type);
    }
    if (input.has_channel) {
        article->setChannel(input.channel);
    }

    auto author = UserRepository::Get(article->getUserId(), db);
    bool bypass_review = author && CanBypassReview(author->getRole());

    if (old_state == ArticleManager::Status::PUBLISHED) {
        if (!bypass_review) {
            article->setState(ArticleManager::Status::CHECKING);
        }
    } else if (old_state == ArticleManager::Status::REJECTED) {
        article->setState(ArticleManager::Status::PRIVATE);
    }
    int64_t now = chen::TimeUtil::GetCurrentSec();
    article->setUpdateTime(now);

    auto trans = db->openTransaction();
    if (!trans) {
        return UpdateArticleResult::DbError;
    }
    if (!ArticleRepository::Update(article, db)) {
        ERROR(logger) << "UpdateArticle update failed, id=" << input.article_id;
        return UpdateArticleResult::DbError;
    }
    if (input.has_category && !SyncCategoryRels(input.article_id, input.user_id, input.category_ids, db, now)) {
        ERROR(logger) << "UpdateArticle sync categories failed, id=" << input.article_id;
        return UpdateArticleResult::DbError;
    }
    if (input.has_label && !SyncLabelRels(input.article_id, input.label_ids, db, now)) {
        ERROR(logger) << "UpdateArticle sync labels failed, id=" << input.article_id;
        return UpdateArticleResult::DbError;
    }
    if (!trans->commit()) {
        ERROR(logger) << "UpdateArticle commit failed, id=" << input.article_id;
        return UpdateArticleResult::DbError;
    }

    ArticleRepository::Put(article);
    SearchSyncMgr::GetInstance()->markDirty(article->getId(), SearchSync::OP_UPSERT);

    ArticleMgr::GetInstance()->unscheduleArticle(article->getId());
    int32_t new_state = article->getState();
    if ((new_state == ArticleManager::Status::CHECKING || new_state == ArticleManager::Status::PRIVATE)
        && article->getPublishTime() > now) {
        ArticleMgr::GetInstance()->scheduleArticle(article->getId(), article->getPublishTime());
    }
    if (old_state == ArticleManager::Status::PUBLISHED && article->getPublishTime() > 0) {
        ArticleMgr::GetInstance()->clearCalendarCache(article->getUserId(), article->getPublishTime());
    }

    out = article;
    return UpdateArticleResult::Ok;
}

ArticleService::VerifyArticleResult ArticleService::VerifyArticle(int64_t uid, int64_t article_id, int32_t state)
{
    if (state != ArticleManager::Status::PUBLISHED && state != ArticleManager::Status::REJECTED) {
        return VerifyArticleResult::InvalidState;
    }

    auto db = GetDB();
    if (!db) {
        return VerifyArticleResult::DbError;
    }
    auto reviewer = UserRepository::Get(uid, db);
    if (!reviewer) {
        return VerifyArticleResult::UserNotFound;
    }
    int32_t role = reviewer->getRole();
    if (role != UserManager::Role::ADMIN && role != UserManager::Role::CHECKER) {
        return VerifyArticleResult::AccessDenied;
    }

    auto article = ArticleRepository::Get(article_id, db);
    if (!article || article->getIsDeleted()) {
        return VerifyArticleResult::ArticleNotFound;
    }
    if (article->getState() != ArticleManager::Status::CHECKING) {
        return VerifyArticleResult::InvalidState;
    }

    int64_t now = chen::TimeUtil::GetCurrentSec();
    if (state == ArticleManager::Status::REJECTED) {
        article->setState(ArticleManager::Status::REJECTED);
    } else if (article->getPublishTime() <= now) {
        article->setState(ArticleManager::Status::PUBLISHED);
    } else {
        article->setState(ArticleManager::Status::PRIVATE);
    }
    article->setUpdateTime(now);

    if (!ArticleRepository::Update(article, db)) {
        ERROR(logger) << "VerifyArticle update failed, id=" << article_id;
        return VerifyArticleResult::DbError;
    }
    ArticleRepository::Put(article);
    ArticleMgr::GetInstance()->unscheduleArticle(article_id);
    SearchSyncMgr::GetInstance()->markDirty(article_id, SearchSync::OP_UPSERT);

    bool approved = article->getState() == ArticleManager::Status::PUBLISHED;
    EventArticleReviewData data = {};
    data.type = approved ? "approved" : "rejected";
    data.article_id = article->getId();
    data.article_title = article->getTitle();
    data.author_id = article->getUserId();
    data.reviewer_id = uid;
    chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ARTICLE_REVIEW, std::move(data));
    return VerifyArticleResult::Ok;
}

ArticleService::PublishArticleResult ArticleService::PublishArticle(int64_t uid,
    int64_t article_id,
    int64_t publish_time)
{
    auto db = GetDB();
    if (!db) {
        return PublishArticleResult::DbError;
    }
    auto article = ArticleRepository::Get(article_id, db);
    if (!article || article->getIsDeleted()) {
        return PublishArticleResult::ArticleNotFound;
    }
    if (article->getState() == ArticleManager::Status::PUBLISHED
        || article->getState() == ArticleManager::Status::CHECKING) {
        return PublishArticleResult::InvalidState;
    }

    int64_t now = chen::TimeUtil::GetCurrentSec();
    article->setState(ArticleManager::Status::CHECKING);
    article->setPublishTime(publish_time > now ? publish_time : now);
    article->setUpdateTime(now);

    auto author = UserRepository::Get(uid, db);
    bool bypass_review = author && CanBypassReview(author->getRole());
    if (bypass_review) {
        if (article->getPublishTime() <= now) {
            article->setState(ArticleManager::Status::PUBLISHED);
        } else {
            article->setState(ArticleManager::Status::PRIVATE);
        }
    }

    if (!ArticleRepository::Update(article, db)) {
        ERROR(logger) << "PublishArticle update failed, id=" << article_id;
        return PublishArticleResult::DbError;
    }
    ArticleRepository::Put(article);
    SearchSyncMgr::GetInstance()->markDirty(article->getId(), SearchSync::OP_UPSERT);

    ArticleMgr::GetInstance()->clearCalendarCache(article->getUserId(), article->getPublishTime());
    if (article->getPublishTime() > now && article->getState() != ArticleManager::Status::PUBLISHED) {
        ArticleMgr::GetInstance()->scheduleArticle(article_id, article->getPublishTime());
    }

    if (!bypass_review) {
        EventArticleReviewData data = {};
        data.type = "request";
        data.article_id = article->getId();
        data.article_title = article->getTitle();
        data.author_id = uid;
        data.author_name = author ? author->getName() : std::to_string(uid);
        chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ARTICLE_REVIEW, std::move(data));
    }
    return PublishArticleResult::Ok;
}
