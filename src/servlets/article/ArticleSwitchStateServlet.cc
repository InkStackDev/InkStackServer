#include "ArticleSwitchStateServlet.hpp"

#include "../../Event/EventDefine.hpp"
#include "../../Manager/ArticleManager.hpp"
#include "../../Manager/UserManager.hpp"
#include "../../Search/SearchSync.hpp"
#include "../../Util/Util.hpp"

#include <Chen/Log/Log.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

ArticleSwitchStateServlet::ArticleSwitchStateServlet()
    : BlogLoginedServlet("ArticleSwitchStateServlet")
{
}

int32_t ArticleSwitchStateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, uint64_t, id, "id");
        DEFINE_AND_CHECK_TYPE(result, int, new_state, "new_state");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        auto article = ArticleMgr::GetInstance()->get(id);
        if (!article) {
            result->setErrno(ARTICLE_NOT_FOUND);
            break;
        }

        // 保存旧的 publish_time 用于清除日历缓存
        int64_t oldPublishTime = article->getPublishTime();

        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        int32_t role = current_user->getRole();
        if (role != UserManager::Role::ADMIN) {
            if (article->getUserId() != uid) {
                result->setErrno(ACCESS_DENIED);
                break;
            } else if (new_state != ArticleManager::Status::PRIVATE) {
                result->setErrno(ARTICLE_INVALID_STATE);
                break;
            }
        }

        article->setState(new_state);
        article->setUpdateTime(time(0));
        article->setPublishTime(0);

        auto db = GetDB();
        if (!db) {
            result->setErrno(DB_CONNECTION_FAILED);
            break;
        }
        if (DB::ArticleInfoDao::Update(article, db)) {
            result->setErrno(ARTICLE_UPDATE_FAILED);
            break;
        }
        ArticleMgr::GetInstance()->add(article);
        SearchSyncMgr::GetInstance()->markDirty(article->getId(), SearchSync::OP_UPSERT);

        // 状态变更后，从定时发布调度中移除
        ArticleMgr::GetInstance()->unscheduleArticle(article->getId());

        // 清除旧发布时间对应月份的日历缓存
        if (oldPublishTime > 0) {
            ArticleMgr::GetInstance()->clearCalendarCache(article->getUserId(), oldPublishTime);
        }
        // 如果新状态是已发布且有新的发布时间，也清除对应月份缓存
        if (new_state == ArticleManager::Status::PUBLISHED && article->getPublishTime() > 0) {
            ArticleMgr::GetInstance()->clearCalendarCache(article->getUserId(), article->getPublishTime());
        }

        // Notify article author if admin changed state
        if (role == UserManager::Role::ADMIN && article->getUserId() != uid) {
            EventArticleStateChangedData data = {};
            data.author_id = article->getUserId();
            data.article_id = article->getId();
            data.article_title = article->getTitle();
            data.new_state = new_state;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ARTICLE_STATE_CHANGED, std::move(data));
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
