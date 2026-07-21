# Task 1 报告: 数据库查询方法添加 duration 阈值过滤

## 实现内容

在 `src/database/database_manager.cpp` 中为全部 7 个 `sessions` 表读取查询方法添加了 `min_record_threshold` 过滤：

| 方法 | 变更 |
|------|------|
| `getTodaySummary()` | 添加 `AND duration_seconds >= :threshold` |
| `getTodayTotal()` | 添加 `AND duration_seconds >= :threshold` |
| `getYesterdayTotal()` | 添加 `AND duration_seconds >= :threshold` |
| `getWeekSummary()` | 添加 `AND duration_seconds >= :threshold` |
| `getAppRank()` | 主查询和子查询两处均添加 `AND duration_seconds >= :threshold` |
| `getAllSessions()` | 两个分支（带日期范围/不带）均添加 |
| `getDailySummaries()` | 两个分支均添加 |

每个方法通过 `getSetting("min_record_threshold", "40").toInt()` 读取阈值（默认 40 秒），并使用命名绑定参数 `:threshold` 注入 SQL。

## 测试结果

```
All database tests passed!
All exporter tests passed!
```

全部 14 个测试通过（test_database: 12, test_exporter: 2）。

## 变更文件

- `src/database/database_manager.cpp` — 27 行新增，1 行删除

## 提交

- **SHA:** `33a396a`
- **分支:** `feature/settings-list-optimization`
- **消息:** `feat: add min_record_threshold filter to all query methods`

## 问题

### 死锁 bug（已修复：`a777d40`）

`getSetting()`（第 317 行）内部会获取 `QMutexLocker lock(&m_mutex)`，但全部 7 个调用方也在持锁状态下调用它。`QMutex` 默认为非递归锁——同一线程上重复加锁会导致未定义行为/死锁。

**修复：** 在全部 7 个方法中，将 `int threshold = getSetting("min_record_threshold", "40").toInt();` 移至 `QMutexLocker lock(&m_mutex);` **之前**。`getSetting()` 在无竞争条件下自行获取锁，随后主方法再获取锁，不再发生重入。

| 方法 | 修复状态 |
|------|----------|
| `getTodaySummary()` | 已修复 |
| `getTodayTotal()` | 已修复 |
| `getYesterdayTotal()` | 已修复 |
| `getWeekSummary()` | 已修复 |
| `getAppRank()` | 已修复 |
| `getAllSessions()` | 已修复 |
| `getDailySummaries()` | 已修复 |

**修复提交：** `a777d40` — `fix: move getSetting call before QMutexLocker to avoid recursive lock`

**测试结果（修复后）：** 14/14 通过（database: 12, exporter: 2）。
