# 每日推荐

`GET /api/v1/articles/daily` 返回当天的推荐榜单。接口匿名可访问；登录用户会在全局榜的基础上做个性化重排。

## 接口

```
GET /api/v1/articles/daily
```

无需参数。响应：

```json
{
  "errno": 0,
  "msg": "Success",
  "used": 3.12,
  "date": "2026-11-28",
  "total": 6,
  "articles": [
    {
      "article_id": 1024,
      "user_id": 7,
      "author": "Christins",
      "author_avatar": "https://...",
      "title": "…",
      "summary": "正文前 100 字",
      "publish_time": 1795500000,
      "views": 1200,
      "praise": 88,
      "favorites": 31,
      "rank_no": 1,
      "reason": "praise,fresh"
    }
  ]
}
```

`reason` 是逗号分隔的标签，前端可据此渲染「热榜 / 新文 / 你关注的作者 / 匹配你的兴趣」之类文案：

| 标签        | 含义                                 |
|-------------|--------------------------------------|
| `views`     | 浏览量贡献最大                       |
| `praise`    | 点赞贡献最大                         |
| `favorites` | 收藏贡献最大                         |
| `comments`  | 评论贡献最大                         |
| `editor`    | 编辑加权（`article.weight`）贡献最大 |
| `fresh`     | 7 天内发布                           |
| `follow`    | 作者在关注列表（个性化）             |
| `interest`  | 命中兴趣标签（个性化）               |

`rank_no` 已按返回顺序重排（1 开始），全局榜与个性化榜的名次含义一致。

## 打分规则

对候选池中的每篇文章：

```
hot   = w_views*log1p(views) + w_praise*log1p(praise)
      + w_favorites*log1p(favorites) + w_comments*log1p(comments)
      + w_editor*weight
score = hot * exp(-ln2 * age_days / half_life_days) * jitter
```

- `age_days` 是发布至今的天数，按半衰期衰减，老文章自然下沉。
- `jitter = 0.925 + 0.15 * ratio`，`ratio` 是 `date + "#" + article_id` 的稳定哈希（FNV-1a）归一化到 `[0,1)`：
  同一天内所有人看到同一批文章，
  次日因为日期变了整体轮换，避免同一批文章长期霸榜。
- 评论数只对排序靠前的候选补齐（`buffer_size * 3` 篇），避免对整个候选池做 N+1 查询。

候选池：近 `candidate_days` 天、`state = 2`（已发布）、未删除的文章，按 `views` 倒序取前 `candidate_pool` 篇。

选榜时再做多样性约束：同一作者最多 `max_per_author` 篇、同一标签最多 `max_per_label` 篇，
依次贪心挑选直到 `buffer_size` 篇。

## 个性化重排

仅对登录用户生效（`personalize: true`）：

- 作者在关注列表中：`score += follow_boost`。
- 与用户兴趣标签重合：`score += label_boost * 命中标签数`。
  兴趣标签取自用户最近点赞过的文章（最多 50 篇）的标签频次前 5。
- `exclude_interacted: true` 时过滤用户已点赞、已收藏的文章。

重排只在当天的 `buffer_size` 条快照内进行，所以 `buffer_size` 要明显大于 `daily_count`，否则个性化没有腾挪空间。
结果按 `user_id + date` 缓存在 Redis（`cache_ttl_sec`，默认 300 秒）；匿名用户不走缓存，直接读当天的全局榜。

## 存储与任务

- 表 `article_daily_recommend`（`dbproxy/xml/article_daily_recommend.xml`）：
  `recommend_date` + `rank_no` 唯一，`recommend_date` + `article_id` 唯一。
- 后台任务 `article:daily_recommend`：每 10 分钟检查一次，`dedup_interval_ms = 3h`（跨实例、跨重启
  3 小时内最多成功执行一次）；任务体是幂等的——当天快照已存在就直接返回，所以在跨天后的第一次检查里补上当天榜单。
- 请求路径上如果发现当天快照不存在（比如实例刚起来、任务还没跑到），会同步生成一次，保证接口不会空着返回。
- 快照每天约 `buffer_size` 行，可直接保留历史用于回溯/分析。

## 配置

`recommend` 节点（`src/Config/RecommendConfig.hpp`，示例见 `bin/conf/TechHaven.yml`）：
`enabled` / `personalize` / `exclude_interacted` / `daily_count` / `buffer_size` / `candidate_days` /
`candidate_pool` / `half_life_days` / `weight_*` / `max_per_author` / `max_per_label` /
`follow_boost` / `label_boost` / `cache_ttl_sec` / `cache_key`。
配置在模块 `.so` 静态初始化阶段注册，热重载后下一次读取即生效（下一次任务触发 / 下一次请求）。

`follow_boost`、`label_boost` 与 `score` 同单位（`score` 是放大 1000 倍后取整的整数），
默认 30000 相当于把热度分抬高 30，量级上约等于数百次浏览。

## 已知边界

- 没有曝光/阅读历史表，个性化只能基于点赞、收藏与关注，无法做「看过的不再推」。
- 评论数逐篇查询，仅在生成快照时发生（每天一次），不在请求路径上。
- 跨天切换的那一刻，个性化缓存 key 里带日期，不会读到昨天的榜单；但旧 key 要等 `cache_ttl_sec` 过期才消失。
