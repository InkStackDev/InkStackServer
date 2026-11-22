#include "FaqManager.hpp"

#include "../Repository/FaqRepository.hpp"
#include "../Service/FaqService.hpp"
#include "../Util/Util.hpp"

bool FaqManager::listAll(std::vector<DB::HelpFaqsInfo::ptr>& infos)
{
    auto db = GetDB();
    if (!db) {
        return false;
    }
    FaqRepository::ListValid(infos, db);
    return true;
}

bool FaqManager::searchByKeyword(const std::string& keyword, std::vector<DB::HelpFaqsInfo::ptr>& infos)
{
    std::vector<DB::HelpFaqsInfo::ptr> all;
    if (!listAll(all)) {
        return false;
    }
    for (auto& info : all) {
        if (info->getQ().find(keyword) != std::string::npos || info->getA().find(keyword) != std::string::npos) {
            infos.push_back(info);
        }
    }
    return true;
}

bool FaqManager::remove(int64_t id)
{
    return FaqService::Remove(id);
}

bool FaqManager::update(int64_t id, const std::string& q, const std::string& a, const std::string& cat)
{
    return FaqService::Update(id, q, a, cat);
}
