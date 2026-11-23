#include "CategoryService.hpp"

#include <map>
#include <memory>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Repository/CategoryRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

namespace {

void CollectDescendants(const std::map<int64_t, std::vector<DB::CategoryInfo::ptr>>& children,
    int64_t id,
    std::set<DB::CategoryInfo::ptr>& out)
{
    auto it = children.find(id);
    if (it == children.end()) {
        return;
    }
    for (auto& child : it->second) {
        if (!out.insert(child).second) {
            continue;
        }
        CollectDescendants(children, child->getId(), out);
    }
}

} // namespace

CategoryService::Result CategoryService::CreateOrUpdate(const CategoryInput& input,
    int64_t requester_uid,
    bool admin_route,
    DB::CategoryInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return Result::DbError;
    }

    DB::CategoryInfo::ptr info;
    if (input.id != 0) {
        info = CategoryRepository::Get(input.id, db);
        if (!info) {
            return Result::NotFound;
        }
        if (!admin_route && info->getUserId() != requester_uid) {
            return Result::AccessDenied;
        }
    }

    int64_t owner_id = info ? info->getUserId() : input.owner_id;
    if (input.parent_id != 0) {
        auto parent = CategoryRepository::Get(input.parent_id, db);
        if (!parent || parent->getUserId() != owner_id || input.parent_id == input.id) {
            return Result::InvalidParent;
        }
    }

    std::string old_name = info ? info->getName() : std::string();

    if (!info) {
        info = CategoryRepository::GetByUserIdName(owner_id, input.name, db);
        if (!info) {
            info = std::make_shared<DB::CategoryInfo>();
            info->setUserId(owner_id);
            info->setName(input.name);
            info->setCreateTime(chen::TimeUtil::GetCurrentSec());
        } else if (info->getIsDeleted()) {
            info->setIsDeleted(0);
        }
    } else if (info->getName() != input.name) {
        auto same = CategoryRepository::GetByUserIdName(owner_id, input.name, db);
        if (same && same->getId() != info->getId()) {
            return Result::NameExists;
        }
        info->setName(input.name);
    }

    info->setColor(input.color);
    info->setParentId(input.parent_id);
    info->setIsDeleted(0);
    info->setStatus(input.status);
    info->setUrl(input.url);
    info->setIcon(input.icon);
    info->setDescription(input.description);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());

    if (!CategoryRepository::InsertOrUpdate(info, db)) {
        ERROR(logger) << "CategoryService::CreateOrUpdate failed, name=" << input.name;
        return Result::DbError;
    }
    CategoryRepository::Put(info);
    if (!old_name.empty() && old_name != info->getName()) {
        CategoryRepository::EvictUserIdName(owner_id, old_name);
    }
    CategoryRepository::EvictUserIdName(owner_id, info->getName());

    out = info;
    return Result::Ok;
}

bool CategoryService::DeleteMany(const std::vector<DB::CategoryInfo::ptr>& scope,
    const std::set<int64_t>& root_ids,
    std::vector<DB::CategoryInfo::ptr>& deleted)
{
    if (root_ids.empty() || scope.empty()) {
        return true;
    }

    std::map<int64_t, DB::CategoryInfo::ptr> by_id;
    std::map<int64_t, std::vector<DB::CategoryInfo::ptr>> children;
    for (auto& info : scope) {
        by_id[info->getId()] = info;
        if (info->getParentId() != 0) {
            children[info->getParentId()].push_back(info);
        }
    }

    std::set<DB::CategoryInfo::ptr> collect;
    for (auto id : root_ids) {
        auto it = by_id.find(id);
        if (it == by_id.end()) {
            continue;
        }
        if (!collect.insert(it->second).second) {
            continue;
        }
        CollectDescendants(children, id, collect);
    }
    if (collect.empty()) {
        return true;
    }

    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto trans = db->openTransaction();
    if (!trans) {
        return false;
    }
    int64_t now = chen::TimeUtil::GetCurrentSec();
    for (auto& info : collect) {
        info->setIsDeleted(1);
        info->setUpdateTime(now);
        if (!CategoryRepository::Update(info, db)) {
            ERROR(logger) << "CategoryService::DeleteMany update failed, id=" << info->getId();
            trans->rollback();
            return false;
        }
    }
    if (!trans->commit()) {
        ERROR(logger) << "CategoryService::DeleteMany commit failed";
        return false;
    }
    for (auto& info : collect) {
        CategoryRepository::Put(info);
        deleted.push_back(info);
    }
    return true;
}

CategoryService::DeleteScopeResult CategoryService::DeleteForUser(bool admin_route,
    int64_t requester_uid,
    int64_t owner_uid,
    const std::set<int64_t>& root_ids,
    std::vector<DB::CategoryInfo::ptr>& deleted)
{
    auto db = GetDB();
    if (!db) {
        return DeleteScopeResult::DbError;
    }

    std::vector<DB::CategoryInfo::ptr> scope;
    if (admin_route) {
        if (owner_uid) {
            if (!CategoryRepository::ListByUserId(scope, owner_uid, false, db)) {
                return DeleteScopeResult::DbError;
            }
        } else {
            CategoryRepository::ListAll(scope, false, db);
        }
    } else if (!CategoryRepository::ListByUserId(scope, requester_uid, false, db)) {
        return DeleteScopeResult::DbError;
    }

    if (scope.empty()) {
        return DeleteScopeResult::NoCategories;
    }
    if (!DeleteMany(scope, root_ids, deleted)) {
        return DeleteScopeResult::DbError;
    }
    return DeleteScopeResult::Ok;
}
