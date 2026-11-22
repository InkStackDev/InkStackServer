#pragma once

#include "DB/CommentPraiseRelInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

class CommentPraiseRelManager {
public:
    void add(DB::CommentPraiseRelInfo::ptr info);

    DB::CommentPraiseRelInfo::ptr get(int64_t id);

    DB::CommentPraiseRelInfo::ptr getByUserAndComment(int64_t user_id, int64_t comment_id);

    // praise/unpraise
    DB::CommentPraiseRelInfo::ptr praise(int64_t user_id, int64_t comment_id);

    bool unpraise(int64_t user_id, int64_t comment_id);

    bool isPraising(int64_t user_id, int64_t comment_id);

    // count praises for a comment
    int64_t countByComment(int64_t comment_id);
};

typedef chen::Singleton<CommentPraiseRelManager> CommentPraiseRelMgr;
