#include "CommentListServlet.hpp"

#include "../../Manager/UserManager.hpp"
#include "../../Util/Util.hpp"

#include <Chen/Util/Util.hpp>
#include <json/json.h>
#include "../../Repository/ArticleRepository.hpp"
#include "../../Repository/CommentQueryRepository.hpp"
#include "../../Repository/CommentPraiseRepository.hpp"

CommentListServlet::CommentListServlet()
    : BlogServlet("CommentListServlet")
{
}

static Json::Value BuildCommentItem(DB::CommentInfo::ptr c,
    const std::unordered_map<int64_t, std::vector<DB::CommentInfo::ptr>>& children,
    int64_t uid)
{
    Json::Value item;
    item["id"] = c->getId();
    item["content"] = c->getContent();
    item["user_id"] = c->getUserId();
    item["time"] = chen::TimeUtil::Time2StrSec(c->getCreateTime());

    auto user = UserMgr::GetInstance()->get(c->getUserId());
    if (user) {
        item["user"] = user->getName();
        item["avatar"] = user->getAvatar();
    } else {
        item["user"] = "";
        item["avatar"] = "";
    }

    int64_t likes = CommentPraiseRepository::CountByComment(c->getId(), GetDB());
    item["likes"] = likes;
    if (uid > 0) {
        item["is_liked"] = CommentPraiseRepository::IsPraising(uid, c->getId(), GetDB());
    } else {
        item["is_liked"] = false;
    }

    // recursively build nested replies
    Json::Value repliesArr(Json::arrayValue);
    auto it = children.find(c->getId());
    if (it != children.end()) {
        for (auto& child : it->second) {
            repliesArr.append(BuildCommentItem(child, children, uid));
        }
    }
    item["replies"] = repliesArr;
    item["reply_count"] = it != children.end() ? it->second.size() : 0;

    return item;
}

int32_t CommentListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, article_id, "article_id");

        // check article exists
        auto article = ArticleRepository::Get(article_id, GetDB());
        if (!article || article->getIsDeleted()) {
            result->setErrno(ARTICLE_NOT_FOUND);
            break;
        }

        int64_t uid = getUserId(request);

        // fetch all approved comments for this article
        std::vector<DB::CommentInfo::ptr> allComments;
        CommentQueryRepository::ListAllByArticle(allComments, article_id, GetDB());

        // build parent_id -> [children] map
        std::unordered_map<int64_t, std::vector<DB::CommentInfo::ptr>> children;
        for (auto& c : allComments) {
            children[c->getParentId()].push_back(c);
        }

        // sort each group by create_time ascending (oldest first)
        for (auto& [pid, vec] : children) {
            std::sort(vec.begin(), vec.end(), [](auto& a, auto& b) {
                return a->getCreateTime() < b->getCreateTime();
            });
        }

        // build top-level list (parent_id == 0)
        Json::Value list(Json::arrayValue);
        auto it = children.find(0);
        if (it != children.end()) {
            for (auto& c : it->second) {
                list.append(BuildCommentItem(c, children, uid));
            }
        }

        result->set("total", it != children.end() ? it->second.size() : 0);
        result->set("list", list);
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
