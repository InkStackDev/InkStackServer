#include "FeedbackManager.hpp"

#include "../Repository/FeedbackRepository.hpp"
#include "../Service/FeedbackService.hpp"
#include "../Util/Util.hpp"

FeedbackManager::FeedbackManager()
{
}

bool FeedbackManager::list(std::vector<DB::UserFeedbackInfo::ptr>& infos,
    const std::string& type,
    int32_t page,
    int32_t page_size,
    int64_t* total)
{
    int64_t total_cnt = 0;
    if (!FeedbackRepository::ListByPages(infos, type, page, page_size, total_cnt, GetDB())) {
        return false;
    }
    if (total) {
        *total = total_cnt;
    }
    return true;
}

DB::UserFeedbackInfo::ptr FeedbackManager::get(int64_t id)
{
    return FeedbackRepository::Get(id, GetDB());
}

bool FeedbackManager::remove(int64_t id)
{
    return FeedbackService::Remove(id);
}
