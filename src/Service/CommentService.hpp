/**
 * @file CommentService.hpp
 * @brief 评论用例：创建、修改、删除
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/CommentInfo.hpp"

#include <cstdint>
#include <string>

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

    enum class DeleteCommentResult {
        Ok,
        CommentNotFound,
        AccessDenied,
        DbError
    };

    static CreateCommentResult CreateComment(const CreateCommentInput& input, DB::CommentInfo::ptr& out);

    static DeleteCommentResult DeleteComment(int64_t uid, int64_t comment_id);
};
