# 最低记录阈值 — 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在所有查询方法中添加 `duration_seconds >= min_record_threshold` 过滤，并在追踪设置页添加 SpinBox 控件（默认 40s，范围 0-300）。

**Architecture:** 过滤在数据库查询层实现（与现有 `ignored_apps` 过滤模式一致）。每个查询方法读取 `min_record_threshold` 设置，绑定为 SQL 参数。Tracker 和导出模块无需修改。

**Tech Stack:** C++17, Qt6 Sql (QSqlQuery), Qt6 Widgets (QSpinBox, QLabel, QHBoxLayout)

## Global Constraints

- 所有 DB 方法必须 `QMutexLocker lock(&m_mutex)`（已是现状）
- 所有 UI 字体使用 `appFont()` 辅助函数（`Microsoft YaHei`）
- 设置键名 `min_record_threshold`，默认值 `"40"`，以字符串存储
- 遵循现有代码风格（UTF-8 字符串字面量 `QString::fromUtf8(...)`）

---

### Task 1: 数据库查询方法添加 duration 阈值过滤

**Files:**
- Modify: `src/database/database_manager.cpp:112-289`

**Interfaces:**
- Consumes: `getSetting("min_record_threshold", "40")` — 已存在于 `database_manager.cpp`
- Produces: 所有 7 个查询方法对 duration 进行过滤

**受影响的 7 个方法及其行号范围：**
| 方法 | 行号 |
|------|------|
| `getTodaySummary()` | 112-132 |
| `getTodayTotal()` | 134-148 |
| `getYesterdayTotal()` | 150-164 |
| `getWeekSummary()` | 166-189 |
| `getAppRank()` | 191-222 |
| `getAllSessions()` | 224-257 |
| `getDailySummaries()` | 259-289 |

- [ ] **Step 1: 修改 `getTodaySummary()`**

将第 116-121 行的 SQL 改为添加 `AND duration_seconds >= :threshold`：

```cpp
QVector<QVariantMap> DatabaseManager::getTodaySummary()
{
    QMutexLocker lock(&m_mutex);
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QSqlQuery q(m_db);
    q.prepare("SELECT app_name, SUM(duration_seconds) as total_seconds "
              "FROM sessions WHERE date(start_time) = ? "
              "AND duration_seconds >= :threshold "
              "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
              "WHERE sessions.process_name = ia.process_name "
              "OR sessions.process_name LIKE '%\\' || ia.process_name) "
              "GROUP BY app_name ORDER BY total_seconds DESC");
    q.bindValue(":threshold", threshold);
    q.addBindValue(QDate::currentDate().toString(Qt::ISODate));
    q.exec();
    QVector<QVariantMap> results;
    while (q.next()) {
        QVariantMap row;
        row["app_name"] = q.value("app_name");
        row["total_seconds"] = q.value("total_seconds");
        results.append(row);
    }
    return results;
}
```

- [ ] **Step 2: 修改 `getTodayTotal()`**

在第 138-142 行的 SQL 中添加 `AND duration_seconds >= :threshold`：

```cpp
int DatabaseManager::getTodayTotal()
{
    QMutexLocker lock(&m_mutex);
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QSqlQuery q(m_db);
    q.prepare("SELECT COALESCE(SUM(duration_seconds), 0) as total "
              "FROM sessions WHERE date(start_time) = ? "
              "AND duration_seconds >= :threshold "
              "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
              "WHERE sessions.process_name = ia.process_name "
              "OR sessions.process_name LIKE '%\\' || ia.process_name)");
    q.bindValue(":threshold", threshold);
    q.addBindValue(QDate::currentDate().toString(Qt::ISODate));
    q.exec();
    if (q.next())
        return q.value("total").toInt();
    return 0;
}
```

- [ ] **Step 3: 修改 `getYesterdayTotal()`**

在第 154-158 行的 SQL 中添加 `AND duration_seconds >= :threshold`：

```cpp
int DatabaseManager::getYesterdayTotal()
{
    QMutexLocker lock(&m_mutex);
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QSqlQuery q(m_db);
    q.prepare("SELECT COALESCE(SUM(duration_seconds), 0) as total "
              "FROM sessions WHERE date(start_time) = ? "
              "AND duration_seconds >= :threshold "
              "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
              "WHERE sessions.process_name = ia.process_name "
              "OR sessions.process_name LIKE '%\\' || ia.process_name)");
    q.bindValue(":threshold", threshold);
    q.addBindValue(QDate::currentDate().addDays(-1).toString(Qt::ISODate));
    q.exec();
    if (q.next())
        return q.value("total").toInt();
    return 0;
}
```

- [ ] **Step 4: 修改 `getWeekSummary()`**

在第 172-177 行的 SQL 中添加 `AND duration_seconds >= :threshold`：

```cpp
QVector<QVariantMap> DatabaseManager::getWeekSummary()
{
    QMutexLocker lock(&m_mutex);
    QDate today = QDate::currentDate();
    QDate monday = today.addDays(-today.dayOfWeek() + 1);
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QSqlQuery q(m_db);
    q.prepare("SELECT date(start_time) as d, SUM(duration_seconds) as total_seconds "
              "FROM sessions WHERE date(start_time) >= ? AND date(start_time) <= ? "
              "AND duration_seconds >= :threshold "
              "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
              "WHERE sessions.process_name = ia.process_name "
              "OR sessions.process_name LIKE '%\\' || ia.process_name) "
              "GROUP BY date(start_time) ORDER BY d ASC");
    q.bindValue(":threshold", threshold);
    q.addBindValue(monday.toString(Qt::ISODate));
    q.addBindValue(today.toString(Qt::ISODate));
    q.exec();
    QVector<QVariantMap> results;
    while (q.next()) {
        QVariantMap row;
        row["d"] = q.value("d");
        row["total_seconds"] = q.value("total_seconds");
        results.append(row);
    }
    return results;
}
```

- [ ] **Step 5: 修改 `getAppRank()` — 注意：主查询和子查询都需添加过滤**

在第 195-209 行的 SQL 中，**主查询 (`s1`) 和子查询 (`s2`) 两处**都需要添加 `AND duration_seconds >= :threshold`：

```cpp
QVector<QVariantMap> DatabaseManager::getAppRank(const QDate &targetDate)
{
    QMutexLocker lock(&m_mutex);
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QSqlQuery q(m_db);
    q.prepare(
        "SELECT app_name, "
        "  (SELECT s2.process_name FROM sessions s2 "
        "   WHERE s2.app_name = s1.app_name AND date(s2.start_time) = ? "
        "   AND s2.duration_seconds >= :threshold "
        "   AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
        "   WHERE s2.process_name = ia.process_name "
        "   OR s2.process_name LIKE '%\\' || ia.process_name) "
        "   GROUP BY s2.process_name "
        "   ORDER BY SUM(s2.duration_seconds) DESC LIMIT 1) as process_name, "
        "  SUM(s1.duration_seconds) as total_seconds "
        "FROM sessions s1 WHERE date(s1.start_time) = ? "
        "AND s1.duration_seconds >= :threshold "
        "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
        "WHERE s1.process_name = ia.process_name "
        "OR s1.process_name LIKE '%\\' || ia.process_name) "
        "GROUP BY s1.app_name ORDER BY total_seconds DESC");
    q.bindValue(":threshold", threshold);
    q.addBindValue(targetDate.toString(Qt::ISODate));
    q.addBindValue(targetDate.toString(Qt::ISODate));
    q.exec();
    QVector<QVariantMap> results;
    while (q.next()) {
        QVariantMap row;
        row["app_name"] = q.value("app_name");
        row["process_name"] = q.value("process_name");
        row["total_seconds"] = q.value("total_seconds");
        results.append(row);
    }
    return results;
}
```

- [ ] **Step 6: 修改 `getAllSessions()` — 两个分支都需添加**

在第 228-241 行的两个 SQL 分支中都添加 `AND duration_seconds >= :threshold`：

```cpp
QVector<QVariantMap> DatabaseManager::getAllSessions(const QString &startDate, const QString &endDate)
{
    QMutexLocker lock(&m_mutex);
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QSqlQuery q(m_db);
    if (!startDate.isEmpty() && !endDate.isEmpty()) {
        q.prepare("SELECT * FROM sessions WHERE date(start_time) >= ? AND date(start_time) <= ? "
                  "AND duration_seconds >= :threshold "
                  "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
                  "WHERE sessions.process_name = ia.process_name "
                  "OR sessions.process_name LIKE '%\\' || ia.process_name) "
                  "ORDER BY start_time ASC");
        q.bindValue(":threshold", threshold);
        q.addBindValue(startDate);
        q.addBindValue(endDate);
    } else {
        q.prepare("SELECT * FROM sessions "
                  "WHERE duration_seconds >= :threshold "
                  "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
                  "WHERE sessions.process_name = ia.process_name "
                  "OR sessions.process_name LIKE '%\\' || ia.process_name) "
                  "ORDER BY start_time ASC");
        q.bindValue(":threshold", threshold);
    }
    q.exec();
    QVector<QVariantMap> results;
    while (q.next()) {
        QVariantMap row;
        row["id"] = q.value("id");
        row["process_name"] = q.value("process_name");
        row["window_title"] = q.value("window_title");
        row["app_name"] = q.value("app_name");
        row["start_time"] = q.value("start_time");
        row["end_time"] = q.value("end_time");
        row["duration_seconds"] = q.value("duration_seconds");
        results.append(row);
    }
    return results;
}
```

- [ ] **Step 7: 修改 `getDailySummaries()` — 两个分支都需添加**

在第 263-271、273-277 行的两个 SQL 分支中都添加 `AND duration_seconds >= :threshold`：

```cpp
QVector<QVariantMap> DatabaseManager::getDailySummaries(const QString &startDate, const QString &endDate)
{
    QMutexLocker lock(&m_mutex);
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QSqlQuery q(m_db);
    if (!startDate.isEmpty() && !endDate.isEmpty()) {
        q.prepare("SELECT date(start_time) as d, app_name, SUM(duration_seconds) as total_seconds "
                  "FROM sessions WHERE date(start_time) >= ? AND date(start_time) <= ? "
                  "AND duration_seconds >= :threshold "
                  "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
                  "WHERE sessions.process_name = ia.process_name "
                  "OR sessions.process_name LIKE '%\\' || ia.process_name) "
                  "GROUP BY d, app_name ORDER BY d ASC, total_seconds DESC");
        q.bindValue(":threshold", threshold);
        q.addBindValue(startDate);
        q.addBindValue(endDate);
    } else {
        q.prepare("SELECT date(start_time) as d, app_name, SUM(duration_seconds) as total_seconds "
                  "FROM sessions WHERE NOT EXISTS (SELECT 1 FROM ignored_apps ia "
                  "WHERE sessions.process_name = ia.process_name "
                  "OR sessions.process_name LIKE '%\\' || ia.process_name) "
                  "AND duration_seconds >= :threshold "
                  "GROUP BY d, app_name ORDER BY d ASC, total_seconds DESC");
        q.bindValue(":threshold", threshold);
    }
    q.exec();
    QVector<QVariantMap> results;
    while (q.next()) {
        QVariantMap row;
        row["d"] = q.value("d");
        row["app_name"] = q.value("app_name");
        row["total_seconds"] = q.value("total_seconds");
        results.append(row);
    }
    return results;
}
```

- [ ] **Step 8: 构建并运行测试验证**

在项目根目录执行：

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

预期：构建成功，无编译错误。

- [ ] **Step 9: 提交**

```bash
git add src/database/database_manager.cpp
git commit -m "feat: add min_record_threshold filter to all query methods"
```

---

### Task 2: 追踪设置页添加最低记录阈值 SpinBox

**Files:**
- Modify: `src/ui/settings_dialog.h:48-50`（添加成员变量声明）
- Modify: `src/ui/settings_dialog.cpp:184-194`（添加 SpinBox UI）
- Modify: `src/ui/settings_dialog.cpp:235-245`（loadSettings 读取设置）
- Modify: `src/ui/settings_dialog.cpp:247-255`（saveSettings 保存设置）

**Interfaces:**
- Consumes: `m_db->getSetting("min_record_threshold", "40")` / `m_db->setSetting(...)`
- Produces: SpinBox 最小值为 0，最大值为 300，步长为 5

- [ ] **Step 1: 在 `settings_dialog.h` 中添加成员变量 `m_minRecordThreshold`**

在第 50 行（`m_minTrackingSeconds` 声明之后）添加：

```cpp
QSpinBox *m_minRecordThreshold;
```

- [ ] **Step 2: 在 `settings_dialog.cpp` 的追踪设置 Tab 中添加 UI 控件**

在第 192 行（`trackLayout->addLayout(minTrackRow)` 之后，`trackLayout->addStretch()` 之前）添加：

```cpp
QHBoxLayout *minRecordRow = new QHBoxLayout();
minRecordRow->addWidget(new QLabel(QString::fromUtf8("\xe6\x9c\x80\xe4\xbd\x8e\xe8\xae\xb0\xe5\xbd\x95\xe9\x98\x88\xe5\x80\xbc\xef\xbc\x88\xe7\xa7\x92\xef\xbc\x89:"), this));
m_minRecordThreshold = new QSpinBox(this);
m_minRecordThreshold->setRange(0, 300);
m_minRecordThreshold->setValue(40);
m_minRecordThreshold->setSingleStep(5);
m_minRecordThreshold->setSuffix(QString::fromUtf8(" \xe7\xa7\x92"));
m_minRecordThreshold->setToolTip(QString::fromUtf8("\xe5\x8d\x95\xe6\xac\xa1\xe4\xbd\xbf\xe7\x94\xa8\xe6\x97\xb6\xe9\x95\xbf\xe4\xbd\x8e\xe4\xba\x8e\xe6\xad\xa4\xe5\x80\xbc\xe7\x9a\x84\xe8\xae\xb0\xe5\xbd\x95\xe5\xb0\x86\xe4\xb8\x8d\xe8\xae\xa1\xe5\x85\xa5\xe7\xbb\x9f\xe8\xae\xa1\xe5\x92\x8c\xe5\xaf\xbc\xe5\x87\xba\xef\xbc\x8c" "0\xe4\xb8\xba\xe4\xb8\x8d\xe9\x99\x90\xe5\x88\xb6"));
minRecordRow->addWidget(m_minRecordThreshold);
minRecordRow->addStretch();
trackLayout->addLayout(minRecordRow);
```

- [ ] **Step 3: 在 `loadSettings()` 中读取新设置**

在第 240 行（`m_minTrackingSeconds` 读取之后）添加：

```cpp
m_minRecordThreshold->setValue(m_db->getSetting("min_record_threshold", "40").toInt());
```

- [ ] **Step 4: 在 `saveSettings()` 中保存新设置**

在第 252 行（`m_minTrackingSeconds` 保存之后）添加：

```cpp
m_db->setSetting("min_record_threshold", QString::number(m_minRecordThreshold->value()));
```

- [ ] **Step 5: 构建验证**

在项目根目录执行：

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

预期：构建成功，无编译错误。

- [ ] **Step 6: 运行测试**

```powershell
$env:QT_PLUGIN_PATH = "D:\AICOP\requirements\QT6\6.11.1\mingw_64\plugins"
.\build\tests\test_database.exe
.\build\tests\test_exporter.exe
```

预期：所有测试通过。

- [ ] **Step 7: 提交**

```bash
git add src/ui/settings_dialog.h src/ui/settings_dialog.cpp
git commit -m "feat: add min_record_threshold SpinBox to tracking settings"
```

---

### Task 3: Tracker 层读取新设置（一致性）

**Files:**
- Modify: `src/tracker/window_tracker.cpp:220-234`

**Interfaces:**
- Consumes: `m_db->getSetting("min_record_threshold", "40")`
- Produces: 无（tracker 不直接使用此值，仅保持 reloadSettings 与设置项同步）

- [ ] **Step 1: 在 `reloadSettings()` 中读取新设置**

在 `window_tracker.cpp` 第 226 行（`m_minTrackingSeconds` 读取之后）添加：

```cpp
m_db->getSetting("min_record_threshold", "40");
```

- [ ] **Step 2: 构建验证**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

- [ ] **Step 3: 提交**

```bash
git add src/tracker/window_tracker.cpp
git commit -m "chore: read min_record_threshold in tracker reloadSettings"
```
