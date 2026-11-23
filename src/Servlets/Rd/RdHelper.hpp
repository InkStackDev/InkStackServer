/**
 * @file RdHelper.hpp
 * @brief RD 模块辅助函数（分页、状态枚举映射、JSON 构建等）
 * @author Christins
 * @date 2026-06-03
 * @copyright Apache 2.0
 */
#pragma once

#include "../../Repository/OrganizationRepository.hpp"
#include "../../Util/Util.hpp"
#include "DB/BugInfo.hpp"
#include "DB/RequirementInfo.hpp"
#include "DB/TaskInfo.hpp"

#include <Chen/Http/Http.hpp>
#include <Chen/Util/TimeUtil.hpp>
#include <json/json.h>

#include <ctime>
#include <string>
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Domain/RdTypes.hpp"
#include "../../Repository/UserRepository.hpp"

// ============================================================================
// 分页: page/page_size → offset/size
// ============================================================================
inline void ParsePagination(uint64_t page, uint64_t page_size, uint64_t& offset, uint64_t& size)
{
    if (page < 1) {
        page = 1;
    }
    offset = (page - 1) * page_size;
    size = page_size;
}

// ============================================================================
// 日期/时间桶
// ============================================================================
inline time_t ParseDate(const std::string& date_str)
{
    return chen::TimeUtil::Str2Time(date_str.c_str(), "%Y-%m-%d");
}

inline std::string FormatDate(time_t t)
{
    return chen::TimeUtil::Time2StrSec(t, "%Y-%m-%d");
}

inline std::string FormatGroupKey(time_t t, const std::string& granularity)
{
    if (granularity == "week") {
        return FormatDate(chen::TimeUtil::GetWeekStart(t));
    }
    if (granularity == "month") {
        return chen::TimeUtil::Time2StrSec(chen::TimeUtil::GetMonthStart(t), "%Y-%m");
    }
    return FormatDate(t);
}

// ============================================================================
// 需求状态枚举映射
// ============================================================================
inline const char* RequirementStatusToString(int32_t status)
{
    switch (status) {
    case RequirementStatus::DRAFT:
        return "new";
    case RequirementStatus::PENDING:
        return "developing";
    case RequirementStatus::IN_PROGRESS:
        return "testing";
    case RequirementStatus::DONE:
        return "done";
    case RequirementStatus::CLOSED:
        return "closed";
    default:
        return "new";
    }
}

inline int32_t StringToRequirementStatus(const std::string& s)
{
    if (s == "new") {
        return RequirementStatus::DRAFT;
    }
    if (s == "developing") {
        return RequirementStatus::PENDING;
    }
    if (s == "testing") {
        return RequirementStatus::IN_PROGRESS;
    }
    if (s == "done") {
        return RequirementStatus::DONE;
    }
    if (s == "closed") {
        return RequirementStatus::CLOSED;
    }
    return -1;
}

// ============================================================================
// 缺陷状态枚举映射
// ============================================================================
inline const char* BugStatusToString(int32_t status)
{
    switch (status) {
    case BugStatus::PENDING:
        return "new";
    case BugStatus::IN_PROGRESS:
        return "processing";
    case BugStatus::FIXED:
        return "verified";
    case BugStatus::CLOSED:
        return "closed";
    case BugStatus::REOPENED:
        return "reopened";
    default:
        return "new";
    }
}

inline int32_t StringToBugStatus(const std::string& s)
{
    if (s == "new") {
        return BugStatus::PENDING;
    }
    if (s == "processing") {
        return BugStatus::IN_PROGRESS;
    }
    if (s == "verified") {
        return BugStatus::FIXED;
    }
    if (s == "closed") {
        return BugStatus::CLOSED;
    }
    if (s == "reopened") {
        return BugStatus::REOPENED;
    }
    return -1;
}

// ============================================================================
// 任务状态枚举映射
// ============================================================================
inline const char* TaskStatusToString(int32_t status)
{
    switch (status) {
    case TaskStatus::TODO:
        return "todo";
    case TaskStatus::IN_PROGRESS:
        return "doing";
    case TaskStatus::DONE:
        return "done";
    case TaskStatus::CLOSED:
        return "closed";
    default:
        return "todo";
    }
}

inline int32_t StringToTaskStatus(const std::string& s)
{
    if (s == "todo") {
        return TaskStatus::TODO;
    }
    if (s == "doing") {
        return TaskStatus::IN_PROGRESS;
    }
    if (s == "done") {
        return TaskStatus::DONE;
    }
    if (s == "closed") {
        return TaskStatus::CLOSED;
    }
    return -1;
}

// ============================================================================
// 优先级枚举映射
// ============================================================================
inline const char* PriorityToString(int32_t priority)
{
    switch (priority) {
    case 1:
        return "low";
    case 2:
        return "medium";
    case 3:
        return "high";
    case 4:
        return "urgent";
    default:
        return "medium";
    }
}

inline int32_t StringToPriority(const std::string& s)
{
    if (s == "low") {
        return 1;
    }
    if (s == "medium") {
        return 2;
    }
    if (s == "high") {
        return 3;
    }
    if (s == "urgent") {
        return 4;
    }
    return -1;
}

// ============================================================================
// 严重程度枚举映射
// ============================================================================
inline const char* SeverityToString(int32_t severity)
{
    switch (severity) {
    case 1:
        return "minor";
    case 2:
        return "normal";
    case 3:
        return "serious";
    case 4:
        return "fatal";
    default:
        return "normal";
    }
}

inline int32_t StringToSeverity(const std::string& s)
{
    if (s == "minor") {
        return 1;
    }
    if (s == "normal") {
        return 2;
    }
    if (s == "serious") {
        return 3;
    }
    if (s == "fatal") {
        return 4;
    }
    return -1;
}

// ============================================================================
// 组织角色枚举映射（用于前端展示）
// ============================================================================
inline int32_t OrgRoleToFrontend(int32_t role)
{
    // 前端期望: 1=报告者, 2=开发者, 3=研发主管, 4=组织管理员
    switch (role) {
    case OrgRole::MEMBER:
        return 0;
    case OrgRole::REPORTER:
        return 1;
    case OrgRole::DEVELOPER:
        return 2;
    case OrgRole::DEV_LEAD:
        return 3;
    case OrgRole::ORG_ADMIN:
        return 4;
    default:
        return 0;
    }
}

// ============================================================================
// 构建公共响应字段
// ============================================================================

inline std::string GetOrgName(int64_t org_id)
{
    auto org = OrganizationRepository::Get(org_id, GetDB());
    return org ? org->getName() : "";
}

inline std::string GetUserName(int64_t user_id)
{
    auto user = UserRepository::Get(user_id, GetDB());
    return user ? user->getName() : "";
}

inline std::string GetUserAvatar(int64_t user_id)
{
    auto user = UserRepository::Get(user_id, GetDB());
    return user ? user->getAvatar() : "";
}

// ============================================================================
// 构建 JSON 响应对象
// ============================================================================
inline void BuildRequirementJson(Json::Value& item, DB::RequirementInfo::ptr info)
{
    item["id"] = info->getId();
    item["title"] = info->getTitle();
    item["description"] = info->getDescription();
    item["priority"] = PriorityToString(info->getPriority());
    item["status"] = RequirementStatusToString(info->getStatus());
    item["creator"] = GetUserName(info->getCreatorId());
    item["creator_id"] = info->getCreatorId();
    item["assignee"] = GetUserName(info->getAssigneeId());
    item["assignee_avatar"] = GetUserAvatar(info->getAssigneeId());
    item["assignee_id"] = info->getAssigneeId();
    item["org_id"] = info->getOrgId();
    item["org_name"] = GetOrgName(info->getOrgId());
    item["iteration"] = info->getIteration();
    item["category"] = info->getCategory();
    item["source"] = info->getSource();
    item["deadline"] = info->getDeadline();
    item["created_at"] = info->getCreateTime();
    item["updated_at"] = info->getUpdateTime();
}

inline void BuildBugJson(Json::Value& item, DB::BugInfo::ptr info)
{
    item["id"] = info->getId();
    item["title"] = info->getTitle();
    item["description"] = info->getDescription();
    item["severity"] = SeverityToString(info->getSeverity());
    item["priority"] = PriorityToString(info->getPriority());
    item["status"] = BugStatusToString(info->getStatus());
    item["creator"] = GetUserName(info->getCreatorId());
    item["creator_id"] = info->getCreatorId();
    item["assignee"] = GetUserName(info->getAssigneeId());
    item["assignee_avatar"] = GetUserAvatar(info->getAssigneeId());
    item["assignee_id"] = info->getAssigneeId();
    item["org_id"] = info->getOrgId();
    item["org_name"] = GetOrgName(info->getOrgId());
    item["related_requirement_id"] = info->getRequirementId();
    item["module"] = info->getModule();
    item["steps_to_reproduce"] = info->getStepsToReproduce();
    item["environment"] = info->getEnvironment();
    item["created_at"] = info->getCreateTime();
    item["updated_at"] = info->getUpdateTime();
}

inline void BuildTaskJson(Json::Value& item, DB::TaskInfo::ptr info)
{
    item["id"] = info->getId();
    item["title"] = info->getTitle();
    item["description"] = info->getDescription();
    item["status"] = TaskStatusToString(info->getStatus());
    item["priority"] = PriorityToString(info->getPriority());
    item["creator"] = GetUserName(info->getCreatorId());
    item["creator_id"] = info->getCreatorId();
    item["assignee"] = GetUserName(info->getAssigneeId());
    item["assignee_avatar"] = GetUserAvatar(info->getAssigneeId());
    item["assignee_id"] = info->getAssigneeId();
    item["org_id"] = info->getOrgId();
    item["org_name"] = GetOrgName(info->getOrgId());
    item["requirement_id"] = info->getRequirementId();
    item["bug_id"] = info->getBugId();
    item["deadline"] = info->getDeadline();
    item["estimated_hours"] = info->getEstimatedHours();
    item["created_at"] = info->getCreateTime();
    item["updated_at"] = info->getUpdateTime();
}
