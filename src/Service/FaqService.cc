#include "FaqService.hpp"

#include <memory>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Repository/FaqRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

bool FaqService::Create(const std::string& cat,
    const std::string& question,
    const std::string& answer,
    int32_t sort_order,
    DB::HelpFaqsInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto info = std::make_shared<DB::HelpFaqsInfo>();
    info->setCat(cat);
    info->setQ(question);
    info->setA(answer);
    info->setSortOrder(sort_order);
    info->setIsDeleted(0);
    int64_t now = chen::TimeUtil::GetCurrentSec();
    info->setCreateTime(now);
    info->setUpdateTime(now);
    if (!FaqRepository::Insert(info, db)) {
        ERROR(logger) << "FaqService::Create insert failed";
        return false;
    }
    out = info;
    return true;
}

bool FaqService::Update(int64_t id, const std::string& question, const std::string& answer, const std::string& cat)
{
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto info = FaqRepository::Get(id, db);
    if (!info) {
        ERROR(logger) << "faq not found: " << id;
        return false;
    }
    if (!question.empty()) {
        info->setQ(question);
    }
    if (!answer.empty()) {
        info->setA(answer);
    }
    if (!cat.empty()) {
        info->setCat(cat);
    }
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!FaqRepository::Update(info, db)) {
        ERROR(logger) << "FaqService::Update failed, id=" << id;
        return false;
    }
    return true;
}

bool FaqService::Remove(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto info = FaqRepository::Get(id, db);
    if (!info) {
        ERROR(logger) << "faq not found: " << id;
        return false;
    }
    info->setIsDeleted(1);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!FaqRepository::Update(info, db)) {
        ERROR(logger) << "FaqService::Remove failed, id=" << id;
        return false;
    }
    return true;
}
