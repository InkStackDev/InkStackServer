#include "LabelService.hpp"

#include <map>
#include <memory>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Repository/ArticleRelRepository.hpp"
#include "../Repository/LabelRepository.hpp"
#include "../Util/Util.hpp"

#include <utility>

static chen::Logger::ptr logger = LOG_ROOT();

LabelService::Result LabelService::CreateOrUpdate(int64_t uid,
    int64_t id,
    const std::string& name,
    const std::string& color,
    const std::string& description,
    DB::LabelInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return Result::DbError;
    }

    DB::LabelInfo::ptr info;
    if (id != 0) {
        info = LabelRepository::Get(id, db);
        if (!info) {
            return Result::NotFound;
        }
        if (info->getUserId() != uid) {
            return Result::AccessDenied;
        }
    }

    std::string old_name = info ? info->getName() : std::string();
    if (!info) {
        info = LabelRepository::GetByUserIdName(uid, name, db);
        if (!info) {
            info = std::make_shared<DB::LabelInfo>();
            info->setUserId(uid);
            info->setName(name);
            info->setCreateTime(chen::TimeUtil::GetCurrentSec());
        } else if (info->getIsDeleted()) {
            info->setIsDeleted(0);
        }
    } else {
        info->setName(name);
    }
    info->setDescription(description);
    info->setColor(color);
    info->setIsDeleted(0);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());

    if (!LabelRepository::InsertOrUpdate(info, db)) {
        ERROR(logger) << "LabelService::CreateOrUpdate failed, uid=" << uid << " name=" << name;
        return Result::DbError;
    }
    LabelRepository::Put(info);
    if (!old_name.empty() && old_name != info->getName()) {
        LabelRepository::EvictUserIdName(uid, old_name);
    }
    LabelRepository::EvictUserIdName(uid, info->getName());

    out = info;
    return Result::Ok;
}

bool LabelService::DeleteMany(int64_t uid, const std::set<int64_t>& ids, std::vector<DB::LabelInfo::ptr>& deleted)
{
    if (ids.empty()) {
        return true;
    }
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<DB::LabelInfo::ptr> all;
    if (!LabelRepository::ListByUserId(all, uid, true, db)) {
        return false;
    }
    for (auto& info : all) {
        if (ids.count(info->getId())) {
            deleted.push_back(info);
        }
    }
    if (deleted.empty()) {
        return true;
    }

    auto trans = db->openTransaction();
    if (!trans) {
        return false;
    }
    int64_t now = chen::TimeUtil::GetCurrentSec();
    for (auto& info : deleted) {
        info->setIsDeleted(1);
        info->setUpdateTime(now);
        if (!LabelRepository::Update(info, db)) {
            ERROR(logger) << "LabelService::DeleteMany update failed, id=" << info->getId();
            trans->rollback();
            return false;
        }
    }
    if (!trans->commit()) {
        ERROR(logger) << "LabelService::DeleteMany commit failed";
        return false;
    }
    for (auto& info : deleted) {
        LabelRepository::Put(info);
    }
    return true;
}

bool LabelService::List(int64_t user_id, const std::vector<int64_t>& ids, std::vector<LabelView>& out)
{
    out.clear();
    auto db = GetDB();
    if (!db) {
        return false;
    }

    std::vector<DB::LabelInfo::ptr> infos;
    if (user_id) {
        LabelRepository::ListByUserId(infos, user_id, true, db);
    } else {
        for (auto id : ids) {
            if (!id) {
                continue;
            }
            auto info = LabelRepository::Get(id, db);
            if (info) {
                infos.push_back(info);
            }
        }
    }

    std::map<int64_t, int64_t> article_counts;
    if (!infos.empty()) {
        std::vector<int64_t> label_ids;
        label_ids.reserve(infos.size());
        for (const auto& info : infos) {
            label_ids.push_back(info->getId());
        }
        std::vector<std::pair<int64_t, int64_t>> rows;
        ArticleLabelRelRepository::CountPublishedByLabels(label_ids, rows, db);
        for (const auto& [label_id, count] : rows) {
            article_counts[label_id] = count;
        }
    }

    for (const auto& info : infos) {
        LabelView view;
        view.id = info->getId();
        view.name = info->getName();
        view.color = info->getColor();
        view.description = info->getDescription();
        view.create_time = info->getCreateTime();
        view.article_count = article_counts[info->getId()];
        out.push_back(std::move(view));
    }
    return true;
}
