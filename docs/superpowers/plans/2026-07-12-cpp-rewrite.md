# Time Master C++ Rewrite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rewrite the Python/PySide6 Time Master desktop app in C++ with Qt6, preserving all features.

**Architecture:** Direct port of Python modules to C++ classes. WindowTracker runs in a QThread polling Win32 API. DatabaseManager wraps QSqlDatabase (SQLite) with QMutex for thread safety. Custom QPainter widgets for stats/ranking UI. Minimal hand-written XlsxWriter for Excel export.

**Tech Stack:** C++17, Qt6 (Widgets + Sql), CMake 3.22+, vcpkg (USTC mirror), Win32 API, miniz (bundled ZIP library)

## Global Constraints

- Windows-only (same as Python original)
- Database path: `%APPDATA%/Time Master/data.db`
- vcpkg USTC mirror via `X_VCPKG_ASSET_SOURCES=x-azurl,https://mirrors.ustc.edu.cn/vcpkg/`
- No Python, no PySide6, no psutil, no openpyxl in the C++ version
- Same SQLite schema as Python version
- Same APP_NAME_MAP with 40+ app mappings
- Same visual design (gradients, glassmorphism, dark theme, PingFang SC font)

---

### Task 1: Project Scaffold (CMake + vcpkg + directory structure)

**Files:**
- Create: `CMakeLists.txt`
- Create: `vcpkg.json`
- Create: `CMakePresets.json`
- Create: `src/main.cpp` (stub)
- Create: `resources/resources.qrc`
- Create: `third_party/miniz/miniz.h`
- Create: `third_party/miniz/miniz.c`

**Interfaces:**
- Consumes: (none)
- Produces: Build system that finds Qt6, compiles C++17, links Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql

- [ ] **Step 1: Create root CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.22)
project(TimeMaster VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)

add_subdirectory(src)
```

- [ ] **Step 2: Create vcpkg.json**

```json
{
  "name": "timemaster",
  "version": "1.0.0",
  "dependencies": [
    "qt6-base",
    "qt6-tools"
  ]
}
```

- [ ] **Step 3: Create CMakePresets.json**

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "default",
      "displayName": "Windows x64 (USTC mirror)",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "binaryDir": "${sourceDir}/build",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
        "X_VCPKG_ASSET_SOURCES": "x-azurl,https://mirrors.ustc.edu.cn/vcpkg/"
      }
    }
  ]
}
```

- [ ] **Step 4: Create src/CMakeLists.txt**

```cmake
set(SOURCES
    main.cpp
    database/database_manager.cpp
    tracker/window_tracker.cpp
    ui/main_window.cpp
    ui/stats_widget.cpp
    ui/app_rank_widget.cpp
    ui/tray_manager.cpp
    export/exporter.cpp
    export/xlsx_writer.cpp
    ${CMAKE_SOURCE_DIR}/third_party/miniz/miniz.c
)

set(HEADERS
    database/database_manager.h
    tracker/window_tracker.h
    ui/main_window.h
    ui/stats_widget.h
    ui/app_rank_widget.h
    ui/tray_manager.h
    export/exporter.h
    export/xlsx_writer.h
)

qt6_add_resources(RESOURCES ${CMAKE_SOURCE_DIR}/resources/resources.qrc)

add_executable(TimeMaster WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})

target_include_directories(TimeMaster PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(TimeMaster PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
)
```

- [ ] **Step 5: Create stub main.cpp**

```cpp
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Time Master");
    app.setOrganizationName("TimeMaster");
    return app.exec();
}
```

- [ ] **Step 6: Create resources.qrc**

```xml
<RCC>
    <qresource prefix="/">
        <file>icon.ico</file>
        <file>icon.png</file>
    </qresource>
</RCC>
```

- [ ] **Step 7: Create directories and third_party/miniz**

```bash
New-Item -ItemType Directory -Path src/database, src/tracker, src/ui, src/export, third_party/miniz, tests -Force
```

Download miniz single-file library (miniz.h + miniz.c) from https://raw.githubusercontent.com/richgel999/miniz/master/miniz.h and https://raw.githubusercontent.com/richgel999/miniz/master/miniz.c into `third_party/miniz/`.

```cmake
# Add to src/CMakeLists.txt
target_include_directories(TimeMaster PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
```

- [ ] **Step 8: Verify build compiles**

```bash
cmake --preset default
cmake --build build
```

Expected: EXE builds and links. Runs (shows empty window).

- [ ] **Step 9: Commit**

```bash
git add -A
git commit -m "feat: project scaffold with CMake + vcpkg + Qt6"
```

---

### Task 2: DatabaseManager

**Files:**
- Create: `src/database/database_manager.h`
- Create: `src/database/database_manager.cpp`
- Create: `tests/test_database.cpp`
- Create: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Qt6::Sql
- Produces: `DatabaseManager` class with all DB operations

- [ ] **Step 1: Write database_manager.h**

```cpp
#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <QString>
#include <QSqlDatabase>
#include <QMutex>
#include <QDateTime>
#include <QDate>
#include <QVariantMap>
#include <QVector>

class DatabaseManager
{
public:
    explicit DatabaseManager(const QString &dbPath = QString());
    ~DatabaseManager();

    qint64 insertSession(const QString &processName, const QString &windowTitle,
                         const QString &appName, const QDateTime &startTime,
                         const QDateTime &endTime, int durationSeconds);
    void updateSessionEnd(qint64 sessionId, const QDateTime &endTime, int durationSeconds);

    QVector<QVariantMap> getTodaySummary();
    int getTodayTotal();
    QVector<QVariantMap> getWeekSummary();
    QVector<QVariantMap> getAppRank(const QDate &targetDate = QDate::currentDate());
    QVector<QVariantMap> getAllSessions(const QString &startDate = QString(),
                                        const QString &endDate = QString());
    QVector<QVariantMap> getDailySummaries(const QString &startDate = QString(),
                                           const QString &endDate = QString());

    void close();

private:
    void migrate();
    QSqlDatabase m_db;
    QMutex m_mutex;
    QString m_dbPath;
};

#endif // DATABASE_MANAGER_H
```

- [ ] **Step 2: Write database_manager.cpp**

```cpp
#include "database_manager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QUuid>

DatabaseManager::DatabaseManager(const QString &dbPath)
    : m_dbPath(dbPath)
{
    if (m_dbPath.isEmpty()) {
        QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(appData);
        m_dbPath = appData + "/data.db";
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", QUuid::createUuid().toString());
    m_db.setDatabaseName(m_dbPath);
    m_db.open();
    migrate();
}

DatabaseManager::~DatabaseManager()
{
    close();
}

void DatabaseManager::migrate()
{
    QSqlQuery q(m_db);
    q.exec(
        "CREATE TABLE IF NOT EXISTS sessions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "process_name TEXT NOT NULL, "
        "window_title TEXT NOT NULL DEFAULT '', "
        "app_name TEXT NOT NULL, "
        "start_time TEXT NOT NULL, "
        "end_time TEXT, "
        "duration_seconds INTEGER DEFAULT 0)"
    );
    q.exec("CREATE INDEX IF NOT EXISTS idx_sessions_start ON sessions(start_time)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_sessions_app ON sessions(app_name)");
}

qint64 DatabaseManager::insertSession(const QString &processName, const QString &windowTitle,
                                       const QString &appName, const QDateTime &startTime,
                                       const QDateTime &endTime, int durationSeconds)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO sessions (process_name, window_title, app_name, start_time, end_time, duration_seconds) "
              "VALUES (?, ?, ?, ?, ?, ?)");
    q.addBindValue(processName);
    q.addBindValue(windowTitle);
    q.addBindValue(appName);
    q.addBindValue(startTime.toString(Qt::ISODate));
    q.addBindValue(endTime.isValid() ? endTime.toString(Qt::ISODate) : QVariant());
    q.addBindValue(durationSeconds);
    q.exec();
    return q.lastInsertId().toLongLong();
}

void DatabaseManager::updateSessionEnd(qint64 sessionId, const QDateTime &endTime, int durationSeconds)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("UPDATE sessions SET end_time=?, duration_seconds=? WHERE id=?");
    q.addBindValue(endTime.toString(Qt::ISODate));
    q.addBindValue(durationSeconds);
    q.addBindValue(sessionId);
    q.exec();
}

QVector<QVariantMap> DatabaseManager::getTodaySummary()
{
    QSqlQuery q(m_db);
    q.prepare("SELECT app_name, SUM(duration_seconds) as total_seconds "
              "FROM sessions WHERE date(start_time) = ? "
              "GROUP BY app_name ORDER BY total_seconds DESC");
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

int DatabaseManager::getTodayTotal()
{
    QSqlQuery q(m_db);
    q.prepare("SELECT COALESCE(SUM(duration_seconds), 0) as total "
              "FROM sessions WHERE date(start_time) = ?");
    q.addBindValue(QDate::currentDate().toString(Qt::ISODate));
    q.exec();
    if (q.next())
        return q.value("total").toInt();
    return 0;
}

QVector<QVariantMap> DatabaseManager::getWeekSummary()
{
    QDate today = QDate::currentDate();
    QDate monday = today.addDays(-today.dayOfWeek() + 1);
    QSqlQuery q(m_db);
    q.prepare("SELECT date(start_time) as d, SUM(duration_seconds) as total_seconds "
              "FROM sessions WHERE date(start_time) >= ? AND date(start_time) <= ? "
              "GROUP BY date(start_time) ORDER BY d ASC");
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

QVector<QVariantMap> DatabaseManager::getAppRank(const QDate &targetDate)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT app_name, SUM(duration_seconds) as total_seconds "
              "FROM sessions WHERE date(start_time) = ? "
              "GROUP BY app_name ORDER BY total_seconds DESC");
    q.addBindValue(targetDate.toString(Qt::ISODate));
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

QVector<QVariantMap> DatabaseManager::getAllSessions(const QString &startDate, const QString &endDate)
{
    QSqlQuery q(m_db);
    if (!startDate.isEmpty() && !endDate.isEmpty()) {
        q.prepare("SELECT * FROM sessions WHERE date(start_time) >= ? AND date(start_time) <= ? "
                  "ORDER BY start_time ASC");
        q.addBindValue(startDate);
        q.addBindValue(endDate);
    } else {
        q.prepare("SELECT * FROM sessions ORDER BY start_time ASC");
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

QVector<QVariantMap> DatabaseManager::getDailySummaries(const QString &startDate, const QString &endDate)
{
    QSqlQuery q(m_db);
    if (!startDate.isEmpty() && !endDate.isEmpty()) {
        q.prepare("SELECT date(start_time) as d, app_name, SUM(duration_seconds) as total_seconds "
                  "FROM sessions WHERE date(start_time) >= ? AND date(start_time) <= ? "
                  "GROUP BY d, app_name ORDER BY d ASC, total_seconds DESC");
        q.addBindValue(startDate);
        q.addBindValue(endDate);
    } else {
        q.prepare("SELECT date(start_time) as d, app_name, SUM(duration_seconds) as total_seconds "
                  "FROM sessions GROUP BY d, app_name ORDER BY d ASC, total_seconds DESC");
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

void DatabaseManager::close()
{
    QMutexLocker lock(&m_mutex);
    if (m_db.isOpen())
        m_db.close();
    m_db = QSqlDatabase();
    QString connName = m_db.connectionName();
    if (!connName.isEmpty())
        QSqlDatabase::removeDatabase(connName);
}
```

- [ ] **Step 3: Write test_database.cpp**

```cpp
#include <cassert>
#include <iostream>
#include <QTemporaryFile>
#include <QDateTime>
#include "database/database_manager.h"

void test_create_table()
{
    QTemporaryFile tmpFile;
    tmpFile.open();
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    QVector<QVariantMap> sessions = db.getAllSessions();
    assert(sessions.isEmpty());
    std::cout << "test_create_table PASS" << std::endl;
}

void test_insert_and_query()
{
    QTemporaryFile tmpFile;
    tmpFile.open();
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    QDateTime now = QDateTime::currentDateTime();
    qint64 sid = db.insertSession("chrome.exe", "Google", "Chrome", now, now, 60);
    assert(sid > 0);

    QVector<QVariantMap> summary = db.getTodaySummary();
    assert(summary.size() == 1);
    assert(summary[0]["app_name"].toString() == "Chrome");
    assert(summary[0]["total_seconds"].toInt() == 60);
    std::cout << "test_insert_and_query PASS" << std::endl;
}

void test_week_summary()
{
    QTemporaryFile tmpFile;
    tmpFile.open();
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    QDateTime now = QDateTime::currentDateTime();
    db.insertSession("Code.exe", "test.py", "VS Code", now, now, 120);
    QVector<QVariantMap> week = db.getWeekSummary();
    assert(week.size() >= 1);
    assert(week[0]["total_seconds"].toInt() == 120);
    std::cout << "test_week_summary PASS" << std::endl;
}

void test_app_rank()
{
    QTemporaryFile tmpFile;
    tmpFile.open();
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    QDateTime now = QDateTime::currentDateTime();
    db.insertSession("chrome.exe", "Google", "Chrome", now, now, 60);
    db.insertSession("Code.exe", "test.py", "VS Code", now, now, 120);
    QVector<QVariantMap> rank = db.getAppRank();
    assert(rank.size() == 2);
    assert(rank[0]["app_name"].toString() == "VS Code");
    assert(rank[0]["total_seconds"].toInt() == 120);
    std::cout << "test_app_rank PASS" << std::endl;
}

void test_update_session_end()
{
    QTemporaryFile tmpFile;
    tmpFile.open();
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    QDateTime now = QDateTime::currentDateTime();
    qint64 sid = db.insertSession("chrome.exe", "Google", "Chrome", now, QDateTime(), 0);
    assert(sid > 0);

    QDateTime newEnd = now.addSecs(3600);
    db.updateSessionEnd(sid, newEnd, 3600);

    QVector<QVariantMap> all = db.getAllSessions();
    assert(all.size() == 1);
    assert(all[0]["end_time"].toString() == newEnd.toString(Qt::ISODate));
    assert(all[0]["duration_seconds"].toInt() == 3600);
    std::cout << "test_update_session_end PASS" << std::endl;
}

int main()
{
    test_create_table();
    test_insert_and_query();
    test_week_summary();
    test_app_rank();
    test_update_session_end();
    std::cout << "All database tests passed!" << std::endl;
    return 0;
}
```

- [ ] **Step 4: Create tests/CMakeLists.txt**

```cmake
if(BUILD_TESTING)
    enable_testing()

    add_executable(test_database test_database.cpp)
    target_include_directories(test_database PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_database PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME test_database COMMAND test_database)

    add_executable(test_exporter test_exporter.cpp)
    target_include_directories(test_exporter PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_exporter PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME test_exporter COMMAND test_exporter)
endif()
```

- [ ] **Step 5: Update root CMakeLists.txt to add tests**

```cmake
# Add after add_subdirectory(src)
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

- [ ] **Step 6: Build and run tests**

```bash
cmake --build build
build\test\Debug\test_database.exe
```

Expected: All database tests pass.

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "feat: DatabaseManager with SQLite + tests"
```

---

### Task 3: WindowTracker

**Files:**
- Create: `src/tracker/window_tracker.h`
- Create: `src/tracker/window_tracker.cpp`

**Interfaces:**
- Consumes: `DatabaseManager`, Win32 API
- Produces: `WindowTracker` class (QThread subclass)

- [ ] **Step 1: Write window_tracker.h**

```cpp
#ifndef WINDOW_TRACKER_H
#define WINDOW_TRACKER_H

#include <QThread>
#include <QDateTime>
#include <QString>
#include <QMap>
#include <QAtomicInt>

class DatabaseManager;

class WindowTracker : public QThread
{
    Q_OBJECT
public:
    explicit WindowTracker(DatabaseManager *db, QObject *parent = nullptr);
    void stop();
    static QString classifyApp(const QString &processName);

signals:
    void activeWindowChanged(const QString &processName, const QString &windowTitle,
                             const QString &appName);
    void idleChanged(bool idle);

protected:
    void run() override;

private:
    void tick();
    void closeCurrentSession(const QDateTime &now);
    struct WindowInfo {
        unsigned long pid;
        QString processName;
        QString windowTitle;
        QString appName;
    };
    WindowInfo getForegroundWindowInfo();

    DatabaseManager *m_db;
    QAtomicInt m_running;
    qint64 m_currentSessionId = -1;
    unsigned long m_currentPid = 0;
    QString m_currentTitle;
    QDateTime m_sessionStart;
    float m_idleSeconds = 0;
    bool m_isIdle = false;

    static constexpr float POLL_INTERVAL = 1.0f;
    static constexpr float IDLE_THRESHOLD = 60.0f;
};

#endif // WINDOW_TRACKER_H
```

- [ ] **Step 2: Write window_tracker.cpp**

```cpp
#include "window_tracker.h"
#include "database/database_manager.h"

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <QDebug>

static const QMap<QString, QString> APP_NAME_MAP = {
    {L"chrome.exe", "Chrome"},
    {L"msedge.exe", "Edge"},
    {L"firefox.exe", "Firefox"},
    {L"Code.exe", "VS Code"},
    {L"WeChat.exe", "WeChat"},
    {L"WeChatWork.exe", "WeCom"},
    {L"sublime_text.exe", "Sublime Text"},
    {L"cmd.exe", "Command Prompt"},
    {L"WindowsTerminal.exe", "Terminal"},
    {L"explorer.exe", "File Explorer"},
    {L"OUTLOOK.EXE", "Outlook"},
    {L"WINWORD.EXE", "Word"},
    {L"EXCEL.EXE", "Excel"},
    {L"POWERPNT.EXE", "PowerPoint"},
    {L"wmplayer.exe", "Windows Media Player"},
    {L"Spotify.exe", "Spotify"},
    {L"Obsidian.exe", "Obsidian"},
    {L"notepad.exe", "Notepad"},
    {L"notepad++.exe", "Notepad++"},
    {L"pycharm64.exe", "PyCharm"},
    {L"idea64.exe", "IntelliJ IDEA"},
    {L"clion64.exe", "CLion"},
    {L"goland64.exe", "GoLand"},
    {L"devenv.exe", "Visual Studio"},
    {L"typora.exe", "Typora"},
    {L"Foxmail.exe", "Foxmail"},
    {L"DingTalk.exe", "DingTalk"},
    {L"Lark.exe", "Lark"},
    {L"Slack.exe", "Slack"},
    {L"Discord.exe", "Discord"},
    {L"Telegram.exe", "Telegram"},
    {L"PotPlayer.exe", "PotPlayer"},
    {L"eclipse.exe", "Eclipse"},
    {L"AndroidStudio.exe", "Android Studio"},
    {L"Tableau.exe", "Tableau"},
    {L"Figma.exe", "Figma"},
    {L"Photoshop.exe", "Photoshop"},
    {L"winword.exe", "Word"},
    {L"excel.exe", "Excel"},
    {L"powerpnt.exe", "PowerPoint"},
    {L"outlook.exe", "Outlook"},
};

WindowTracker::WindowTracker(DatabaseManager *db, QObject *parent)
    : QThread(parent), m_db(db)
{
}

void WindowTracker::stop()
{
    m_running.storeRelaxed(0);
}

QString WindowTracker::classifyApp(const QString &processName)
{
    int pos = processName.lastIndexOf('\\');
    QString name = (pos >= 0) ? processName.mid(pos + 1) : processName;
    auto it = APP_NAME_MAP.find(name);
    if (it != APP_NAME_MAP.end())
        return it.value();
    int dotPos = name.lastIndexOf('.');
    return (dotPos >= 0) ? name.left(dotPos) : name;
}

void WindowTracker::run()
{
    m_running.storeRelaxed(1);
    while (m_running.loadRelaxed()) {
        tick();
        msleep(static_cast<unsigned long>(POLL_INTERVAL * 1000));
    }
}

void WindowTracker::tick()
{
    QDateTime now = QDateTime::currentDateTime();
    WindowInfo info = getForegroundWindowInfo();

    if (info.pid == 0) {
        m_idleSeconds += POLL_INTERVAL;
        if (m_idleSeconds >= IDLE_THRESHOLD && !m_isIdle) {
            m_isIdle = true;
            emit idleChanged(true);
            closeCurrentSession(now);
        }
        return;
    }

    m_idleSeconds = 0;
    if (m_isIdle) {
        m_isIdle = false;
        emit idleChanged(false);
    }

    if (info.pid != m_currentPid || info.windowTitle != m_currentTitle) {
        closeCurrentSession(now);
        m_currentPid = info.pid;
        m_currentTitle = info.windowTitle;
        m_sessionStart = now;
        m_currentSessionId = m_db->insertSession(
            info.processName, info.windowTitle, info.appName,
            now, QDateTime(), 0);
        emit activeWindowChanged(info.processName, info.windowTitle, info.appName);
    }
}

void WindowTracker::closeCurrentSession(const QDateTime &now)
{
    if (m_currentSessionId >= 0 && m_sessionStart.isValid()) {
        int duration = static_cast<int>(m_sessionStart.secsTo(now));
        m_db->updateSessionEnd(m_currentSessionId, now, duration);
        m_currentSessionId = -1;
        m_sessionStart = QDateTime();
    }
}

WindowTracker::WindowInfo WindowTracker::getForegroundWindowInfo()
{
    HWND hwnd = GetForegroundWindow();
    if (hwnd == nullptr)
        return {0, "", "", ""};

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0)
        return {0, "", "", ""};

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    QString processName;
    if (hProcess) {
        wchar_t exePath[MAX_PATH] = {0};
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, exePath, &size)) {
            processName = QString::fromWCharArray(exePath);
        }
        CloseHandle(hProcess);
    }
    if (processName.isEmpty())
        processName = QString("pid_%1").arg(pid);

    wchar_t title[256] = {0};
    GetWindowTextW(hwnd, title, 256);
    QString windowTitle = QString::fromWCharArray(title);
    if (windowTitle.isEmpty())
        windowTitle = processName;

    QString appName = classifyApp(processName);
    return {pid, processName, windowTitle, appName};
}
```

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: WindowTracker with Win32 foreground window polling"
```

---

### Task 4: XlsxWriter (Minimal XLSX Export)

**Files:**
- Create: `src/export/xlsx_writer.h`
- Create: `src/export/xlsx_writer.cpp`

**Interfaces:**
- Consumes: miniz (third_party/miniz/miniz.h + miniz.c)
- Produces: `XlsxWriter` class

- [ ] **Step 1: Write xlsx_writer.h**

```cpp
#ifndef XLSX_WRITER_H
#define XLSX_WRITER_H

#include <QString>
#include <QVector>
#include <QStringList>
#include <QVariant>
#include <QMap>

class XlsxWriter
{
public:
    XlsxWriter();
    ~XlsxWriter();

    void addSheet(const QString &name);
    void setHeaders(const QStringList &headers);
    void addRow(const QVector<QVariant> &row);
    void setColumnWidths(const QVector<double> &widths);
    bool save(const QString &path);

private:
    struct Sheet {
        QString name;
        QStringList headers;
        QVector<QVector<QVariant>> rows;
        QVector<double> columnWidths;
    };

    QString escapeXml(const QString &str);
    QByteArray buildContentTypes();
    QByteArray buildRels();
    QByteArray buildWorkbook();
    QByteArray buildWorkbookRels();
    QByteArray buildSheet(const Sheet &sheet, int index);
    QByteArray buildSharedStrings();

    QVector<Sheet> m_sheets;
    QMap<QString, int> m_sharedStrings;
    QVector<QString> m_sharedStringsList;
};

#endif // XLSX_WRITER_H
```

- [ ] **Step 2: Write xlsx_writer.cpp**

```cpp
#include "xlsx_writer.h"
#include "miniz.h"

#include <QFile>
#include <QCoreApplication>
#include <QDebug>

XlsxWriter::XlsxWriter() {}

XlsxWriter::~XlsxWriter() {}

void XlsxWriter::addSheet(const QString &name)
{
    Sheet s;
    s.name = name;
    m_sheets.append(s);
}

void XlsxWriter::setHeaders(const QStringList &headers)
{
    if (!m_sheets.isEmpty())
        m_sheets.last().headers = headers;
}

void XlsxWriter::addRow(const QVector<QVariant> &row)
{
    if (!m_sheets.isEmpty())
        m_sheets.last().rows.append(row);
}

void XlsxWriter::setColumnWidths(const QVector<double> &widths)
{
    if (!m_sheets.isEmpty())
        m_sheets.last().columnWidths = widths;
}

QString XlsxWriter::escapeXml(const QString &str)
{
    QString escaped = str;
    escaped.replace('&', "&amp;");
    escaped.replace('<', "&lt;");
    escaped.replace('>', "&gt;");
    escaped.replace('"', "&quot;");
    escaped.replace('\'', "&apos;");
    return escaped;
}

QByteArray XlsxWriter::buildContentTypes()
{
    return R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml" ContentType="application/xml"/>
  <Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>
  <Override PartName="/xl/sharedStrings.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml"/>
)" + [&]() -> QByteArray {
    QByteArray result;
    for (int i = 0; i < m_sheets.size(); ++i) {
        result += QByteArray("  <Override PartName=\"/xl/worksheets/sheet")
            + QByteArray::number(i + 1)
            + ".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n";
    }
    result += "</Types>";
    return result;
}().toUtf8();
}

QByteArray XlsxWriter::buildRels()
{
    return R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>
</Relationships>)";
}

QByteArray XlsxWriter::buildWorkbook()
{
    QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"
          xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
  <sheets>
)";
    for (int i = 0; i < m_sheets.size(); ++i) {
        xml += QByteArray("    <sheet name=\"")
            + escapeXml(m_sheets[i].name).toUtf8()
            + "\" sheetId=\"" + QByteArray::number(i + 1)
            + "\" r:id=\"rId" + QByteArray::number(i + 1) + "\"/>\n";
    }
    xml += "  </sheets>\n</workbook>";
    return xml;
}

QByteArray XlsxWriter::buildWorkbookRels()
{
    QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
)";
    for (int i = 0; i < m_sheets.size(); ++i) {
        xml += QByteArray("  <Relationship Id=\"rId")
            + QByteArray::number(i + 1)
            + "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet"
            + QByteArray::number(i + 1) + ".xml\"/>\n";
    }
    xml += R"(  <Relationship Id="rIdSharedStrings" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings" Target="sharedStrings.xml"/>
</Relationships>)";
    return xml;
}

QByteArray XlsxWriter::buildSharedStrings()
{
    QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" count=")"
        + QByteArray::number(m_sharedStringsList.size())
        + "\" uniqueCount=\"" + QByteArray::number(m_sharedStringsList.size()) + "\">";
    for (const auto &str : m_sharedStringsList) {
        xml += "<si><t>" + escapeXml(str).toUtf8() + "</t></si>";
    }
    xml += "</sst>";
    return xml;
}

QByteArray XlsxWriter::buildSheet(const Sheet &sheet, int index)
{
    Q_UNUSED(index);
    QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
  <cols>
)";
    if (!sheet.columnWidths.isEmpty()) {
        for (int i = 0; i < sheet.columnWidths.size(); ++i) {
            xml += QByteArray("    <col min=\"") + QByteArray::number(i + 1)
                + "\" max=\"" + QByteArray::number(i + 1)
                + "\" width=\"" + QByteArray::number(sheet.columnWidths[i], 'f', 1)
                + "\" customWidth=\"1\"/>\n";
        }
    }
    xml += "  </cols>\n  <sheetData>\n";

    auto addCell = [&](int row, int col, const QVariant &value) {
        int idx = m_sharedStringsList.size();
        m_sharedStringsList.append(value.toString());
        m_sharedStrings[value.toString()] = idx;
        xml += QByteArray("    <row r=\"") + QByteArray::number(row) + "\">\n";
        // Headers get bold style
        bool isHeader = (row == 1);
        xml += QByteArray("      <c r=\"") + QChar('A' + col) + QByteArray::number(row)
            + "\" t=\"s\""
            + (isHeader ? " s=\"1\"" : "")
            + "><v>" + QByteArray::number(idx) + "</v></c>\n";
        xml += "    </row>\n";
    };

    // Headers
    if (!sheet.headers.isEmpty()) {
        QByteArray rowXml;
        for (int c = 0; c < sheet.headers.size(); ++c) {
            int idx = m_sharedStringsList.size();
            m_sharedStringsList.append(sheet.headers[c]);
            m_sharedStrings[sheet.headers[c]] = idx;
            rowXml += QByteArray("      <c r=\"") + QChar('A' + c) + "1\" t=\"s\" s=\"1\">"
                      "<v>" + QByteArray::number(idx) + "</v></c>\n";
        }
        xml += "    <row r=\"1\">\n" + rowXml + "    </row>\n";
    }

    // Data rows
    for (int r = 0; r < sheet.rows.size(); ++r) {
        int rowNum = r + 2; // 1-indexed, header is row 1
        xml += QByteArray("    <row r=\"") + QByteArray::number(rowNum) + "\">\n";
        for (int c = 0; c < sheet.rows[r].size(); ++c) {
            const QVariant &val = sheet.rows[r][c];
            bool isNumeric = false;
            double numVal = 0;
            if (val.typeId() == QMetaType::Int || val.typeId() == QMetaType::Double) {
                isNumeric = true;
                numVal = val.toDouble();
            }
            if (isNumeric) {
                xml += QByteArray("      <c r=\"") + QChar('A' + c) + QByteArray::number(rowNum)
                    + "\"><v>" + QByteArray::number(numVal) + "</v></c>\n";
            } else {
                int idx = m_sharedStringsList.size();
                m_sharedStringsList.append(val.toString());
                m_sharedStrings[val.toString()] = idx;
                xml += QByteArray("      <c r=\"") + QChar('A' + c) + QByteArray::number(rowNum)
                    + "\" t=\"s\"><v>" + QByteArray::number(idx) + "</v></c>\n";
            }
        }
        xml += "    </row>\n";
    }

    xml += "  </sheetData>\n</worksheet>";
    return xml;
}

bool XlsxWriter::save(const QString &path)
{
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_file(&zip, path.toUtf8().constData(), 0)) {
        qWarning() << "Failed to create ZIP file:" << path;
        return false;
    }

    auto addFile = [&](const char *name, const QByteArray &content) {
        mz_zip_writer_add_mem(&zip, name, content.constData(), content.size(), MZ_DEFAULT_COMPRESSION);
    };

    addFile("[Content_Types].xml", buildContentTypes());
    addFile("_rels/.rels", buildRels());
    addFile("xl/workbook.xml", buildWorkbook());
    addFile("xl/_rels/workbook.xml.rels", buildWorkbookRels());
    addFile("xl/sharedStrings.xml", buildSharedStrings());

    for (int i = 0; i < m_sheets.size(); ++i) {
        QByteArray name = QByteArray("xl/worksheets/sheet") + QByteArray::number(i + 1) + ".xml";
        addFile(name.constData(), buildSheet(m_sheets[i], i));
    }

    mz_zip_writer_finalize_archive(&zip);
    mz_zip_writer_end(&zip);
    return true;
}
```

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: XlsxWriter with miniz ZIP support"
```

---

### Task 5: Exporter (CSV + XLSX)

**Files:**
- Create: `src/export/exporter.h`
- Create: `src/export/exporter.cpp`
- Create: `tests/test_exporter.cpp`

**Interfaces:**
- Consumes: `DatabaseManager`, `XlsxWriter`
- Produces: `Exporter` class

- [ ] **Step 1: Write exporter.h**

```cpp
#ifndef EXPORTER_H
#define EXPORTER_H

#include <QString>

class DatabaseManager;

class Exporter
{
public:
    explicit Exporter(DatabaseManager *db);
    bool exportCsv(const QString &path);
    bool exportExcel(const QString &path);

private:
    DatabaseManager *m_db;
};

#endif // EXPORTER_H
```

- [ ] **Step 2: Write exporter.cpp**

```cpp
#include "exporter.h"
#include "database/database_manager.h"
#include "export/xlsx_writer.h"

#include <QFile>
#include <QTextStream>
#include <QDate>
#include <QDebug>

Exporter::Exporter(DatabaseManager *db)
    : m_db(db)
{
}

bool Exporter::exportCsv(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#endif
    // UTF-8 BOM
    out << QChar(0xFEFF);

    out << QString::fromUtf8("\xe8\xbf\x9b\xe7\xa8\x8b\xe5\x90\x8d")
        << "," << QString::fromUtf8("\xe7\xaa\x97\xe5\x8f\xa3\xe6\xa0\x87\xe9\xa2\x98")
        << "," << QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe5\x90\x8d")
        << "," << QString::fromUtf8("\xe5\xbc\x80\xe5\xa7\x8b\xe6\x97\xb6\xe9\x97\xb4")
        << "," << QString::fromUtf8("\xe7\xbb\x93\xe6\x9d\x9f\xe6\x97\xb6\xe9\x97\xb4")
        << "," << QString::fromUtf8("\xe6\x8c\x81\xe7\xbb\xad\xe7\xa7\x92\xe6\x95\xb0")
        << "\n";

    auto sessions = m_db->getAllSessions();
    for (const auto &s : sessions) {
        auto csvEscape = [](const QString &val) {
            if (val.contains(',') || val.contains('"') || val.contains('\n')) {
                QString escaped = val;
                escaped.replace('"', "\"\"");
                return '"' + escaped + '"';
            }
            return val;
        };
        out << csvEscape(s["process_name"].toString()) << ","
            << csvEscape(s["window_title"].toString()) << ","
            << csvEscape(s["app_name"].toString()) << ","
            << s["start_time"].toString() << ","
            << s["end_time"].toString() << ","
            << s["duration_seconds"].toString() << "\n";
    }

    file.close();
    return true;
}

bool Exporter::exportExcel(const QString &path)
{
    XlsxWriter xlsx;

    // Sheet 1: Usage Records
    xlsx.addSheet(QString::fromUtf8("\xe4\xbd\xbf\xe7\x94\xa8\xe8\xae\xb0\xe5\xbd\x95"));
    xlsx.setHeaders({
        QString::fromUtf8("\xe8\xbf\x9b\xe7\xa8\x8b\xe5\x90\x8d"),
        QString::fromUtf8("\xe7\xaa\x97\xe5\x8f\xa3\xe6\xa0\x87\xe9\xa2\x98"),
        QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe5\x90\x8d"),
        QString::fromUtf8("\xe5\xbc\x80\xe5\xa7\x8b\xe6\x97\xb6\xe9\x97\xb4"),
        QString::fromUtf8("\xe7\xbb\x93\xe6\x9d\x9f\xe6\x97\xb6\xe9\x97\xb4"),
        QString::fromUtf8("\xe6\x8c\x81\xe7\xbb\xad\xe7\xa7\x92\xe6\x95\xb0")
    });
    xlsx.setColumnWidths({20, 30, 15, 22, 22, 12});

    auto sessions = m_db->getAllSessions();
    for (const auto &s : sessions) {
        xlsx.addRow({
            s["process_name"].toString(),
            s["window_title"].toString(),
            s["app_name"].toString(),
            s["start_time"].toString(),
            s["end_time"].toString(),
            s["duration_seconds"].toInt()
        });
    }

    // Sheet 2: Daily Summary
    QDate today = QDate::currentDate();
    QDate monday = today.addDays(-today.dayOfWeek() + 1);
    xlsx.addSheet(QString::fromUtf8("\xe6\xaf\x8f\xe6\x97\xa5\xe6\xb1\x87\xe6\x80\xbb"));
    xlsx.setHeaders({
        QString::fromUtf8("\xe6\x97\xa5\xe6\x9c\x9f"),
        QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe5\x90\x8d"),
        QString::fromUtf8("\xe4\xbd\xbf\xe7\x94\xa8\xe7\xa7\x92\xe6\x95\xb0")
    });
    xlsx.setColumnWidths({15, 20, 12});

    auto summaries = m_db->getDailySummaries(monday.toString(Qt::ISODate), today.toString(Qt::ISODate));
    for (const auto &s : summaries) {
        xlsx.addRow({s["d"].toString(), s["app_name"].toString(), s["total_seconds"].toInt()});
    }

    // Sheet 3: App Ranking
    xlsx.addSheet(QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe6\x8e\x92\xe8\xa1\x8c"));
    xlsx.setHeaders({
        QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe5\x90\x8d"),
        QString::fromUtf8("\xe4\xbd\xbf\xe7\x94\xa8\xe7\xa7\x92\xe6\x95\xb0")
    });
    xlsx.setColumnWidths({20, 12});

    auto rank = m_db->getAppRank();
    for (const auto &r : rank) {
        xlsx.addRow({r["app_name"].toString(), r["total_seconds"].toInt()});
    }

    return xlsx.save(path);
}
```

- [ ] **Step 3: Write test_exporter.cpp**

```cpp
#include <cassert>
#include <iostream>
#include <QTemporaryFile>
#include <QDir>
#include <QDateTime>
#include "database/database_manager.h"
#include "export/exporter.h"

void test_export_csv()
{
    QTemporaryFile tmpFile;
    tmpFile.open();
    QString dbPath = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(dbPath);
    QDateTime now = QDateTime::currentDateTime();
    db.insertSession("chrome.exe", "test", "Chrome", now, now, 60);

    Exporter exporter(&db);
    QString csvPath = QDir::tempPath() + "/test_export.csv";
    bool ok = exporter.exportCsv(csvPath);
    assert(ok);

    QFile file(csvPath);
    assert(file.open(QIODevice::ReadOnly));
    QByteArray content = file.readAll();
    file.close();
    QDir().remove(csvPath);

    assert(content.contains("Chrome"));
    std::cout << "test_export_csv PASS" << std::endl;
}

void test_export_excel()
{
    QTemporaryFile tmpFile;
    tmpFile.open();
    QString dbPath = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(dbPath);
    QDateTime now = QDateTime::currentDateTime();
    db.insertSession("Code.exe", "test.py", "VS Code", now, now, 120);

    Exporter exporter(&db);
    QString xlsxPath = QDir::tempPath() + "/test_export.xlsx";
    bool ok = exporter.exportExcel(xlsxPath);
    assert(ok);
    assert(QFile::exists(xlsxPath));
    QDir().remove(xlsxPath);
    std::cout << "test_export_excel PASS" << std::endl;
}

int main()
{
    test_export_csv();
    test_export_excel();
    std::cout << "All exporter tests passed!" << std::endl;
    return 0;
}
```

- [ ] **Step 4: Add test build to tests/CMakeLists.txt**

```cmake
add_executable(test_exporter test_exporter.cpp)
target_include_directories(test_exporter PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_exporter PRIVATE Qt6::Core Qt6::Sql)
add_test(NAME test_exporter COMMAND test_exporter)
```

- [ ] **Step 5: Build and run tests**

```bash
cmake --build build
build\tests\Debug\test_exporter.exe
```

Expected: All exporter tests pass.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "feat: Exporter with CSV and XLSX export"
```

---

### Task 6: TrayManager

**Files:**
- Create: `src/ui/tray_manager.h`
- Create: `src/ui/tray_manager.cpp`

**Interfaces:**
- Consumes: Qt6::Widgets
- Produces: `TrayManager` class with signals `showMainWindow`, `quitApp`

- [ ] **Step 1: Write tray_manager.h**

```cpp
#ifndef TRAY_MANAGER_H
#define TRAY_MANAGER_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QIcon>

class TrayManager : public QObject
{
    Q_OBJECT
public:
    explicit TrayManager(const QString &appName = "Time Master", QObject *parent = nullptr);
    void show();
    void setTooltip(const QString &text);
    void showNotification(const QString &title, const QString &message);

signals:
    void showMainWindow();
    void quitApp();

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);

private:
    QIcon loadIcon();
    QSystemTrayIcon *m_tray;
    QMenu *m_menu;
};

#endif // TRAY_MANAGER_H
```

- [ ] **Step 2: Write tray_manager.cpp**

```cpp
#include "tray_manager.h"
#include <QCoreApplication>
#include <QDir>
#include <QAction>

TrayManager::TrayManager(const QString &appName, QObject *parent)
    : QObject(parent)
{
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(loadIcon());
    m_tray->setToolTip(appName);

    m_menu = new QMenu();
    QAction *showAction = m_menu->addAction(QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba\xe4\xb8\xbb\xe7\x95\x8c\xe9\x9d\xa2"));
    connect(showAction, &QAction::triggered, this, &TrayManager::showMainWindow);

    m_menu->addSeparator();

    QAction *quitAction = m_menu->addAction(QString::fromUtf8("\xe9\x80\x80\xe5\x87\xba"));
    connect(quitAction, &QAction::triggered, this, &TrayManager::quitApp);

    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, &TrayManager::onActivated);
}

QIcon TrayManager::loadIcon()
{
    QString pngPath = QCoreApplication::applicationDirPath() + "/icon.png";
    if (!QFile::exists(pngPath)) {
        pngPath = ":/icon.png";
    }
    if (QFile::exists(pngPath))
        return QIcon(pngPath);
    return QIcon();
}

void TrayManager::show()
{
    m_tray->show();
}

void TrayManager::setTooltip(const QString &text)
{
    m_tray->setToolTip(text);
}

void TrayManager::showNotification(const QString &title, const QString &message)
{
    m_tray->showMessage(title, message, QIcon(), 3000);
}

void TrayManager::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick)
        emit showMainWindow();
}
```

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: TrayManager with system tray icon and context menu"
```

---

### Task 7: StatsWidget (CircularProgress, WeeklyBar, GlassCard)

**Files:**
- Create: `src/ui/stats_widget.h`
- Create: `src/ui/stats_widget.cpp`

**Interfaces:**
- Consumes: `DatabaseManager`, Qt6::Widgets
- Produces: `StatsWidget` containing `CircularProgress` and `WeeklyBar`

- [ ] **Step 1: Write stats_widget.h**

```cpp
#ifndef STATS_WIDGET_H
#define STATS_WIDGET_H

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMap>
#include <QDate>

class DatabaseManager;

class CircularProgress : public QWidget
{
    Q_OBJECT
public:
    explicit CircularProgress(QWidget *parent = nullptr);
    void setValue(int totalSeconds);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_value = 0;
    int m_maxValue = 86400;
    int m_hours = 0;
    int m_minutes = 0;
};

class GlassCard : public QFrame
{
    Q_OBJECT
public:
    explicit GlassCard(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

class WeeklyBar : public QWidget
{
    Q_OBJECT
public:
    explicit WeeklyBar(QWidget *parent = nullptr);
    void setData(const QVector<QVariantMap> &weekData);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QMap<QString, int> m_data;
    int m_maxVal = 1;
};

class StatsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit StatsWidget(DatabaseManager *db, QWidget *parent = nullptr);
    void refresh();

private:
    DatabaseManager *m_db;
    CircularProgress *m_circularProgress;
    WeeklyBar *m_weeklyBar;
};

#endif // STATS_WIDGET_H
```

- [ ] **Step 2: Write stats_widget.cpp**

```cpp
#include "stats_widget.h"
#include "database/database_manager.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QFont>
#include <QDate>
#include <QtMath>

// ========== CircularProgress ==========

CircularProgress::CircularProgress(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(140, 140);
}

void CircularProgress::setValue(int totalSeconds)
{
    m_value = qMin(totalSeconds, m_maxValue);
    int totalMinutes = totalSeconds / 60;
    m_hours = totalMinutes / 60;
    m_minutes = totalMinutes % 60;
    update();
}

void CircularProgress::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF rect(10, 10, 120, 120);
    double penWidth = 10;

    QPen bgPen(QColor(255, 255, 255, 30), penWidth);
    bgPen.setCapStyle(Qt::RoundCap);
    painter.setPen(bgPen);
    painter.drawArc(rect, 0, 360 * 16);

    double ratio = static_cast<double>(m_value) / m_maxValue;
    QLinearGradient gradient(0, 0, 140, 140);
    gradient.setColorAt(0.0, QColor("#818CF8"));
    gradient.setColorAt(1.0, QColor("#6366F1"));
    QPen fgPen(QBrush(gradient), penWidth);
    fgPen.setCapStyle(Qt::RoundCap);
    painter.setPen(fgPen);
    int span = static_cast<int>(-ratio * 360 * 16);
    painter.drawArc(rect, 90 * 16, span);

    QFont fontH("PingFang SC", 32, QFont::Light);
    painter.setFont(fontH);
    painter.setPen(QColor(255, 255, 255));
    painter.drawText(QRectF(0, 40, 140, 50), Qt::AlignCenter, QString::number(m_hours));

    QFont fontM("PingFang SC", 14, QFont::Normal);
    painter.setFont(fontM);
    painter.setPen(QColor(255, 255, 255, 180));
    painter.drawText(QRectF(0, 82, 140, 30), Qt::AlignCenter,
                     QString("%1 %2").arg(m_minutes).arg(QString::fromUtf8("\xe5\x88\x86\xe9\x92\x9f")));
}

// ========== GlassCard ==========

GlassCard::GlassCard(QWidget *parent)
    : QFrame(parent)
{
    setStyleSheet("GlassCard { background-color: rgba(255, 255, 255, 8); border-radius: 16px; }");
}

void GlassCard::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(0, 0, width(), height(), 16, 16);
    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0.0, QColor(255, 255, 255, 25));
    gradient.setColorAt(1.0, QColor(255, 255, 255, 8));
    painter.fillPath(path, QBrush(gradient));
    QPen pen(QColor(255, 255, 255, 30), 1);
    painter.setPen(pen);
    painter.drawPath(path);
}

// ========== WeeklyBar ==========

WeeklyBar::WeeklyBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(120);
}

void WeeklyBar::setData(const QVector<QVariantMap> &weekData)
{
    QStringList dayNames = {
        QString::fromUtf8("\xe4\xb8\x80"),
        QString::fromUtf8("\xe4\xba\x8c"),
        QString::fromUtf8("\xe4\xb8\x89"),
        QString::fromUtf8("\xe5\x9b\x9b"),
        QString::fromUtf8("\xe4\xba\x94"),
        QString::fromUtf8("\xe5\x85\xad"),
        QString::fromUtf8("\xe6\x97\xa5")
    };

    QDate today = QDate::currentDate();
    QDate monday = today.addDays(-today.dayOfWeek() + 1);
    m_data.clear();
    m_maxVal = 1;

    for (int i = 0; i < 7; ++i) {
        m_data[monday.addDays(i).toString(Qt::ISODate)] = 0;
    }
    for (const auto &item : weekData) {
        QString d = item["d"].toString();
        if (m_data.contains(d))
            m_data[d] = item["total_seconds"].toInt();
    }
    for (auto it = m_data.begin(); it != m_data.end(); ++it)
        m_maxVal = qMax(m_maxVal, it.value());
    if (m_maxVal == 0) m_maxVal = 1;
    update();
}

void WeeklyBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    double w = width();
    double h = height();
    int barCount = 7;
    double barW = qMin(32.0, (w - 40) / barCount);
    double gap = (barCount > 1) ? ((w - 40 - barW * barCount) / (barCount - 1)) : 0;
    double startX = 20;
    double labelY = h - 20;

    QStringList dayNames = {
        QString::fromUtf8("\xe4\xb8\x80"),
        QString::fromUtf8("\xe4\xba\x8c"),
        QString::fromUtf8("\xe4\xb8\x89"),
        QString::fromUtf8("\xe5\x9b\x9b"),
        QString::fromUtf8("\xe4\xba\x94"),
        QString::fromUtf8("\xe5\x85\xad"),
        QString::fromUtf8("\xe6\x97\xa5")
    };
    QDate today = QDate::currentDate();
    QDate monday = today.addDays(-today.dayOfWeek() + 1);

    QFont fontSmall("PingFang SC", 11);
    painter.setFont(fontSmall);

    for (int i = 0; i < 7; ++i) {
        double x = startX + i * (barW + gap);
        QString d = monday.addDays(i).toString(Qt::ISODate);
        int val = m_data.value(d, 0);
        double barH = (m_maxVal > 0) ? (static_cast<double>(val) / m_maxVal) * (labelY - 30) : 0;

        if (barH > 0) {
            QLinearGradient gradient(x, labelY - 20 - barH, x, labelY - 20);
            gradient.setColorAt(0.0, QColor("#A5B4FC"));
            gradient.setColorAt(1.0, QColor("#6366F1"));
            painter.setBrush(QBrush(gradient));
            painter.setPen(Qt::NoPen);
            QPainterPath path;
            path.addRoundedRect(x, labelY - 20 - barH, barW, barH, 4, 4);
            painter.drawPath(path);
        }

        painter.setPen(QColor(255, 255, 255, 160));
        painter.drawText(QRectF(x, labelY - 5, barW, 20), Qt::AlignCenter, dayNames[i]);
        if (val > 0) {
            int mins = val / 60;
            painter.setPen(QColor(255, 255, 255, 100));
            painter.drawText(QRectF(x, labelY - 28 - barH, barW, 16), Qt::AlignCenter,
                             QString("%1m").arg(mins));
        }
    }
}

// ========== StatsWidget ==========

StatsWidget::StatsWidget(DatabaseManager *db, QWidget *parent)
    : QWidget(parent), m_db(db)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);

    // Today card
    GlassCard *todayCard = new GlassCard(this);
    QVBoxLayout *todayLayout = new QVBoxLayout(todayCard);
    todayLayout->setAlignment(Qt::AlignCenter);
    QLabel *todayTitle = new QLabel(QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe4\xbd\xbf\xe7\x94\xa8"), todayCard);
    todayTitle->setFont(QFont("PingFang SC", 14, QFont::Normal));
    todayTitle->setStyleSheet("color: rgba(255,255,255,180);");
    todayTitle->setAlignment(Qt::AlignCenter);
    m_circularProgress = new CircularProgress(todayCard);
    todayLayout->addWidget(todayTitle);
    todayLayout->addWidget(m_circularProgress, 0, Qt::AlignCenter);
    layout->addWidget(todayCard);

    // Weekly card
    GlassCard *weekCard = new GlassCard(this);
    QVBoxLayout *weekLayout = new QVBoxLayout(weekCard);
    QLabel *weekTitle = new QLabel(QString::fromUtf8("\xe6\x9c\xac\xe5\x91\xa8\xe8\xb6\x8b\xe5\x8a\xbf"), weekCard);
    weekTitle->setFont(QFont("PingFang SC", 14, QFont::Normal));
    weekTitle->setStyleSheet("color: rgba(255,255,255,180);");
    weekTitle->setAlignment(Qt::AlignCenter);
    m_weeklyBar = new WeeklyBar(weekCard);
    weekLayout->addWidget(weekTitle);
    weekLayout->addWidget(m_weeklyBar);
    layout->addWidget(weekCard);
}

void StatsWidget::refresh()
{
    m_circularProgress->setValue(m_db->getTodayTotal());
    m_weeklyBar->setData(m_db->getWeekSummary());
}
```

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: StatsWidget with circular progress and weekly bar chart"
```

---

### Task 8: AppRankWidget

**Files:**
- Create: `src/ui/app_rank_widget.h`
- Create: `src/ui/app_rank_widget.cpp`

**Interfaces:**
- Consumes: `DatabaseManager`, Qt6::Widgets
- Produces: `AppRankWidget` showing per-app time ranking

- [ ] **Step 1: Write app_rank_widget.h**

```cpp
#ifndef APP_RANK_WIDGET_H
#define APP_RANK_WIDGET_H

#include <QWidget>
#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>

class DatabaseManager;

class AppRankItem : public QWidget
{
    Q_OBJECT
public:
    AppRankItem(int rank, const QString &appName, int totalSeconds, int maxSeconds,
                QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_rank;
    QString m_appName;
    int m_totalSeconds;
    int m_maxSeconds;
};

class AppRankWidget : public QFrame
{
    Q_OBJECT
public:
    explicit AppRankWidget(DatabaseManager *db, QWidget *parent = nullptr);
    void refresh();

private:
    DatabaseManager *m_db;
    QVBoxLayout *m_listLayout;
    QWidget *m_listWidget;
};

#endif // APP_RANK_WIDGET_H
```

- [ ] **Step 2: Write app_rank_widget.cpp**

```cpp
#include "app_rank_widget.h"
#include "database/database_manager.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QFont>

AppRankItem::AppRankItem(int rank, const QString &appName, int totalSeconds, int maxSeconds,
                         QWidget *parent)
    : QWidget(parent), m_rank(rank), m_appName(appName),
      m_totalSeconds(totalSeconds), m_maxSeconds(maxSeconds)
{
    setFixedHeight(48);
}

void AppRankItem::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    double w = width();
    double h = height();
    double barMaxW = w - 200;

    painter.setFont(QFont("PingFang SC", 13, QFont::Normal));
    painter.setPen(QColor(255, 255, 255, 200));
    painter.drawText(QRectF(8, 0, 30, h), Qt::AlignLeft | Qt::AlignVCenter, QString::number(m_rank));

    painter.setPen(QColor(255, 255, 255, 220));
    painter.drawText(QRectF(40, 0, 100, h), Qt::AlignLeft | Qt::AlignVCenter, m_appName);

    double ratio = (m_maxSeconds > 0) ? (static_cast<double>(m_totalSeconds) / m_maxSeconds) : 0;
    double barW = barMaxW * ratio;
    if (barW > 0) {
        QLinearGradient gradient(0, 0, barW, 0);
        gradient.setColorAt(0.0, QColor("#818CF8"));
        gradient.setColorAt(1.0, QColor("#6366F1"));
        painter.setBrush(QBrush(gradient));
        painter.setPen(Qt::NoPen);
        QPainterPath path;
        path.addRoundedRect(140, h / 2 - 6, barW, 12, 6, 6);
        painter.drawPath(path);
    }

    int mins = m_totalSeconds / 60;
    int hours = mins / 60;
    int remainMins = mins % 60;
    QString timeStr = (hours > 0) ? QString("%1h %2m").arg(hours).arg(remainMins)
                                  : QString("%1m").arg(remainMins);
    painter.setPen(QColor(255, 255, 255, 140));
    painter.drawText(QRectF(w - 60, 0, 55, h), Qt::AlignRight | Qt::AlignVCenter, timeStr);
}

AppRankWidget::AppRankWidget(DatabaseManager *db, QWidget *parent)
    : QFrame(parent), m_db(db)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(4);

    QLabel *title = new QLabel(QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe4\xbd\xbf\xe7\x94\xa8\xe6\x8e\x92\xe8\xa1\x8c (\xe4\xbb\x8a\xe6\x97\xa5)"), this);
    title->setFont(QFont("PingFang SC", 14, QFont::Normal));
    title->setStyleSheet("color: rgba(255,255,255,180);");
    layout->addWidget(title);

    m_listWidget = new QWidget(this);
    m_listLayout = new QVBoxLayout(m_listWidget);
    m_listLayout->setContentsMargins(0, 8, 0, 0);
    m_listLayout->setSpacing(2);
    layout->addWidget(m_listWidget);
}

void AppRankWidget::refresh()
{
    QVector<QVariantMap> data = m_db->getAppRank();

    // Clear existing items
    while (m_listLayout->count()) {
        QLayoutItem *item = m_listLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    int maxSec = data.isEmpty() ? 1 : data[0]["total_seconds"].toInt();
    for (int i = 0; i < data.size(); ++i) {
        AppRankItem *rankItem = new AppRankItem(
            i + 1, data[i]["app_name"].toString(),
            data[i]["total_seconds"].toInt(), maxSec, m_listWidget);
        m_listLayout->addWidget(rankItem);
    }
    m_listLayout->addStretch();
}
```

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: AppRankWidget with per-app time ranking list"
```

---

### Task 9: MainWindow

**Files:**
- Create: `src/ui/main_window.h`
- Create: `src/ui/main_window.cpp`

**Interfaces:**
- Consumes: `DatabaseManager`, `StatsWidget`, `AppRankWidget`, `Exporter`, Qt6::Widgets
- Produces: `MainWindow` with Mica/acrylic backdrop

- [ ] **Step 1: Write main_window.h**

```cpp
#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QTimer>

class DatabaseManager;
class StatsWidget;
class AppRankWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(DatabaseManager *db, QWidget *parent = nullptr);

public slots:
    void refreshData();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void applyBackdrop();
    void onExport();

private:
    DatabaseManager *m_db;
    StatsWidget *m_statsWidget;
    AppRankWidget *m_appRankWidget;
    QTimer *m_refreshTimer;
};

#endif // MAIN_WINDOW_H
```

- [ ] **Step 2: Write main_window.cpp**

```cpp
#include "main_window.h"
#include "database/database_manager.h"
#include "ui/stats_widget.h"
#include "ui/app_rank_widget.h"
#include "export/exporter.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QWidget>
#include <QApplication>

#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

static const DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
static const DWORD DWMWA_SYSTEMBACKDROP_TYPE = 38;
static const int MICA = 2;
static const int DWMSBT_ACRYLICWINDOW = 3;

static DWORD getWindowsBuild()
{
    using RtlGetVersionPtr = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll");
    if (!ntdll) return 0;
    auto RtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(
        GetProcAddress(ntdll, "RtlGetVersion"));
    if (!RtlGetVersion) return 0;

    RTL_OSVERSIONINFOW osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    if (RtlGetVersion(&osvi) == 0)
        return osvi.dwBuildNumber;
    return 0;
}

static void applyBackdropEffect(HWND hwnd)
{
    DWORD build = getWindowsBuild();
    int backdrop = (build >= 22000) ? MICA : DWMSBT_ACRYLICWINDOW;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
}

MainWindow::MainWindow(DatabaseManager *db, QWidget *parent)
    : QMainWindow(parent), m_db(db)
{
    setWindowTitle("Time Master");
    setMinimumSize(800, 500);
    resize(900, 600);
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(99, 102, 241, 200);"
        "  color: white; border: none; border-radius: 8px;"
        "  padding: 8px 20px; font-family: 'PingFang SC'; font-size: 13px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(129, 140, 248, 220);"
        "}"
        "QPushButton:pressed {"
        "  background-color: rgba(79, 70, 229, 220);"
        "}"
    );

    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *layout = new QVBoxLayout(central);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);

    m_statsWidget = new StatsWidget(db, this);
    layout->addWidget(m_statsWidget);

    m_appRankWidget = new AppRankWidget(db, this);
    layout->addWidget(m_appRankWidget);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    QPushButton *exportBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x93\xa4 \xe5\xaf\xbc\xe5\x87\xba\xe8\xae\xb0\xe5\xbd\x95"), this);
    connect(exportBtn, &QPushButton::clicked, this, &MainWindow::onExport);
    btnLayout->addWidget(exportBtn);

    QPushButton *refreshBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x94\x84 \xe5\x88\xb7\xe6\x96\xb0"), this);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshData);
    btnLayout->addWidget(refreshBtn);

    layout->addLayout(btnLayout);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(10000);
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::refreshData);
    m_refreshTimer->start();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    QTimer::singleShot(100, this, &MainWindow::applyBackdrop);
}

void MainWindow::applyBackdrop()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    applyBackdropEffect(hwnd);
}

void MainWindow::refreshData()
{
    m_statsWidget->refresh();
    m_appRankWidget->refresh();
}

void MainWindow::onExport()
{
    QStringList formats;
    formats << QString::fromUtf8("CSV (.csv)")
            << QString::fromUtf8("Excel (.xlsx)");
    bool ok = false;
    QString fmt = QInputDialog::getItem(this,
        QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe6\xa0\xbc\xe5\xbc\x8f"),
        QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe5\xaf\xbc\xe5\x87\xba\xe6\xa0\xbc\xe5\xbc\x8f:"),
        formats, 0, false, &ok);
    if (!ok) return;

    QString filter = fmt.contains("CSV")
        ? QString::fromUtf8("CSV \xe6\x96\x87\xe4\xbb\xb6 (*.csv)")
        : QString::fromUtf8("Excel \xe6\x96\x87\xe4\xbb\xb6 (*.xlsx)");
    QString path = QFileDialog::getSaveFileName(this,
        QString::fromUtf8("\xe4\xbf\x9d\xe5\xad\x98\xe6\x96\x87\xe4\xbb\xb6"),
        "", filter);
    if (path.isEmpty()) return;

    try {
        Exporter exporter(m_db);
        bool success = false;
        if (fmt.contains("CSV"))
            success = exporter.exportCsv(path);
        else
            success = exporter.exportExcel(path);

        if (success) {
            QMessageBox::information(this,
                QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe6\x88\x90\xe5\x8a\x9f"),
                QString::fromUtf8("\xe8\xae\xb0\xe5\xbd\x95\xe5\xb7\xb2\xe5\xaf\xbc\xe5\x87\xba\xe5\x88\xb0:\n") + path);
        } else {
            QMessageBox::critical(this,
                QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe5\xa4\xb1\xe8\xb4\xa5"),
                QString::fromUtf8("\xe6\x97\xa0\xe6\xb3\x95\xe5\x86\x99\xe5\x85\xa5\xe6\x96\x87\xe4\xbb\xb6"));
        }
    } catch (const std::exception &e) {
        QMessageBox::critical(this,
            QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe5\xa4\xb1\xe8\xb4\xa5"),
            e.what());
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
}
```

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: MainWindow with Mica backdrop and widget integration"
```

---

### Task 10: main.cpp Entry Point

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: All components
- Produces: Complete application

- [ ] **Step 1: Rewrite main.cpp**

```cpp
#include <QApplication>
#include <QFont>
#include <QTimer>
#include "database/database_manager.h"
#include "tracker/window_tracker.h"
#include "ui/main_window.h"
#include "ui/tray_manager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Time Master");
    app.setOrganizationName("TimeMaster");
    app.setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    // Set global font
    QFont font("PingFang SC", 10);
    font.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(font);

    DatabaseManager db;

    MainWindow window(&db);

    WindowTracker tracker(&db);
    tracker.start();

    TrayManager tray("Time Master");
    QObject::connect(&tray, &TrayManager::showMainWindow, [&]() {
        window.show();
        window.refreshData();
    });

    // Update tray tooltip every 10 seconds
    QTimer tooltipTimer;
    auto updateTooltip = [&]() {
        int total = db.getTodayTotal();
        int hours = total / 3600;
        int minutes = (total % 3600) / 60;
        tray.setTooltip(QString("Time Master - %1 %2h %3m")
            .arg(QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe5\xb7\xb2\xe7\x94\xa8"))
            .arg(hours).arg(minutes));
    };
    QObject::connect(&tooltipTimer, &QTimer::timeout, updateTooltip);
    tooltipTimer.setInterval(10000);
    tooltipTimer.start();
    updateTooltip();

    QObject::connect(&tray, &TrayManager::quitApp, [&]() {
        tracker.stop();
        tracker.wait(2000);
        db.close();
        app.quit();
    });

    tray.show();
    window.refreshData();

    return app.exec();
}
```

- [ ] **Step 2: Build the full project**

```bash
cmake --build build
```

Expected: Clean build, no errors, `TimeMaster.exe` is produced.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: complete main.cpp wiring all components together"
```

---

### Task 11: Smoke Test and Verification

**Files:**
- Modify: `CMakeLists.txt` (if needed for release build)

- [ ] **Step 1: Build Release**

```bash
cmake --preset default
cmake --build build --config Release
```

- [ ] **Step 2: Run database tests**

```bash
build\tests\Release\test_database.exe
```

Expected: All database tests pass.

- [ ] **Step 3: Run exporter tests**

```bash
build\tests\Release\test_exporter.exe
```

Expected: All exporter tests pass.

- [ ] **Step 4: Verify application launches**

```bash
build\src\Release\TimeMaster.exe
```

Expected: Window appears with system tray icon. Close minimizes to tray. Double-click tray icon shows window.

- [ ] **Step 5: Remove old Python files** (optional)

If the user wants to remove the Python source files after confirming the C++ version works:

```bash
git rm -r tracker/ ui/ data/ export/ tests/ main.py requirements.txt build_export_files.py "Time Master.spec"
git commit -m "chore: remove Python source files after C++ rewrite"
```
