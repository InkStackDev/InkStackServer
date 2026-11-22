#include "AdminFeedbackConvertServlet.hpp"

#include "../../Include/Managers.hpp"
#include "../../Service/BugService.hpp"
#include "../../Service/FaqService.hpp"
#include "../../Service/FeedbackService.hpp"
#include "../../Service/RequirementService.hpp"
#include "../../Util/Util.hpp"

#include <Chen/Log/Log.hpp>
#include "../../Domain/AccessTypes.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Domain/UserTypes.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

AdminFeedbackConvertServlet::AdminFeedbackConvertServlet()
    : BlogAdminServlet("AdminFeedbackConvertServlet")
{
}

int32_t AdminFeedbackConvertServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        int64_t uid = viewer.user_id;

        DEFINE_AND_CHECK_STRING(result, id_str, "id");
        DEFINE_AND_CHECK_STRING(result, target, "target");
        DEFINE_AND_CHECK_STRING(result, title, "title");

        // 校验 target 值
        if (target != "faq" && target != "requirement" && target != "bug") {
            result->setErrno(PARAM_INVALID, "target must be faq, requirement or bug");
            break;
        }

        int64_t id = 0;
        try {
            id = chen::TypeUtil::Atoi(id_str);
        } catch (...) {
            result->setErrno(PARAM_INVALID, "invalid id");
            break;
        }

        // 获取反馈
        auto feedback = FeedbackService::Get(id);
        if (!feedback) {
            result->setErrno(FEEDBACK_NOT_FOUND);
            break;
        }

        // content 不传则复用原反馈内容
        std::string content = request->getParam("content");
        if (content.empty()) {
            content = feedback->getContent();
        }

        int64_t converted_id = 0;

        if (target == "faq") {
            // 转换为常见问题
            std::string cat = request->getParam("cat");

            DB::HelpFaqsInfo::ptr faq;
            if (!FaqService::Create(cat, title, content, 0, faq)) {
                result->setErrno(DB_OPERATION_FAILED, "insert faq failed");
                break;
            }

            converted_id = faq->getId();
        } else if (target == "requirement") {
            // 转换为需求
            std::string org_id_str = request->getParam("org_id");
            int64_t org_id = 0;
            if (!org_id_str.empty()) {
                try {
                    org_id = chen::TypeUtil::Atoi(org_id_str);
                } catch (...) {
                    result->setErrno(PARAM_INVALID, "invalid orgId");
                    break;
                }
            }

            RequirementInput input;
            input.org_id = org_id;
            input.creator_id = uid;
            input.title = title;
            input.description = content;

            DB::RequirementInfo::ptr req;
            if (RequirementService::Save(viewer, input, req) != RequirementService::SaveResult::Ok) {
                result->setErrno(DB_OPERATION_FAILED, "insert requirement failed");
                break;
            }

            converted_id = req->getId();
        } else if (target == "bug") {
            // 转换为缺陷
            std::string org_id_str = request->getParam("org_id");
            int64_t org_id = 0;
            if (!org_id_str.empty()) {
                try {
                    org_id = chen::TypeUtil::Atoi(org_id_str);
                } catch (...) {
                    result->setErrno(PARAM_INVALID, "invalid orgId");
                    break;
                }
            }

            BugInput input;
            input.org_id = org_id;
            input.creator_id = uid;
            input.title = title;
            input.description = content;

            DB::BugInfo::ptr bug;
            if (BugService::Save(viewer, input, bug) != BugService::SaveResult::Ok) {
                result->setErrno(DB_OPERATION_FAILED, "insert bug failed");
                break;
            }

            converted_id = bug->getId();
        }

        // 转换成功后删除原反馈
        if (converted_id > 0) {
            if (!FeedbackService::Remove(id)) {
                WARN(logger) << "failed to delete feedback after convert, id=" << id;
            }
        }

        result->set("id", std::to_string(converted_id));
        result->setErrno(SUCCESS);
    } while (0);

    DEBUG(logger) << "AdminFeedbackConvertServlet handle result: " << result->toJsonString();
    response->setBody(result->toJsonString());
    return 0;
}
