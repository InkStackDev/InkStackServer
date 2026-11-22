#include "AdminCommentService.hpp"

#include "../Repository/ArticleRepository.hpp"
#include "../Repository/CommentPraiseRepository.hpp"
#include "../Repository/CommentQueryRepository.hpp"
#include "../Repository/CommentRepository.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Util/Util.hpp"

#include <utility>

bool AdminCommentService::List(int64_t page_num,
    int64_t page_size,
    int32_t status,
    const std::string& keyword,
    int64_t article_id,
    int32_t is_reported,
    std::vector<Item>& out,
    int64_t& total)
{
    out.clear();
    total = 0;
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<DB::CommentInfo::ptr> comments;
    total = CommentQueryRepository::ListByAdmin(
        comments, page_num, page_size, status, keyword, article_id, is_reported, db);
    for (const auto& comment : comments) {
        if (!comment) {
            continue;
        }
        Item item;
        item.id = comment->getId();
        item.content = comment->getContent();
        item.parent_id = comment->getParentId();
        item.status = comment->getStatus();
        item.create_time = comment->getCreateTime();
        item.update_time = comment->getUpdateTime();
        item.likes = CommentPraiseRepository::CountByComment(comment->getId(), db);
        item.reply_count = CommentQueryRepository::CountReplies(comment->getId(), db);
        item.is_reported = comment->getIsReported() != 0;
        item.report_count = comment->getReportCount();
        item.ip = comment->getIp();
        item.user_agent = comment->getUserAgent();

        auto user = UserRepository::Get(comment->getUserId(), db);
        if (user) {
            item.author.valid = true;
            item.author.id = user->getId();
            item.author.name = user->getName();
            item.author.email = user->getEmail();
            item.author.avatar = user->getAvatar();
        }

        item.article_id = comment->getArticleId();
        auto article = ArticleRepository::Get(comment->getArticleId(), db);
        if (article) {
            item.article_title = article->getTitle();
        }
        out.push_back(std::move(item));
    }
    return true;
}

bool AdminCommentService::GetStats(Stats& out)
{
    out = Stats{};
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto stats = CommentQueryRepository::GetStats(db);
    out.total = stats.total;
    out.pending = stats.pending;
    out.approved = stats.approved;
    out.spam = stats.spam;
    out.reported = stats.reported;
    return true;
}

bool AdminCommentService::GetComment(int64_t comment_id, DB::CommentInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return false;
    }
    out = CommentRepository::Get(comment_id, db);
    return out != nullptr;
}
