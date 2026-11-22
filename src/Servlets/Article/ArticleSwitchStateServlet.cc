#include "ArticleSwitchStateServlet.hpp"

#include "../../Service/ArticleService.hpp"
#include "../../Util/Util.hpp"

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

        auto rt = ArticleService::SwitchState(uid, static_cast<int64_t>(id), new_state);
        if (rt == ArticleService::SwitchStateResult::Ok) {
            result->setErrno(SUCCESS);
        } else if (rt == ArticleService::SwitchStateResult::ArticleNotFound) {
            result->setErrno(ARTICLE_NOT_FOUND);
        } else if (rt == ArticleService::SwitchStateResult::UserNotFound) {
            result->setErrno(USER_NOT_FOUND);
        } else if (rt == ArticleService::SwitchStateResult::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
        } else if (rt == ArticleService::SwitchStateResult::InvalidState) {
            result->setErrno(ARTICLE_INVALID_STATE);
        } else {
            result->setErrno(ARTICLE_UPDATE_FAILED);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
