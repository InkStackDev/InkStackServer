#include "CommentManager.hpp"

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Repository/CommentQueryRepository.hpp"
#include "../Repository/CommentRepository.hpp"
#include "../Service/CommentService.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

void CommentManager::add(DB::CommentInfo::ptr info)
{
    CommentRepository::Put(info);
}

DB::CommentInfo::ptr CommentManager::get(int64_t id)
{
    return CommentRepository::Get(id, GetDB());
}

DB::CommentInfo::ptr CommentManager::create(int64_t article_id,
    int64_t user_id,
    const std::string& content,
    int64_t parent_id,
    const std::string& ip,
    const std::string& user_agent)
{
    CommentService::CreateCommentInput input;
    input.article_id = article_id;
    input.user_id = user_id;
    input.content = content;
    input.parent_id = parent_id;
    input.ip = ip;
    input.user_agent = user_agent;

    DB::CommentInfo::ptr info;
    if (CommentService::CreateComment(input, info) != CommentService::CreateCommentResult::Ok) {
        ERROR(logger) << "CommentManager create failed, article_id=" << article_id;
        return nullptr;
    }
    return info;
}

bool CommentManager::update(int64_t id, const std::string& content)
{
    auto info = get(id);
    if (!info || info->getIsDeleted()) {
        return false;
    }
    auto db = GetDB();
    if (!db) {
        return false;
    }
    info->setContent(content);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!CommentRepository::Update(info, db)) {
        ERROR(logger) << "CommentManager update failed, id=" << id;
        return false;
    }
    CommentRepository::Put(info);
    CommentQueryRepository::InvalidateArticle(info->getArticleId());
    CommentQueryRepository::InvalidateStats();
    return true;
}

bool CommentManager::del(int64_t id)
{
    auto info = get(id);
    if (!info || info->getIsDeleted()) {
        return false;
    }
    auto db = GetDB();
    if (!db) {
        return false;
    }
    info->setIsDeleted(1);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!CommentRepository::Update(info, db)) {
        ERROR(logger) << "CommentManager del failed, id=" << id;
        return false;
    }
    CommentRepository::Put(info);
    CommentQueryRepository::InvalidateArticle(info->getArticleId());
    CommentQueryRepository::InvalidateStats();
    if (info->getParentId() > 0) {
        CommentQueryRepository::InvalidateReplies(info->getParentId());
    }
    return true;
}

void CommentManager::listAllByArticle(std::vector<DB::CommentInfo::ptr>& results, int64_t article_id)
{
    CommentQueryRepository::ListAllByArticle(results, article_id, GetDB());
}

void CommentManager::listByArticle(std::vector<DB::CommentInfo::ptr>& results,
    int64_t article_id,
    uint64_t offset,
    uint64_t size)
{
    CommentQueryRepository::ListByArticle(results, article_id, offset, size, GetDB());
}

void CommentManager::listReplies(std::vector<DB::CommentInfo::ptr>& results,
    int64_t parent_id,
    uint64_t offset,
    uint64_t size)
{
    CommentQueryRepository::ListReplies(results, parent_id, offset, size, GetDB());
}

int64_t CommentManager::countByArticle(int64_t article_id)
{
    return CommentQueryRepository::CountByArticle(article_id, GetDB());
}

int64_t CommentManager::countReplies(int64_t parent_id)
{
    return CommentQueryRepository::CountReplies(parent_id, GetDB());
}

int64_t CommentManager::listByAdmin(std::vector<DB::CommentInfo::ptr>& results,
    int64_t page_num,
    int64_t page_size,
    int32_t status,
    const std::string& keyword,
    int64_t article_id,
    int32_t is_reported)
{
    return CommentQueryRepository::ListByAdmin(
        results, page_num, page_size, status, keyword, article_id, is_reported, GetDB());
}

int64_t CommentManager::batchUpdateStatus(const std::vector<int64_t>& ids, int32_t status)
{
    return CommentService::BatchUpdateStatus(ids, status);
}

int64_t CommentManager::batchDelete(const std::vector<int64_t>& ids)
{
    return CommentService::BatchDelete(ids);
}

CommentManager::CommentStats CommentManager::getStats()
{
    return CommentQueryRepository::GetStats(GetDB());
}
