#include "ArticleCategoryService.hpp"

#include <memory>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Repository/ArticleRelRepository.hpp"
#include "../Repository/CategoryRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

bool ArticleCategoryService::UpdateForArticle(int64_t article_id,
    int64_t uid,
    const std::vector<int64_t>& add_ids,
    const std::vector<int64_t>& del_ids,
    std::vector<int64_t>& added,
    std::vector<int64_t>& removed)
{
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto trans = db->openTransaction();
    if (!trans) {
        return false;
    }
    int64_t now = chen::TimeUtil::GetCurrentSec();

    for (auto cid : add_ids) {
        auto category = CategoryRepository::Get(cid, db);
        if (!category || category->getUserId() != uid) {
            WARN(logger) << "skip category not owned by user, user_id=" << uid << " category_id=" << cid;
            continue;
        }
        auto rel = ArticleCategoryRelRepository::GetByArticleAndCategory(article_id, cid, db);
        if (rel) {
            if (rel->getIsDeleted()) {
                rel->setIsDeleted(0);
                rel->setUpdateTime(now);
                if (!ArticleCategoryRelRepository::Update(rel, db)) {
                    WARN(logger) << "reactivate category rel failed, article=" << article_id << " category=" << cid;
                    continue;
                }
                added.push_back(cid);
            }
            continue;
        }

        rel = std::make_shared<DB::ArticleCategoryRelInfo>();
        rel->setArticleId(article_id);
        rel->setCategoryId(cid);
        rel->setUpdateTime(now);
        if (!ArticleCategoryRelRepository::Insert(rel, db)) {
            WARN(logger) << "insert category rel failed, article=" << article_id << " category=" << cid;
            continue;
        }
        added.push_back(cid);
    }

    for (auto cid : del_ids) {
        auto category = CategoryRepository::Get(cid, db);
        if (!category || category->getUserId() != uid) {
            WARN(logger) << "skip category not owned by user, user_id=" << uid << " category_id=" << cid;
            continue;
        }
        auto rel = ArticleCategoryRelRepository::GetByArticleAndCategory(article_id, cid, db);
        if (!rel || rel->getIsDeleted()) {
            continue;
        }
        rel->setIsDeleted(1);
        rel->setUpdateTime(now);
        if (!ArticleCategoryRelRepository::Update(rel, db)) {
            WARN(logger) << "remove category rel failed, article=" << article_id << " category=" << cid;
            continue;
        }
        removed.push_back(cid);
    }

    if (!trans->commit()) {
        ERROR(logger) << "ArticleCategoryService commit failed, article=" << article_id;
        return false;
    }
    return true;
}
