#include "CommentQueryService.hpp"

#include "../Repository/ArticleRepository.hpp"
#include "../Repository/CommentPraiseRepository.hpp"
#include "../Repository/CommentQueryRepository.hpp"
#include "../Repository/CommentRepository.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Util/Util.hpp"

#include <algorithm>
#include <unordered_map>
#include <utility>

static CommentQueryService::Item BuildItem(const DB::CommentInfo::ptr& comment,
    const std::unordered_map<int64_t, std::vector<DB::CommentInfo::ptr>>* children,
    int64_t viewer_uid,
    chen::IDB::ptr db)
{
    CommentQueryService::Item item;
    item.id = comment->getId();
    item.content = comment->getContent();
    item.user_id = comment->getUserId();
    item.create_time = comment->getCreateTime();

    auto user = UserRepository::Get(comment->getUserId(), db);
    if (user) {
        item.user = user->getName();
        item.avatar = user->getAvatar();
    }
    item.likes = CommentPraiseRepository::CountByComment(comment->getId(), db);
    if (viewer_uid > 0) {
        item.is_liked = CommentPraiseRepository::IsPraising(viewer_uid, comment->getId(), db);
    }

    if (children) {
        auto it = children->find(comment->getId());
        if (it != children->end()) {
            item.reply_count = (int64_t)it->second.size();
            for (const auto& child : it->second) {
                item.replies.push_back(BuildItem(child, children, viewer_uid, db));
            }
        }
    }
    return item;
}

CommentQueryService::ListResult CommentQueryService::ListTree(int64_t article_id,
    int64_t viewer_uid,
    std::vector<Item>& out,
    int64_t& total)
{
    out.clear();
    total = 0;
    auto db = GetDB();
    if (!db) {
        return ListResult::DbError;
    }

    auto article = ArticleRepository::Get(article_id, db);
    if (!article || article->getIsDeleted()) {
        return ListResult::ArticleNotFound;
    }

    std::vector<DB::CommentInfo::ptr> all_comments;
    CommentQueryRepository::ListAllByArticle(all_comments, article_id, db);

    std::unordered_map<int64_t, std::vector<DB::CommentInfo::ptr>> children;
    for (const auto& comment : all_comments) {
        children[comment->getParentId()].push_back(comment);
    }
    for (auto& [parent_id, vec] : children) {
        std::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
            return a->getCreateTime() < b->getCreateTime();
        });
    }

    auto it = children.find(0);
    if (it != children.end()) {
        total = (int64_t)it->second.size();
        for (const auto& comment : it->second) {
            out.push_back(BuildItem(comment, &children, viewer_uid, db));
        }
    }
    return ListResult::Ok;
}

CommentQueryService::ListResult CommentQueryService::ListReplies(int64_t comment_id,
    int64_t viewer_uid,
    int64_t offset,
    int64_t size,
    std::vector<Item>& out,
    int64_t& total)
{
    out.clear();
    total = 0;
    auto db = GetDB();
    if (!db) {
        return ListResult::DbError;
    }

    auto parent = CommentRepository::Get(comment_id, db);
    if (!parent || parent->getIsDeleted()) {
        return ListResult::CommentNotFound;
    }

    std::vector<DB::CommentInfo::ptr> replies;
    CommentQueryRepository::ListReplies(replies, comment_id, offset, size, db);
    total = CommentQueryRepository::CountReplies(comment_id, db);
    for (const auto& reply : replies) {
        auto item = BuildItem(reply, nullptr, viewer_uid, db);
        item.reply_count = CommentQueryRepository::CountReplies(reply->getId(), db);
        out.push_back(std::move(item));
    }
    return ListResult::Ok;
}

bool CommentQueryService::GetParent(int64_t comment_id, DB::CommentInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return false;
    }
    out = CommentRepository::Get(comment_id, db);
    return out != nullptr;
}
