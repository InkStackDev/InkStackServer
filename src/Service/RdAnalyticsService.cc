#include "RdAnalyticsService.hpp"

#include "../Access/AccessScopeResolver.hpp"
#include "../Domain/OrganizationTypes.hpp"
#include "../Domain/RdTypes.hpp"
#include "../Repository/PrStatsQueryRepository.hpp"
#include "../Repository/RdStatsQueryRepository.hpp"
#include "../Util/Util.hpp"

#include <Chen/Util/TimeUtil.hpp>

#include <cmath>
#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

using Entity = RdStatsQueryRepository::Entity;
using Result = RdAnalyticsService::Result;
using TrendPoint = RdAnalyticsService::TrendPoint;

static time_t ParseDate(const std::string& date)
{
    return chen::TimeUtil::Str2Time(date.c_str(), "%Y-%m-%d");
}

static std::string FormatDate(time_t t)
{
    return chen::TimeUtil::Time2StrSec(t, "%Y-%m-%d");
}

static std::string GroupKey(time_t t, const std::string& granularity)
{
    if (granularity == "week") {
        return FormatDate(chen::TimeUtil::GetWeekStart(t));
    }
    if (granularity == "month") {
        return chen::TimeUtil::Time2StrSec(chen::TimeUtil::GetMonthStart(t), "%Y-%m");
    }
    return FormatDate(t);
}

static std::vector<time_t> BuildDatePoints(time_t start, time_t end, const std::string& granularity)
{
    std::vector<time_t> points;
    time_t cursor = start;
    while (cursor < end) {
        points.push_back(cursor);
        if (granularity == "week") {
            cursor += 7 * 86400;
        } else if (granularity == "month") {
            struct tm tm;
            localtime_r(&cursor, &tm);
            tm.tm_mon += 1;
            cursor = mktime(&tm);
        } else {
            cursor += 86400;
        }
    }
    return points;
}

static Result ResolveScope(const Viewer& viewer, int64_t org_id, OrgScope& scope, chen::IDB::ptr& db)
{
    if (!viewer.valid()) {
        return Result::AccessDenied;
    }
    db = GetDB();
    if (!db) {
        return Result::DbError;
    }
    if (!AccessScopeResolver::ResolveOrgs(scope, viewer, db)) {
        return Result::DbError;
    }
    if (viewer.is_platform_admin) {
        return Result::Ok;
    }
    if (org_id > 0) {
        if (!scope.allows(org_id, OrgRole::REPORTER)) {
            return Result::AccessDenied;
        }
    } else if (scope.orgIds(OrgRole::REPORTER).empty()) {
        return Result::NoAccessibleOrg;
    }
    return Result::Ok;
}

static void AddDailyCounts(std::map<std::string, TrendPoint>& series,
    const std::vector<std::pair<std::string, int64_t>>& rows,
    const std::string& granularity,
    int64_t TrendPoint::* field)
{
    for (const auto& [date, count] : rows) {
        time_t t = ParseDate(date);
        auto it = series.find(GroupKey(t, granularity));
        if (it != series.end()) {
            (it->second).*field += count;
        }
    }
}

RdAnalyticsService::Result RdAnalyticsService::GetTrend(const Viewer& viewer, const TrendQuery& query, TrendResult& out)
{
    out = TrendResult{};

    OrgScope scope;
    chen::IDB::ptr db;
    Result result = ResolveScope(viewer, query.org_id, scope, db);
    if (result != Result::Ok) {
        return result;
    }

    std::string granularity = query.granularity;
    if (granularity != "day" && granularity != "week" && granularity != "month") {
        granularity = "day";
    }

    time_t now = time(0);
    time_t end = chen::TimeUtil::GetDayStart(now) + 86400;
    if (!query.end_date.empty()) {
        end = ParseDate(query.end_date) + 86400;
        if (end <= 86400) {
            return Result::InvalidRange;
        }
    }

    int64_t period_days = query.period_days > 0 ? query.period_days : 30;
    time_t start = end - period_days * 86400;
    if (!query.start_date.empty()) {
        start = ParseDate(query.start_date);
        if (start <= 0) {
            return Result::InvalidRange;
        }
    }
    if (start >= end) {
        return Result::InvalidRange;
    }

    const int64_t org_id = query.org_id;
    int64_t new_req = 0;
    int64_t done_req = 0;
    int64_t bug_total = 0;
    int64_t done_bug = 0;
    int64_t new_task = 0;
    int64_t done_task = 0;

    if (!RdStatsQueryRepository::CountCreated(scope, org_id, Entity::Requirement, start, end, new_req, db)
        || !RdStatsQueryRepository::CountByMinStatus(
            scope, org_id, Entity::Requirement, RequirementStatus::DONE, start, end, done_req, db)
        || !RdStatsQueryRepository::CountCreated(scope, org_id, Entity::Bug, start, end, bug_total, db)
        || !RdStatsQueryRepository::CountByMinStatus(
            scope, org_id, Entity::Bug, BugStatus::FIXED, start, end, done_bug, db)
        || !RdStatsQueryRepository::CountCreated(scope, org_id, Entity::Task, start, end, new_task, db)
        || !RdStatsQueryRepository::CountByMinStatus(
            scope, org_id, Entity::Task, TaskStatus::DONE, start, end, done_task, db)) {
        return Result::DbError;
    }

    int64_t total_completed = done_req + done_bug + done_task;

    int64_t total_reviewed = 0;
    int64_t approved = 0;
    if (!PrStatsQueryRepository::CountReviewed(scope, org_id, start, end, total_reviewed, db)
        || !PrStatsQueryRepository::CountApproved(scope, org_id, start, end, approved, db)) {
        return Result::DbError;
    }

    int64_t review_pass_rate = 0;
    if (total_reviewed > 0) {
        review_pass_rate = approved * 100 / total_reviewed;
    }

    double req_cycle = 0.0;
    double bug_cycle = 0.0;
    double task_cycle = 0.0;
    if (!RdStatsQueryRepository::AvgCycleDays(
            scope, org_id, Entity::Requirement, RequirementStatus::DONE, start, end, req_cycle, db)
        || !RdStatsQueryRepository::AvgCycleDays(
            scope, org_id, Entity::Bug, BugStatus::FIXED, start, end, bug_cycle, db)
        || !RdStatsQueryRepository::AvgCycleDays(
            scope, org_id, Entity::Task, TaskStatus::DONE, start, end, task_cycle, db)) {
        return Result::DbError;
    }

    double avg_cycle_time = 0.0;
    if (total_completed > 0) {
        avg_cycle_time = (req_cycle * done_req + bug_cycle * done_bug + task_cycle * done_task) / total_completed;
    }

    int64_t tasks_at_end = 0;
    int64_t tasks_at_start = 0;
    if (!RdStatsQueryRepository::CountCreatedBefore(scope, org_id, Entity::Task, end, tasks_at_end, db)
        || !RdStatsQueryRepository::CountCreatedBefore(scope, org_id, Entity::Task, start, tasks_at_start, db)) {
        return Result::DbError;
    }

    double cycle_delta = 0.0;
    time_t prev_end = start;
    time_t prev_start = start - (end - start);
    if (prev_start > 0) {
        double prev_req = 0.0;
        double prev_bug = 0.0;
        double prev_task = 0.0;
        if (!RdStatsQueryRepository::AvgCycleDays(
                scope, org_id, Entity::Requirement, RequirementStatus::DONE, prev_start, prev_end, prev_req, db)
            || !RdStatsQueryRepository::AvgCycleDays(
                scope, org_id, Entity::Bug, BugStatus::FIXED, prev_start, prev_end, prev_bug, db)
            || !RdStatsQueryRepository::AvgCycleDays(
                scope, org_id, Entity::Task, TaskStatus::DONE, prev_start, prev_end, prev_task, db)) {
            return Result::DbError;
        }
        if (total_completed > 0) {
            double prev_total = (prev_req * done_req + prev_bug * done_bug + prev_task * done_task) / total_completed;
            cycle_delta = avg_cycle_time - prev_total;
        }
    }

    auto date_points = BuildDatePoints(start, end, granularity);
    std::map<std::string, TrendPoint> series;
    for (time_t t : date_points) {
        TrendPoint point;
        point.date = GroupKey(t, granularity);
        series[point.date] = point;
    }

    std::vector<std::pair<std::string, int64_t>> rows;
    if (!RdStatsQueryRepository::ListCreatedByDay(scope, org_id, Entity::Requirement, -1, start, end, rows, db)) {
        return Result::DbError;
    }
    AddDailyCounts(series, rows, granularity, &TrendPoint::requirements);

    if (!RdStatsQueryRepository::ListCreatedByDay(scope, org_id, Entity::Bug, -1, start, end, rows, db)) {
        return Result::DbError;
    }
    AddDailyCounts(series, rows, granularity, &TrendPoint::bugs);

    if (!RdStatsQueryRepository::ListCreatedByDay(scope, org_id, Entity::Task, -1, start, end, rows, db)) {
        return Result::DbError;
    }
    AddDailyCounts(series, rows, granularity, &TrendPoint::tasks);

    if (!RdStatsQueryRepository::ListCreatedByDay(
            scope, org_id, Entity::Requirement, RequirementStatus::DONE, start, end, rows, db)) {
        return Result::DbError;
    }
    AddDailyCounts(series, rows, granularity, &TrendPoint::completed);

    if (!RdStatsQueryRepository::ListCreatedByDay(scope, org_id, Entity::Bug, BugStatus::FIXED, start, end, rows, db)) {
        return Result::DbError;
    }
    AddDailyCounts(series, rows, granularity, &TrendPoint::completed);

    if (!RdStatsQueryRepository::ListCreatedByDay(
            scope, org_id, Entity::Task, TaskStatus::DONE, start, end, rows, db)) {
        return Result::DbError;
    }
    AddDailyCounts(series, rows, granularity, &TrendPoint::completed);

    int64_t pr_count = 0;
    if (!PrStatsQueryRepository::CountCreated(scope, org_id, start, end, pr_count, db)) {
        return Result::DbError;
    }

    out.completed_total = total_completed;
    out.bug_total = bug_total;
    out.review_pass_rate = review_pass_rate;
    out.avg_cycle_time = avg_cycle_time;
    out.task_delta = tasks_at_end - tasks_at_start;
    out.cycle_delta = cycle_delta;
    out.new_requirements = new_req;
    out.new_tasks = new_task;
    out.pr_count = pr_count;
    out.series.reserve(date_points.size());
    for (time_t t : date_points) {
        auto it = series.find(GroupKey(t, granularity));
        if (it != series.end()) {
            out.series.push_back(it->second);
        }
    }

    return Result::Ok;
}

RdAnalyticsService::Result RdAnalyticsService::GetOverview(const Viewer& viewer, int64_t org_id, Overview& out)
{
    out = Overview{};

    OrgScope scope;
    chen::IDB::ptr db;
    Result result = ResolveScope(viewer, org_id, scope, db);
    if (result != Result::Ok) {
        return result;
    }

    int64_t now = time(0);
    RdStatsQueryRepository::StatusStats req_stats;
    RdStatsQueryRepository::StatusStats bug_stats;
    RdStatsQueryRepository::StatusStats task_stats;
    if (!RdStatsQueryRepository::GetStatusStats(scope, org_id, Entity::Requirement, now, req_stats, db)
        || !RdStatsQueryRepository::GetStatusStats(scope, org_id, Entity::Bug, now, bug_stats, db)
        || !RdStatsQueryRepository::GetStatusStats(scope, org_id, Entity::Task, now, task_stats, db)) {
        return Result::DbError;
    }

    out.total_requirements = req_stats.total;
    out.open_requirements = req_stats.open;
    out.total_bugs = bug_stats.total;
    out.unresolved_bugs = bug_stats.open;
    out.total_tasks = task_stats.total;
    out.overdue_tasks = task_stats.overdue;

    return Result::Ok;
}

RdAnalyticsService::Result RdAnalyticsService::GetPrDistribution(const Viewer& viewer,
    const PrDistributionQuery& query,
    PrDistribution& out)
{
    out = PrDistribution{};

    bool is_15d = (query.range == "15d");
    if (!is_15d && query.range != "3m") {
        return Result::InvalidParam;
    }
    if ((is_15d && query.granularity != "day") || (!is_15d && query.granularity != "week")) {
        return Result::InvalidParam;
    }

    OrgScope scope;
    chen::IDB::ptr db;
    Result result = ResolveScope(viewer, query.org_id, scope, db);
    if (result != Result::Ok) {
        return result;
    }

    time_t now = time(0);
    time_t today_start = chen::TimeUtil::GetDayStart(now);
    time_t end = chen::TimeUtil::GetDayStart(today_start + 86400);
    time_t start = 0;
    if (is_15d) {
        start = chen::TimeUtil::GetDayStart(today_start - 14 * 86400);
    } else {
        time_t current_monday = chen::TimeUtil::GetWeekStart(today_start);
        start = chen::TimeUtil::GetWeekStart(current_monday - 12 * 7 * 86400);
    }

    std::vector<time_t> date_points;
    time_t cursor = start;
    while (cursor < end) {
        date_points.push_back(cursor);
        if (is_15d) {
            cursor = chen::TimeUtil::GetDayStart(cursor + 86400);
        } else {
            cursor = chen::TimeUtil::GetWeekStart(cursor + 7 * 86400);
        }
    }

    std::map<std::string, PrPoint> points;
    for (time_t t : date_points) {
        PrPoint point;
        point.date = GroupKey(t, query.granularity);
        if (is_15d) {
            point.end_date = point.date;
        } else {
            time_t week_end = chen::TimeUtil::GetWeekEnd(t);
            if (week_end > today_start) {
                week_end = today_start;
            }
            point.end_date = FormatDate(week_end);
        }
        points[point.date] = point;
    }

    std::vector<std::pair<std::string, int64_t>> rows;
    if (!PrStatsQueryRepository::CountByDay(rows, scope, query.org_id, start, end, false, db)) {
        return Result::DbError;
    }
    for (const auto& [date, count] : rows) {
        std::string key = date;
        if (query.granularity == "week") {
            key = GroupKey(ParseDate(date), "week");
        } else if (query.granularity == "month") {
            key = date.substr(0, 7);
        }
        auto it = points.find(key);
        if (it != points.end()) {
            it->second.total += count;
        }
    }

    if (!PrStatsQueryRepository::CountByDay(rows, scope, query.org_id, start, end, true, db)) {
        return Result::DbError;
    }
    for (const auto& [date, count] : rows) {
        std::string key = date;
        if (query.granularity == "week") {
            key = GroupKey(ParseDate(date), "week");
        } else if (query.granularity == "month") {
            key = date.substr(0, 7);
        }
        auto it = points.find(key);
        if (it != points.end()) {
            it->second.merged += count;
        }
    }

    int64_t total_sum = 0;
    int64_t merged_sum = 0;
    out.points.reserve(date_points.size());
    for (time_t t : date_points) {
        auto it = points.find(GroupKey(t, query.granularity));
        if (it == points.end()) {
            continue;
        }
        total_sum += it->second.total;
        merged_sum += it->second.merged;
        out.points.push_back(it->second);
    }

    out.total = total_sum;
    out.merged = merged_sum;
    if (total_sum > 0) {
        out.merge_rate = (double)merged_sum * 100.0 / (double)total_sum;
        out.merge_rate = std::round(out.merge_rate * 10.0) / 10.0;
    }

    return Result::Ok;
}
