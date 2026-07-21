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

