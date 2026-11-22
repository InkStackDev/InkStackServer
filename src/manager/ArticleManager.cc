#include "ArticleManager.hpp"

#include "../Repository/ArticleRepository.hpp"
#include "../Service/ArticleCounterService.hpp"
#include "../Service/ArticleScheduleService.hpp"
#include "../Util/Util.hpp"

void ArticleManager::add(DB::ArticleInfo::ptr info)
{
    ArticleRepository::Put(info);
}

void ArticleManager::del(int64_t id)
{
    ArticleRepository::Evict(id);
}

DB::ArticleInfo::ptr ArticleManager::get(int64_t id)
{
    return ArticleRepository::Get(id, GetDB());
}

bool ArticleManager::listByUserId(std::vector<DB::ArticleInfo::ptr>& infos, int64_t id, bool valid)
{
    return ArticleQueryRepository::ListByUserId(infos, id, valid, GetDB());
}

int64_t ArticleManager::listByUserIdPages(std::vector<DB::ArticleInfo::ptr>& infos,
    int64_t id,
    int32_t offset,
    int32_t size,
    bool valid,
    int state)
{
    return ArticleQueryRepository::ListByUserIdPages(infos, id, offset, size, valid, state, GetDB());
}

int64_t ArticleManager::listByLabelPages(std::vector<DB::ArticleInfo::ptr>& infos,
    int64_t label_id,
    int32_t offset,
    int32_t size,
    bool valid)
{
    return ArticleQueryRepository::ListByLabelPages(infos, label_id, offset, size, valid, GetDB());
}

int64_t ArticleManager::listByCategoryPages(std::vector<DB::ArticleInfo::ptr>& infos,
    int64_t category_id,
    int32_t offset,
    int32_t size,
    bool valid)
{
    return ArticleQueryRepository::ListByCategoryPages(infos, category_id, offset, size, valid, GetDB());
}

int64_t ArticleManager::listByPages(std::vector<DB::ArticleInfo::ptr>& infos,
    int32_t offset,
    int state,
    int category,
    int32_t role,
    int32_t days,
    int32_t size,
    bool valid)
{
    return ArticleQueryRepository::ListByPages(infos, offset, state, category, role, days, size, valid, GetDB());
}

void ArticleManager::delVerify(int64_t id)
{
    ArticleRepository::Evict(id);
}

void ArticleManager::addVerify(DB::ArticleInfo::ptr info)
{
    ArticleRepository::Put(info);
}

int64_t ArticleManager::listVerifyPages(std::vector<DB::ArticleInfo::ptr>& infos, int32_t offset, int32_t size)
{
    int64_t total = ArticleQueryRepository::ListVerifyPages(infos, offset, size, GetDB());
    for (auto& info : infos) {
        if (info->getIsDeleted()) {
            ArticleRepository::Evict(info->getId());
        }
    }
    return total;
}

std::pair<DB::ArticleInfo::ptr, DB::ArticleInfo::ptr> ArticleManager::nearby(int64_t id)
{
    return ArticleQueryRepository::Nearby(id, GetDB());
}

ArticleManager::ArticleStats ArticleManager::getStats(int32_t category, int32_t role, int32_t days, const std::string& keyword)
{
    return ArticleQueryRepository::GetStats(category, role, days, keyword, GetDB());
}

void ArticleManager::start()
{
    ArticleCounterService::SyncStatsFromDB();
}

void ArticleManager::stop()
{
    ArticleCounterService::FlushDirty();
}

void ArticleManager::flushDirty()
{
    ArticleCounterService::FlushDirty();
}

bool ArticleManager::incViews(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    return ArticleCounterService::IncViews(id, cookie_id, user_id);
}

bool ArticleManager::incPraise(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    return ArticleCounterService::IncPraise(id, cookie_id, user_id);
}

bool ArticleManager::incFavorites(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    return ArticleCounterService::IncFavorites(id, cookie_id, user_id);
}

bool ArticleManager::decPraise(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    return ArticleCounterService::DecPraise(id, cookie_id, user_id);
}

bool ArticleManager::decFavorites(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    return ArticleCounterService::DecFavorites(id, cookie_id, user_id);
}

bool ArticleManager::listUserFav(int64_t id, std::map<int64_t, int64_t>& articles)
{
    return ArticleCounterService::ListUserFav(id, articles);
}

bool ArticleManager::listUserPra(int64_t id, std::map<int64_t, int64_t>& articles)
{
    return ArticleCounterService::ListUserPra(id, articles);
}

bool ArticleManager::listArticleFav(int64_t id, std::map<int64_t, int64_t>& users)
{
    return ArticleCounterService::ListArticleFav(id, users);
}

bool ArticleManager::listArticlePra(int64_t id, std::map<int64_t, int64_t>& users)
{
    return ArticleCounterService::ListArticlePra(id, users);
}
#undef PROC

void ArticleManager::scheduleArticle(int64_t article_id, int64_t publish_time)
{
    ArticleScheduleService::Schedule(article_id, publish_time);
}

void ArticleManager::unscheduleArticle(int64_t article_id)
{
    ArticleScheduleService::Unschedule(article_id);
}

void ArticleManager::onTick()
{
    ArticleScheduleService::PublishDue();
    ArticleCounterService::FlushDirty();
}

void ArticleManager::incPraiseCount(int64_t id)
{
    ArticleCounterService::IncPraiseCount(id);
}

void ArticleManager::decPraiseCount(int64_t id)
{
    ArticleCounterService::DecPraiseCount(id);
}

int64_t ArticleManager::getTodayViews()
{
    return ArticleCounterService::GetTodayViews();
}

int64_t ArticleManager::getTotalViews()
{
    return ArticleCounterService::GetTotalViews();
}

int64_t ArticleManager::getTotalVisitors()
{
    return ArticleCounterService::GetTotalVisitors();
}

void ArticleManager::getCalendarDays(int64_t user_id, int32_t year, int32_t month, std::vector<int32_t>& days)
{
    ArticleScheduleService::GetCalendarDays(user_id, year, month, days);
}

void ArticleManager::clearCalendarCache(int64_t user_id, int64_t publish_time)
{
    ArticleScheduleService::ClearCalendarCache(user_id, publish_time);
}
