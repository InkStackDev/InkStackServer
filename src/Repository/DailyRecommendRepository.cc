#include "DailyRecommendRepository.hpp"

#include <Chen/Log/Log.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

bool DailyRecommendRepository::HasDate(const std::string& recommend_date, chen::IDB::ptr conn)
{
    if (!conn || recommend_date.empty()) {
        return false;
    }
    auto qb = DB::ArticleDailyRecommendInfoDao::newQuery("r");
    if (!qb) {
        return false;
    }
    qb->select("r.id");
    qb->where("r.recommend_date", "=", recommend_date);
    qb->limit(1);

    std::vector<DB::ArticleDailyRecommendInfo::ptr> rows;
    if (DB::ArticleDailyRecommendInfoDao::QueryByBuilder(rows, qb, conn) != 0) {
        ERROR(logger) << "DailyRecommendRepository::HasDate failed, date=" << recommend_date;
        return false;
    }
    return !rows.empty();
}

bool DailyRecommendRepository::ListByDate(const std::string& recommend_date,
    std::vector<DB::ArticleDailyRecommendInfo::ptr>& rows,
    chen::IDB::ptr conn)
{
    rows.clear();
    if (!conn || recommend_date.empty()) {
        return false;
    }
    auto qb = DB::ArticleDailyRecommendInfoDao::newQuery("r");
    if (!qb) {
        return false;
    }
    qb->select("r.id, r.recommend_date, r.article_id, r.rank_no, r.score, r.reason, r.create_time, r.update_time");
    qb->where("r.recommend_date", "=", recommend_date);
    qb->orderBy("r.rank_no", "ASC");

    if (DB::ArticleDailyRecommendInfoDao::QueryByBuilder(rows, qb, conn) != 0) {
        ERROR(logger) << "DailyRecommendRepository::ListByDate failed, date=" << recommend_date;
        return false;
    }
    return true;
}

bool DailyRecommendRepository::ReplaceDate(const std::string& recommend_date,
    const std::vector<DB::ArticleDailyRecommendInfo::ptr>& rows,
    chen::IDB::ptr conn)
{
    if (!conn || recommend_date.empty() || rows.empty()) {
        return false;
    }
    if (DB::ArticleDailyRecommendInfoDao::DeleteByRecommendDate(recommend_date, conn) != 0) {
        ERROR(logger) << "DailyRecommendRepository::ReplaceDate delete failed, date=" << recommend_date;
        return false;
    }
    for (const auto& row : rows) {
        if (!row) {
            continue;
        }
        if (DB::ArticleDailyRecommendInfoDao::Insert(row, conn) != 0) {
            ERROR(logger) << "DailyRecommendRepository::ReplaceDate insert failed, date=" << recommend_date
                          << " article_id=" << row->getArticleId();
            return false;
        }
    }
    return true;
}
