/**
 * @file CommentService.hpp
 * @brief 评论用例：创建、修改、删除、审核、点赞
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/CommentInfo.hpp"

#include <cstdint>
#include <string>
#include <vector>

class CommentService {
public:
    struct CreateCommentInput {
        int64_t article_id = 0;
        int64_t user_id = 0;
        int64_t parent_id = 0;
        std::string content;
        std::string ip;
        std::string user_agent;
    };

    enum class CreateCommentResult {
        Ok,
        ArticleNotFound,
        ParentNotFound,
        DbError
    };

    enum class UpdateCommentResult {
        Ok,
        CommentNotFound,
        AccessDenied,
        DbError
    };

    enum class DeleteCommentResult {
        Ok,
        CommentNotFound,
        AccessDenied,
        DbError
    };

    enum class PraiseResult {
        Ok,
        CommentNotFound,
        DbError
    };

    static CreateCommentResult CreateComment(const CreateCommentInput& input, DB::CommentInfo::ptr& out);

    /**
     * @brief 作者修改评论内容
     */
    static UpdateCommentResult UpdateComment(int64_t uid,
        int64_t comment_id,
        const std::string& content,
        DB::CommentInfo::ptr& out);

    /**
     * @brief 作者删除自己的评论（软删除）
     */
    static DeleteCommentResult DeleteComment(int64_t uid, int64_t comment_id);

    /**
     * @brief 管理员批量修改评论状态（通过/拒绝/垃圾）
     * @return 受影响的评论数
     */
    static int64_t BatchUpdateStatus(const std::vector<int64_t>& ids, int32_t status);

    /**
     * @brief 管理员批量删除评论（软删除）
     * @return 受影响的评论数
     */
    static int64_t BatchDelete(const std::vector<int64_t>& ids);

    /**
     * @brief 点赞/取消点赞切换
     * @param[out] is_praising 操作后是否处于点赞状态
     * @param[out] out 评论对象
     */
    static PraiseResult TogglePraise(int64_t uid, int64_t comment_id, bool& is_praising, DB::CommentInfo::ptr& out);

    static int64_t CountPraise(int64_t comment_id);

    static bool IsPraising(int64_t uid, int64_t comment_id);

    static int64_t CountReplies(int64_t parent_id);
};
