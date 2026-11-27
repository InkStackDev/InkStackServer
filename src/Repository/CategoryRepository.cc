#include "CategoryRepository.hpp"

#include <climits>
#include <memory>
#include <set>
#include <utility>

#include "../Cache/EntityCache.hpp"
#include "../Manager/CacheUtil.hpp"

namespace {

struct CategoryCodec {
    static DB::CategoryInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::CategoryInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setUserId(CacheJsonInt64(v, "user_id"));
        info->setName(CacheJsonString(v, "name"));
        info->setColor(CacheJsonString(v, "color"));
        info->setDescription(CacheJsonString(v, "description"));
        info->setUrl(CacheJsonString(v, "url"));
        info->setIcon(CacheJsonString(v, "icon"));
        info->setParentId(CacheJsonInt64(v, "parent_id"));
        info->setStatus(CacheJsonInt32(v, "status"));
        info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
        info->setCreateTime(CacheJsonTime(v, "create_time"));
        info->setUpdateTime(CacheJsonTime(v, "update_time"));
        info->markClean();
        return info;
    }
};

const EntityCache<DB::CategoryInfo, CategoryCodec> g_category_cache{{"cat2", 300}};

std::string UserNameKey(int64_t user_id, const std::string& name)
{
    return "cat:uid_name:" + std::to_string(user_id) + ":" + name;
}

std::vector<int64_t> CollectKeywordIds(const std::string& keyword, chen::IDB::ptr conn)
{
    std::set<int64_t> ids;
    {
        auto qb = DB::CategoryInfoDao::newQuery();
        if (!qb) {
            return {};
        }
        qb->select("id");
        qb->where("is_deleted", "=", (int64_t)0);
        qb->whereLike("name", keyword);
        std::vector<int64_t> part;
        if (qb->queryColumn<int64_t>(part, conn) == 0) {
            ids.insert(part.begin(), part.end());
        }
    }
    {
        auto qb = DB::CategoryInfoDao::newQuery();
        if (!qb) {
            return {};
        }
        qb->select("id");
        qb->where("is_deleted", "=", (int64_t)0);
        qb->whereLike("description", keyword);
        std::vector<int64_t> part;
        if (qb->queryColumn<int64_t>(part, conn) == 0) {
            ids.insert(part.begin(), part.end());
        }
    }
    return {ids.begin(), ids.end()};
}

std::string SortColumn(const std::string& sort_by)
{
    if (sort_by == "created_at") {
        return "c.create_time";
    }
    if (sort_by == "article_count") {
        return "article_count";
    }
    if (sort_by == "views") {
        return "view_count";
    }
    return "c.name";
}

std::string SortDirection(const std::string& sort_order)
{
    return sort_order == "desc" ? "DESC" : "ASC";
}

} // namespace

DB::CategoryInfo::ptr CategoryRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return g_category_cache.Get(id, [id, conn]() {
        return DB::CategoryInfoDao::Query(id, conn);
    });
}

DB::CategoryInfo::ptr CategoryRepository::GetByUserIdName(int64_t user_id, const std::string& name, chen::IDB::ptr conn)
{
    if (!conn) {
        return nullptr;
    }
    const std::string key = UserNameKey(user_id, name);
    int64_t cached_id = GetCachedIdMapping(key);
    if (cached_id > 0) {
        auto cached = Get(cached_id, conn);
        // 改名后旧映射可能仍在缓存中，校验归属和名称，避免命中脏数据
        if (cached && cached->getUserId() == user_id && cached->getName() == name) {
            return cached;
        }
    }

    auto info = DB::CategoryInfoDao::QueryByUserIdName(user_id, name, conn);
    if (info) {
        Put(info);
        CacheIdMapping(key, info->getId());
    }
    return info;
}

bool CategoryRepository::InsertOrUpdate(DB::CategoryInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::CategoryInfoDao::InsertOrUpdate(info, conn) == 0;
}

bool CategoryRepository::Update(DB::CategoryInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::CategoryInfoDao::Update(info, conn) == 0;
}

void CategoryRepository::ListAll(std::vector<DB::CategoryInfo::ptr>& infos, bool is_valid, chen::IDB::ptr conn)
{
    if (!conn) {
        return;
    }
    auto qb = DB::CategoryInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "ASC");
    DB::CategoryInfoDao::QueryByBuilder(infos, qb, conn);
}

bool CategoryRepository::ListByUserId(std::vector<DB::CategoryInfo::ptr>& infos,
    int64_t user_id,
    bool is_valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return false;
    }
    auto qb = DB::CategoryInfoDao::newQuery();
    if (!qb) {
        return false;
    }
    qb->where("user_id", "=", user_id);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "ASC");
    return DB::CategoryInfoDao::QueryByBuilder(infos, qb, conn) == 0;
}

int64_t CategoryRepository::ListByPages(std::vector<ListItem>& out,
    uint64_t offset,
    uint64_t limit,
    const ListFilter& filter,
    bool is_valid,
    chen::IDB::ptr conn)
{
    out.clear();
    if (!conn) {
        return 0;
    }

    std::vector<int64_t> keyword_ids;
    if (!filter.keyword.empty()) {
        keyword_ids = CollectKeywordIds(filter.keyword, conn);
        if (keyword_ids.empty()) {
            return 0;
        }
    }

    auto qb = DB::CategoryInfoDao::newQuery("c");
    if (!qb) {
        return 0;
    }
    qb->select("c.id, c.user_id, c.name, c.url, c.icon, c.color, c.description, c.parent_id, c.status, "
               "c.create_time, c.update_time, COUNT(a.id) AS article_count, CAST(COALESCE(SUM(a.views), 0) AS SIGNED) "
               "AS view_count");
    qb->join("LEFT", "article_category_rel acr", "acr.category_id = c.id AND acr.is_deleted = 0");
    qb->join("LEFT", "article a", "a.id = acr.article_id");
    qb->whereIf(is_valid, "c.is_deleted", "=", (int64_t)0);
    qb->whereIf(filter.user_id > 0, "c.user_id", "=", filter.user_id);
    qb->whereIf(filter.status != -1, "c.status", "=", (int64_t)filter.status);
    if (!keyword_ids.empty()) {
        qb->whereIn("c.id", keyword_ids);
    }
    qb->groupBy("c.id");
    qb->orderBy(SortColumn(filter.sort_by), SortDirection(filter.sort_order));
    if (offset > (uint64_t)INT32_MAX) {
        offset = INT32_MAX;
    }
    qb->limit((int32_t)(limit > (uint64_t)INT32_MAX ? INT32_MAX : limit));
    qb->offset((int32_t)offset);

    auto data = qb->executeQuery(conn);
    if (!data) {
        return 0;
    }
    while (data->next()) {
        ListItem item;
        item.id = data->getInt64(0);
        item.user_id = data->getInt64(1);
        item.name = data->getString(2);
        item.url = data->getString(3);
        item.icon = data->getString(4);
        item.color = data->getString(5);
        item.description = data->getString(6);
        item.parent_id = data->getInt64(7);
        item.status = data->getInt32(8);
        item.create_time = data->getTime(9);
        item.update_time = data->getTime(10);
        item.article_count = data->getInt64(11);
        item.view_count = data->getInt64(12);
        out.push_back(std::move(item));
    }

    auto count_qb = DB::CategoryInfoDao::newQuery();
    if (!count_qb) {
        return 0;
    }
    count_qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    count_qb->whereIf(filter.user_id > 0, "user_id", "=", filter.user_id);
    count_qb->whereIf(filter.status != -1, "status", "=", (int64_t)filter.status);
    if (!keyword_ids.empty()) {
        count_qb->whereIn("id", keyword_ids);
    }
    int64_t total = 0;
    if (count_qb->executeCount(total, conn) != 0) {
        return 0;
    }
    return total;
}

CategoryRepository::Stats CategoryRepository::GetStats(chen::IDB::ptr conn)
{
    Stats stats;
    if (!conn) {
        return stats;
    }

    {
        auto qb = DB::CategoryInfoDao::newQuery();
        if (qb) {
            qb->where("is_deleted", "=", (int64_t)0);
            qb->executeCount(stats.total, conn);
        }
    }
    {
        auto qb = DB::CategoryInfoDao::newQuery();
        if (qb) {
            qb->where("is_deleted", "=", (int64_t)0);
            qb->where("status", "=", (int64_t)1);
            qb->executeCount(stats.active, conn);
        }
    }
    {
        auto qb = DB::CategoryInfoDao::newQuery("c");
        if (qb) {
            qb->select("COUNT(a.id)");
            qb->join("INNER", "article_category_rel acr", "acr.category_id = c.id AND acr.is_deleted = 0");
            qb->join("LEFT", "article a", "a.id = acr.article_id");
            qb->where("c.is_deleted", "=", (int64_t)0);
            qb->queryScalarInt64(stats.articles, conn);
        }
    }
    {
        auto qb = DB::CategoryInfoDao::newQuery("c");
        if (qb) {
            qb->select("CAST(COALESCE(SUM(a.views), 0) AS SIGNED)");
            qb->join("INNER", "article_category_rel acr", "acr.category_id = c.id AND acr.is_deleted = 0");
            qb->join("LEFT", "article a", "a.id = acr.article_id");
            qb->where("c.is_deleted", "=", (int64_t)0);
            qb->queryScalarInt64(stats.views, conn);
        }
    }
    return stats;
}

void CategoryRepository::Put(const DB::CategoryInfo::ptr& info)
{
    g_category_cache.Put(info);
}

void CategoryRepository::Evict(int64_t id)
{
    g_category_cache.Evict(id);
}

void CategoryRepository::EvictUserIdName(int64_t user_id, const std::string& name)
{
    RedisCacheDel("cache:map:" + UserNameKey(user_id, name));
}
