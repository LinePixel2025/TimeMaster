# 代码审查缺陷修复实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复代码审查中发现的全部 12 个缺陷（2 严重 + 5 中等 + 5 低）

**Architecture:** 直接修改现有源文件，无新增文件。修改集中在 6 个文件中（xlsx_writer、window_tracker、stats_widget、database_manager、main_window、main、exporter、app_rank_widget、app_icon_provider）

**Tech Stack:** C++17, Qt6 Widgets+Sql, CMake + Ninja (MinGW)

## 全局约束

- 字体约定：Windows 上必须使用 `Microsoft YaHei`，禁止 `PingFang SC`
- 数据库线程安全：所有访问 `m_db` 的方法必须获取 `QMutexLocker lock(&m_mutex)`
- 构建：`cmake --preset mingw && cmake --build build`
- 测试：`$env:QT_PLUGIN_PATH = "D:\AICOP\requirements\QT6\6.11.1\mingw_64\plugins"; .\build\tests\test_database.exe; .\build\tests\test_exporter.exe`
- 背景：纯色 `#F0F2F5`，禁止 MICA/透明背景

---

### Task 1: 修复 XLSX 导出 sharedStrings 构建顺序（严重）

**Files:**
- Modify: `src/export/xlsx_writer.h:32-34`
- Modify: `src/export/xlsx_writer.cpp:117-128, 140-195, 197-233`

**Interfaces:**
- Consumes: `XlsxWriter::save()`, `XlsxWriter::buildSheet()`, `XlsxWriter::buildSharedStrings()`
- Produces: sharedStrings.xml 在 sheet XML 之后构建，包含所有字符串

- [ ] **Step 1: 修改 `buildSharedStrings()` 使 buildSheet 先收集所有字符串**

在 `save()` 中将 `buildSharedStrings()` 的调用从 `addFiles` 初始化列表中移到所有 `buildSheet()` 循环之后。将变量 `addFiles` 拆分为显式调用：

修改 `src/export/xlsx_writer.cpp:210-218`，将：
```cpp
    auto addFiles = {addFile("[Content_Types].xml", buildContentTypes()),
                     addFile("_rels/.rels", buildRels()),
                     addFile("xl/workbook.xml", buildWorkbook()),
                     addFile("xl/_rels/workbook.xml.rels", buildWorkbookRels()),
                     addFile("xl/sharedStrings.xml", buildSharedStrings())};

    for (bool ok : addFiles) {
        if (!ok) { qWarning() << "Failed to add file to XLSX ZIP"; return false; }
    }

    for (int i = 0; i < m_sheets.size(); ++i) {
        QByteArray name = QByteArray("xl/worksheets/sheet") + QByteArray::number(i + 1) + ".xml";
        if (!addFile(name.constData(), buildSheet(m_sheets[i], i)))
            return false;
    }
```

改为：
```cpp
    if (!addFile("[Content_Types].xml", buildContentTypes())) return false;
    if (!addFile("_rels/.rels", buildRels())) return false;
    if (!addFile("xl/workbook.xml", buildWorkbook())) return false;
    if (!addFile("xl/_rels/workbook.xml.rels", buildWorkbookRels())) return false;

    for (int i = 0; i < m_sheets.size(); ++i) {
        QByteArray name = QByteArray("xl/worksheets/sheet") + QByteArray::number(i + 1) + ".xml";
        if (!addFile(name.constData(), buildSheet(m_sheets[i], i)))
            return false;
    }

    if (!addFile("xl/sharedStrings.xml", buildSharedStrings())) return false;
```

- [ ] **Step 2: 构建和运行测试验证**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --preset mingw; if ($?) { cmake --build build }
$env:QT_PLUGIN_PATH = "D:\AICOP\requirements\QT6\6.11.1\mingw_64\plugins"
.\build\tests\test_exporter.exe
```

- [ ] **Step 3: 提交**

```bash
git add src/export/xlsx_writer.cpp
git commit -m "fix: XLSX sharedStrings 在 sheet XML 之后构建以确保字符串表完整"
```

---

### Task 2: 修复轮询间隔设置被忽略（严重）

**Files:**
- Modify: `src/tracker/window_tracker.cpp:83`

**Interfaces:**
- Consumes: `WindowTracker::m_pollInterval` (已在 `reloadSettings()` 中加载并受 `m_settingsMutex` 保护)
- Produces: 追踪循环按用户配置的间隔执行

- [ ] **Step 1: 将硬编码 `POLL_INTERVAL` 替换为 `m_pollInterval`**

修改 `src/tracker/window_tracker.cpp:83-84`：
```cpp
        for (int i = 0; i < 5 && m_running.loadRelaxed(); ++i)
            msleep(static_cast<unsigned long>(POLL_INTERVAL * 200));
```
改为：
```cpp
        for (int i = 0; i < 5 && m_running.loadRelaxed(); ++i)
            msleep(static_cast<unsigned long>(m_pollInterval * 200));
```

- [ ] **Step 2: 构建验证**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

- [ ] **Step 3: 提交**

```bash
git add src/tracker/window_tracker.cpp
git commit -m "fix: 追踪循环使用 m_pollInterval 替代硬编码 POLL_INTERVAL"
```

---

### Task 3: 修复 PingFang SC 字体为 Microsoft YaHei（中等）

**Files:**
- Modify: `src/ui/stats_widget.cpp:280, 291, 304, 318`

**Interfaces:**
- Consumes: 本地 `appFont()` 辅助函数（已在第 14-19 行定义）
- Produces: 所有 UI 文本统一使用 `Microsoft YaHei` 字体

- [ ] **Step 1: 替换 4 处 PingFang SC 用法**

`src/ui/stats_widget.cpp:280`:
```cpp
    header->setFont(QFont("PingFang SC", 18, QFont::Medium));
```
改为：
```cpp
    header->setFont(appFont(18, QFont::Medium));
```

`src/ui/stats_widget.cpp:291`:
```cpp
    todayTitle->setFont(QFont("PingFang SC", 12, QFont::Normal));
```
改为：
```cpp
    todayTitle->setFont(appFont(12));
```

`src/ui/stats_widget.cpp:304`:
```cpp
    topAppTitle->setFont(QFont("PingFang SC", 12, QFont::Normal));
```
改为：
```cpp
    topAppTitle->setFont(appFont(12));
```

`src/ui/stats_widget.cpp:318`:
```cpp
    weekTitle->setFont(QFont("PingFang SC", 12, QFont::Normal));
```
改为：
```cpp
    weekTitle->setFont(appFont(12));
```

- [ ] **Step 2: 构建验证**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

- [ ] **Step 3: 提交**

```bash
git add src/ui/stats_widget.cpp
git commit -m "fix: 将 PingFang SC 字体替换为 Microsoft YaHei（使用 appFont()）"
```

---

### Task 4: DatabaseManager::close() 防重入 + addIgnoredApp 返回值（中等）

**Files:**
- Modify: `src/database/database_manager.h:56`
- Modify: `src/database/database_manager.cpp:281-292, 360-369`

**Interfaces:**
- Consumes: `DatabaseManager::close()`, `DatabaseManager::addIgnoredApp()`
- Produces: close() 可安全多次调用，addIgnoredApp() 重复插入时返回已有行 ID 而非 0

- [ ] **Step 1: 添加 `m_closed` 标志到头文件**

在 `src/database/database_manager.h:56`（`QMutex m_mutex;` 之后）添加：
```cpp
    bool m_closed = false;
```

- [ ] **Step 2: 在 close() 中添加防重入保护**

修改 `src/database/database_manager.cpp:360-369`：
```cpp
void DatabaseManager::close()
{
    QMutexLocker lock(&m_mutex);
    if (m_closed) return;
    m_closed = true;
    QString connName = m_db.connectionName();
    if (m_db.isOpen())
        m_db.close();
    m_db = QSqlDatabase();
    if (!connName.isEmpty())
        QSqlDatabase::removeDatabase(connName);
}
```

- [ ] **Step 3: 修复 addIgnoredApp 重复插入返回值**

修改 `src/database/database_manager.cpp:281-292`：
```cpp
int DatabaseManager::addIgnoredApp(const QString &processName)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM ignored_apps WHERE process_name = ?");
    q.addBindValue(processName);
    q.exec();
    if (q.next())
        return q.value("id").toInt();
    q.prepare("INSERT INTO ignored_apps (process_name) VALUES (?)");
    q.addBindValue(processName);
    if (!q.exec()) {
        qWarning() << "addIgnoredApp failed:" << q.lastError();
        return -1;
    }
    return q.lastInsertId().toInt();
}
```

- [ ] **Step 4: 构建和运行测试验证**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
$env:QT_PLUGIN_PATH = "D:\AICOP\requirements\QT6\6.11.1\mingw_64\plugins"
.\build\tests\test_database.exe
```

- [ ] **Step 5: 提交**

```bash
git add src/database/database_manager.h src/database/database_manager.cpp
git commit -m "fix: close() 防重入保护；addIgnoredApp 重复时返回已有 ID"
```

---

### Task 5: 修复其余中低严重性问题

本任务处理：线程等待超时、DWM 框架清理、追踪禁用时 pending 清理、导出范围一致性、AppRankWidget 布局、QMetaType 类型检查

**Files:**
- Modify: `src/main.cpp:52`
- Modify: `src/ui/main_window.cpp:137-138`
- Modify: `src/tracker/window_tracker.cpp:105-107`
- Modify: `src/export/exporter.cpp:87-97`
- Modify: `src/ui/main_window.cpp:118`
- Modify: `src/export/xlsx_writer.cpp:175-179`

**Interfaces:**
- Consumes: 各模块现有接口
- Produces: 行为更健壮、一致

- [ ] **Step 1: 增加线程退出等待超时**

`src/main.cpp:52`:
```cpp
        tracker.wait(3000);
```
改为：
```cpp
        tracker.wait(10000);
```

- [ ] **Step 2: 移除 DWM 框架扩展残留调用**

`src/ui/main_window.cpp:137-138`，删除这两行：
```cpp
    MARGINS margins = {0, 0, 0, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
```

同时删除文件顶部的 `#include <dwmapi.h>`（第 21 行）和 `#pragma comment(lib, "dwmapi.lib")`（第 22 行），仅当确认 `DWMWA_USE_IMMERSIVE_DARK_MODE` 和 `DWMWA_CAPTION_COLOR` 的定义不需要 dwmapi.h 时... 实际上这两个常量来自 `dwmapi.h`，`DwmSetWindowAttribute` 函数也来自 dwmapi。所以只删除第 137-138 行即可，保留 include 和 pragma。

- [ ] **Step 3: 追踪禁用时清理 pending 状态**

`src/tracker/window_tracker.cpp:105-107`：
```cpp
    if (!trackingEnabled)
        return;
```
改为：
```cpp
    if (!trackingEnabled) {
        m_pendingPid = 0;
        return;
    }
```

- [ ] **Step 4: 统一 Excel 导出每日汇总范围（导出全部数据）**

`src/export/exporter.cpp:87-88`，将硬编码的周一-今天范围改为空参数以查询全部数据：
```cpp
    QDate today = QDate::currentDate();
    QDate monday = today.addDays(-today.dayOfWeek() + 1);
    xlsx.addSheet(QString::fromUtf8("\xe6\xaf\x8f\xe6\x97\xa5\xe6\xb1\x87\xe6\x80\xbb"));
```
改为：
```cpp
    xlsx.addSheet(QString::fromUtf8("\xe6\xaf\x8f\xe6\x97\xa5\xe6\xb1\x87\xe6\x80\xbb"));
```

`src/export/exporter.cpp:97`:
```cpp
    auto summaries = m_db->getDailySummaries(monday.toString(Qt::ISODate), today.toString(Qt::ISODate));
```
改为：
```cpp
    auto summaries = m_db->getDailySummaries();
```

- [ ] **Step 5: 修复 AppRankWidget 宽度布局**

`src/ui/main_window.cpp:118`，删除多余的 stretch：
```cpp
    rankRow->addStretch(1);
```
改为删除此行。

- [ ] **Step 6: 增强 QMetaType 数值类型检测**

`src/export/xlsx_writer.cpp:175-179`：
```cpp
            bool isNumeric = false;
            double numVal = 0;
            if (val.typeId() == QMetaType::Int || val.typeId() == QMetaType::Double) {
                isNumeric = true;
                numVal = val.toDouble();
            }
```
改为：
```cpp
            bool isNumeric = false;
            double numVal = 0;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            int typeId = val.typeId();
            if (typeId == QMetaType::Int || typeId == QMetaType::UInt ||
                typeId == QMetaType::LongLong || typeId == QMetaType::ULongLong ||
                typeId == QMetaType::Double || typeId == QMetaType::Float) {
                isNumeric = true;
                numVal = val.toDouble();
            }
#else
            if (val.type() == QVariant::Int || val.type() == QVariant::UInt ||
                val.type() == QVariant::LongLong || val.type() == QVariant::ULongLong ||
                val.type() == QVariant::Double) {
                isNumeric = true;
                numVal = val.toDouble();
            }
#endif
```

- [ ] **Step 7: 构建验证所有修改**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

- [ ] **Step 8: 运行测试**

```powershell
$env:QT_PLUGIN_PATH = "D:\AICOP\requirements\QT6\6.11.1\mingw_64\plugins"
.\build\tests\test_database.exe
.\build\tests\test_exporter.exe
```

- [ ] **Step 9: 提交**

```bash
git add src/main.cpp src/ui/main_window.cpp src/tracker/window_tracker.cpp src/export/exporter.cpp src/export/xlsx_writer.cpp
git commit -m "fix: 线程超时、DWM 清理、pending 清理、导出一致性、布局、类型检测等修复"
```
