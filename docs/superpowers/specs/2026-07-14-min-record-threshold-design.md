# 最低记录阈值 — 设计文档

日期: 2026-07-14

## 背景

用户快速切换窗口（如 Alt+Tab）时会产生极短会话记录（1-3 秒），这些碎片记录污染了统计数据和导出结果。现有 `min_tracking_seconds`（默认 0s）仅控制"新窗口需活跃多久才开始计时"（pending 延迟机制），无法过滤已经开始记录但总时长很短的会话。

## 需求

新增"最低记录阈值"设置，默认 40 秒。单次会话总时长（`duration_seconds`）低于该阈值的记录不计入任何统计、列表和导出。

## 设计

### 1. 设置项

| 属性 | 值 |
|------|-----|
| 键名 | `min_record_threshold` |
| 存储位置 | `settings` 表（与现有设置一致） |
| 默认值 | `"40"` |
| 范围 | 0 ~ 300 秒（0 = 不限制） |
| 步长 | 5 秒 |

### 2. 过滤层级：数据库查询层

所有从 `sessions` 表读取数据的方法（统计 + 导出 + 列表），在 SQL 中统一添加 `duration_seconds >= :threshold` 过滤条件。阈值从 `getSetting("min_record_threshold", "40")` 动态读取，值为 0 时 `>= 0` 等价于不过滤。

**优点：**
- 与现有 `ignored_apps` 过滤模式一致
- 不影响会话记录逻辑（数据库层不改动 `insertSession` / `updateSessionDuration`）
- 阈值变更后统计立即生效（无需重启追踪线程）
- 原始会话数据完整保留，可随时调整阈值查看不同粒度

### 3. 需修改的数据库方法

共 7 个查询方法，每个方法在现有 WHERE 条件下追加 `AND sessions.duration_seconds >= :threshold`：

| 方法 | 文件:行号 | 影响范围 |
|------|-----------|---------|
| `getTodaySummary()` | database_manager.cpp:112 | StatsWidget 今日应用汇总 |
| `getTodayTotal()` | database_manager.cpp:134 | BigNumberWidget 今日总时长 |
| `getYesterdayTotal()` | database_manager.cpp:150 | YesterdayCompare 昨日对比 |
| `getWeekSummary()` | database_manager.cpp:166 | WeeklyBar / WeeklyLine 本周趋势 |
| `getAppRank(date)` | database_manager.cpp:191 | AppRankWidget 应用排行 |
| `getAllSessions(...)` | database_manager.cpp:224 | 会话列表 / CSV 导出 / XLSX 导出 |
| `getDailySummaries(...)` | database_manager.cpp:259 | XLSX 每日汇总导出 |

实现模式（以 `getTodayTotal` 为例）：
```cpp
int DatabaseManager::getTodayTotal()
{
    QMutexLocker lock(&m_mutex);
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QSqlQuery q(m_db);
    q.prepare("SELECT COALESCE(SUM(duration_seconds), 0) FROM sessions "
              "WHERE date(start_time) = date('now', 'localtime') "
              "AND duration_seconds >= :threshold "
              "AND NOT EXISTS (...)");
    q.bindValue(":threshold", threshold);
    q.exec();
    // ...
}
```

### 4. UI 修改

**文件:** `src/ui/settings_dialog.h` / `settings_dialog.cpp`

在追踪设置标签页（Tab 2），`m_minTrackingSeconds` 下方新增一行：

| 属性 | 值 |
|------|-----|
| 标签文本 | `最低记录阈值` |
| 控件类型 | `QSpinBox` |
| 成员变量名 | `m_minRecordThreshold` |
| 范围 | 0 ~ 300 |
| 步长 | 5 |
| 后缀 | `秒` |
| 默认值 | 40 |
| Tooltip | `"单次使用时长低于此值的记录将不计入统计和导出，0 为不限制"` |

`loadSettings()` 读取设置：
```cpp
int threshold = m_db->getSetting("min_record_threshold", "40").toInt();
m_minRecordThreshold->setValue(threshold);
```

`saveSettings()` 写入设置：
```cpp
m_db->setSetting("min_record_threshold",
    QString::number(m_minRecordThreshold->value()));
```

### 5. Tracker 层

`reloadSettings()` （`src/tracker/window_tracker.cpp:220`）读取新设置以保持一致性，但 tracker 线程不直接使用此值（过滤在数据库层完成）。

### 6. 不修改的部分

- 数据库表结构
- `insertSession` / `updateSessionDuration` / `updateSessionEnd` 写入方法
- `closeCurrentSession` 逻辑
- 导出模块 `exporter.h/cpp`、`xlsx_writer.h/cpp`
- 主窗口、统计面板、排行面板（过滤由数据库查询层自动生效）

## 边界条件

- **阈值为 0**：`duration_seconds >= 0` 等价于不过滤，行为与当前一致
- **进行中的会话**：`end_time IS NULL` 的会话，其 `duration_seconds` 是中间值（可能 < 阈值），会被过滤。这是预期行为——尚未满阈值的当前会话不应计入统计。
- **阈值变更**：用户保存设置后，emaited `settingsChanged()` 信号触发 `StatsWidget::refresh()` 和 `AppRankWidget::refresh()`，统计立即更新，无需重启。
