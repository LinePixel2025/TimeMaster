# Settings Feature Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a settings dialog (gear icon on main window header) that lets users block apps from tracking, rename apps, and configure tracking parameters. Settings persist in SQLite.

**Architecture:** 3 new tables in SQLite (`settings`, `ignored_apps`, `app_aliases`). New `SettingsDialog` reads/writes via `DatabaseManager`. `WindowTracker` caches settings and applies filtering/aliasing in its tick loop.

**Tech Stack:** C++17, Qt6 Widgets+Sql, MinGW 64-bit, CMake+Ninja, raw `assert()` tests

## Global Constraints

- All `DatabaseManager` methods must acquire `QMutexLocker lock(&m_mutex)`
- Font: `Microsoft YaHei`, background: `#F0F2F5`
- No MICA/transparent backgrounds
- Tests use `QCoreApplication` in `main()`, `QTemporaryFile` for temp DBs, plain `assert()`, return 0 = pass
- `CMakeLists.txt` sources explicitly listed (AUTOMOC handles Q_OBJECT)

---

### Task 1: Database schema migration + default settings seed

**Files:**
- Modify: `src/database/database_manager.cpp:29-44`

- [ ] **Step 1: Add settings tables to migrate()**

Replace the `migrate()` method body:

```cpp
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

    q.exec(
        "CREATE TABLE IF NOT EXISTS settings ("
        "key TEXT PRIMARY KEY, "
        "value TEXT NOT NULL)"
    );

    q.exec(
        "CREATE TABLE IF NOT EXISTS ignored_apps ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "process_name TEXT NOT NULL UNIQUE)"
    );

    q.exec(
        "CREATE TABLE IF NOT EXISTS app_aliases ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "process_name TEXT NOT NULL UNIQUE, "
        "display_name TEXT NOT NULL)"
    );

    q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('tracking_enabled', 'true')");
    q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('poll_interval', '1')");
    q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('idle_threshold', '60')");
    q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('auto_start', 'false')");
}
```

- [ ] **Step 2: Build**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

Expected: compiles and links.

- [ ] **Step 3: Commit**

```bash
git add src/database/database_manager.cpp
git commit -m "feat: add settings tables migration with default seeds"
```

---

### Task 2: DatabaseManager getSetting / setSetting

**Files:**
- Modify: `src/database/database_manager.h` (add declarations)
- Modify: `src/database/database_manager.cpp` (add implementations)
- Modify: `tests/test_database.cpp` (add tests)

**Interfaces:**
- Produces: `QString getSetting(const QString &key, const QString &defaultValue = QString())`
- Produces: `void setSetting(const QString &key, const QString &value)`

- [ ] **Step 1: Add declarations to header**

In `src/database/database_manager.h`, after the `getDailySummaries` declaration (line 31), add:

```cpp
    QString getSetting(const QString &key, const QString &defaultValue = QString());
    void setSetting(const QString &key, const QString &value);
```

- [ ] **Step 2: Add implementations**

In `src/database/database_manager.cpp`, after the `getDailySummaries` method body (before `close()`), add:

```cpp
QString DatabaseManager::getSetting(const QString &key, const QString &defaultValue)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("SELECT value FROM settings WHERE key = ?");
    q.addBindValue(key);
    q.exec();
    if (q.next())
        return q.value("value").toString();
    return defaultValue;
}

void DatabaseManager::setSetting(const QString &key, const QString &value)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)");
    q.addBindValue(key);
    q.addBindValue(value);
    if (!q.exec())
        qWarning() << "setSetting failed:" << q.lastError();
}
```

- [ ] **Step 3: Add tests**

In `tests/test_database.cpp`, add before `main()`:

```cpp
void test_settings_default()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    QString val = db.getSetting("tracking_enabled", "false");
    assert(val == "true");
    std::cout << "test_settings_default PASS" << std::endl;
}

void test_settings_set_get()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("custom_key", "custom_value");
    QString val = db.getSetting("custom_key", "");
    assert(val == "custom_value");
    std::cout << "test_settings_set_get PASS" << std::endl;
}

void test_settings_missing_returns_default()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    QString val = db.getSetting("nonexistent", "fallback");
    assert(val == "fallback");
    std::cout << "test_settings_missing_returns_default PASS" << std::endl;
}
```

In `tests/test_database.cpp`, in `main()`, add after existing test calls:

```cpp
    test_settings_default();
    test_settings_set_get();
    test_settings_missing_returns_default();
```

- [ ] **Step 4: Build and run tests**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
$env:QT_PLUGIN_PATH = "D:\AICOP\requirements\QT6\6.11.1\mingw_64\plugins"
.\build\tests\test_database.exe
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/database/database_manager.h src/database/database_manager.cpp tests/test_database.cpp
git commit -m "feat: add getSetting/setSetting methods with tests"
```

---

### Task 3: DatabaseManager ignored_apps + app_aliases + getAllKnownProcessNames

**Files:**
- Modify: `src/database/database_manager.h` (add declarations and includes)
- Modify: `src/database/database_manager.cpp` (add implementations)
- Modify: `tests/test_database.cpp` (add tests)

**Interfaces:**
- Produces: `QMap<int, QString> getIgnoredApps()` (id -> process_name)
- Produces: `int addIgnoredApp(const QString &processName)`
- Produces: `void removeIgnoredApp(int id)`
- Produces: `QMap<QString, QString> getAppAliases()` (process_name -> display_name)
- Produces: `int setAppAlias(const QString &processName, const QString &displayName)`
- Produces: `void removeAppAlias(int id)`
- Produces: `void removeAppAliasByProcessName(const QString &processName)`
- Produces: `QStringList getAllKnownProcessNames()`

- [ ] **Step 1: Add includes and declarations to header**

At the top of `src/database/database_manager.h`, add:

```cpp
#include <QSet>
#include <QMap>
#include <QStringList>
```

After the settings declarations, add:

```cpp
    QMap<int, QString> getIgnoredApps();
    int addIgnoredApp(const QString &processName);
    void removeIgnoredApp(int id);

    QMap<QString, QString> getAppAliases();
    int setAppAlias(const QString &processName, const QString &displayName);
    void removeAppAlias(int id);
    void removeAppAliasByProcessName(const QString &processName);

    QStringList getAllKnownProcessNames();
```

- [ ] **Step 2: Add implementations**

In `src/database/database_manager.cpp`, after the settings methods, add:

```cpp
QMap<int, QString> DatabaseManager::getIgnoredApps()
{
    QMutexLocker lock(&m_mutex);
    QMap<int, QString> result;
    QSqlQuery q(m_db);
    q.exec("SELECT id, process_name FROM ignored_apps ORDER BY process_name");
    while (q.next())
        result.insert(q.value("id").toInt(), q.value("process_name").toString());
    return result;
}

int DatabaseManager::addIgnoredApp(const QString &processName)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("INSERT OR IGNORE INTO ignored_apps (process_name) VALUES (?)");
    q.addBindValue(processName);
    if (!q.exec()) {
        qWarning() << "addIgnoredApp failed:" << q.lastError();
        return -1;
    }
    return q.lastInsertId().toInt();
}

void DatabaseManager::removeIgnoredApp(int id)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM ignored_apps WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec())
        qWarning() << "removeIgnoredApp failed:" << q.lastError();
}

QMap<QString, QString> DatabaseManager::getAppAliases()
{
    QMutexLocker lock(&m_mutex);
    QMap<QString, QString> result;
    QSqlQuery q(m_db);
    q.exec("SELECT process_name, display_name FROM app_aliases");
    while (q.next())
        result.insert(q.value("process_name").toString(), q.value("display_name").toString());
    return result;
}

int DatabaseManager::setAppAlias(const QString &processName, const QString &displayName)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO app_aliases (process_name, display_name) VALUES (?, ?)");
    q.addBindValue(processName);
    q.addBindValue(displayName);
    if (!q.exec()) {
        qWarning() << "setAppAlias failed:" << q.lastError();
        return -1;
    }
    return q.lastInsertId().toInt();
}

void DatabaseManager::removeAppAlias(int id)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM app_aliases WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec())
        qWarning() << "removeAppAlias failed:" << q.lastError();
}

void DatabaseManager::removeAppAliasByProcessName(const QString &processName)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM app_aliases WHERE process_name = ?");
    q.addBindValue(processName);
    if (!q.exec())
        qWarning() << "removeAppAliasByProcessName failed:" << q.lastError();
}

QStringList DatabaseManager::getAllKnownProcessNames()
{
    QMutexLocker lock(&m_mutex);
    QStringList result;
    QSqlQuery q(m_db);
    q.exec("SELECT DISTINCT process_name FROM sessions ORDER BY process_name");
    while (q.next())
        result.append(q.value("process_name").toString());
    return result;
}
```

- [ ] **Step 3: Add tests**

In `tests/test_database.cpp`, add before `main()`:

```cpp
void test_ignored_apps()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);

    QMap<int, QString> empty = db.getIgnoredApps();
    assert(empty.isEmpty());

    int id1 = db.addIgnoredApp("chrome.exe");
    assert(id1 > 0);
    int id2 = db.addIgnoredApp("explorer.exe");
    assert(id2 > 0);

    QMap<int, QString> apps = db.getIgnoredApps();
    assert(apps.size() == 2);
    assert(apps.values().contains("chrome.exe"));
    assert(apps.values().contains("explorer.exe"));

    db.removeIgnoredApp(id1);
    apps = db.getIgnoredApps();
    assert(apps.size() == 1);
    assert(!apps.values().contains("chrome.exe"));

    std::cout << "test_ignored_apps PASS" << std::endl;
}

void test_app_aliases()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);

    QMap<QString, QString> empty = db.getAppAliases();
    assert(empty.isEmpty());

    db.setAppAlias("code.exe", "VS Code Custom");
    db.setAppAlias("chrome.exe", "My Chrome");

    QMap<QString, QString> aliases = db.getAppAliases();
    assert(aliases.size() == 2);
    assert(aliases["code.exe"] == "VS Code Custom");
    assert(aliases["chrome.exe"] == "My Chrome");

    db.removeAppAliasByProcessName("chrome.exe");
    aliases = db.getAppAliases();
    assert(aliases.size() == 1);
    assert(!aliases.contains("chrome.exe"));

    std::cout << "test_app_aliases PASS" << std::endl;
}

void test_get_all_known_process_names()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);

    QStringList empty = db.getAllKnownProcessNames();
    assert(empty.isEmpty());

    QDateTime now = QDateTime::currentDateTime();
    db.insertSession("chrome.exe", "Google", "Chrome", now, now, 60);
    db.insertSession("code.exe", "test.py", "VS Code", now, now, 120);
    db.insertSession("chrome.exe", "YouTube", "Chrome", now, now, 30);

    QStringList names = db.getAllKnownProcessNames();
    assert(names.size() == 2);
    assert(names.contains("chrome.exe"));
    assert(names.contains("code.exe"));

    std::cout << "test_get_all_known_process_names PASS" << std::endl;
}
```

In `tests/test_database.cpp` `main()`, add:

```cpp
    test_ignored_apps();
    test_app_aliases();
    test_get_all_known_process_names();
```

- [ ] **Step 4: Build and run tests**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
$env:QT_PLUGIN_PATH = "D:\AICOP\requirements\QT6\6.11.1\mingw_64\plugins"
.\build\tests\test_database.exe
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/database/database_manager.h src/database/database_manager.cpp tests/test_database.cpp
git commit -m "feat: add ignored_apps, app_aliases, getAllKnownProcessNames with tests"
```

---

### Task 4: SettingsDialog UI

**Files:**
- Create: `src/ui/settings_dialog.h`
- Create: `src/ui/settings_dialog.cpp`
- Modify: `src/CMakeLists.txt` (add new files to SOURCES and HEADERS)

**Interfaces:**
- Produces: `class SettingsDialog : public QDialog` with constructor `SettingsDialog(DatabaseManager *db, QWidget *parent = nullptr)`
- Produces: signal `settingsChanged()`
- Consumes: `DatabaseManager::getSetting`, `setSetting`, `getIgnoredApps`, `addIgnoredApp`, `removeIgnoredApp`, `getAppAliases`, `setAppAlias`, `removeAppAliasByProcessName`, `getAllKnownProcessNames`

- [ ] **Step 1: Write settings_dialog.h**

```cpp
#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QListWidget>
#include <QTableWidget>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>

class DatabaseManager;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(DatabaseManager *db, QWidget *parent = nullptr);

signals:
    void settingsChanged();

private slots:
    void onAddIgnored();
    void onRemoveIgnored();
    void onAddAlias();
    void onEditAlias();
    void onDeleteAlias();

private:
    void loadSettings();
    void saveSettings();
    void refreshKnownAppsList();
    void refreshIgnoredList();
    void refreshAliasTable();

    DatabaseManager *m_db;

    QTabWidget *m_tabWidget;
    QListWidget *m_knownAppsList;
    QListWidget *m_ignoredAppsList;
    QTableWidget *m_aliasTable;
    QCheckBox *m_trackingEnabled;
    QSpinBox *m_pollInterval;
    QSpinBox *m_idleThreshold;
    QCheckBox *m_autoStart;
};

#endif // SETTINGS_DIALOG_H
```

- [ ] **Step 2: Write settings_dialog.cpp**

```cpp
#include "settings_dialog.h"
#include "database/database_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QHeaderView>

static QFont appFont(int size, QFont::Weight weight = QFont::Normal)
{
    QFont font("Microsoft YaHei", size, weight);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

SettingsDialog::SettingsDialog(DatabaseManager *db, QWidget *parent)
    : QDialog(parent), m_db(db)
{
    setWindowTitle(QString::fromUtf8("\xe8\xae\xbe\xe7\xbd\xae"));
    resize(500, 450);
    setMinimumSize(500, 450);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    m_tabWidget = new QTabWidget(this);
    mainLayout->addWidget(m_tabWidget);

    // ---- Tab 1: App Management ----
    QWidget *appTab = new QWidget();
    QVBoxLayout *appLayout = new QVBoxLayout(appTab);
    appLayout->setContentsMargins(8, 8, 8, 8);
    appLayout->setSpacing(8);

    QLabel *knownLabel = new QLabel(QString::fromUtf8("\xe5\xb7\xb2\xe7\x9f\xa5\xe5\xba\x94\xe7\x94\xa8"), this);
    knownLabel->setFont(appFont(12, QFont::Medium));
    appLayout->addWidget(knownLabel);

    m_knownAppsList = new QListWidget();
    m_knownAppsList->setSelectionMode(QAbstractItemView::MultiSelection);
    m_knownAppsList->setMaximumHeight(100);
    appLayout->addWidget(m_knownAppsList);

    QPushButton *addIgnoredBtn = new QPushButton(QString::fromUtf8("\xe5\x8a\xa0\xe5\x85\xa5\xe5\xb1\x8f\xe8\x94\xbd\xe6\xb8\x85\xe5\x8d\x95"), this);
    connect(addIgnoredBtn, &QPushButton::clicked, this, &SettingsDialog::onAddIgnored);
    appLayout->addWidget(addIgnoredBtn);

    QLabel *ignoredLabel = new QLabel(QString::fromUtf8("\xe5\xb7\xb2\xe5\xb1\x8f\xe8\x94\xbd\xe5\xba\x94\xe7\x94\xa8"), this);
    ignoredLabel->setFont(appFont(12, QFont::Medium));
    appLayout->addWidget(ignoredLabel);

    QHBoxLayout *ignoredRow = new QHBoxLayout();
    m_ignoredAppsList = new QListWidget();
    ignoredRow->addWidget(m_ignoredAppsList, 1);

    QPushButton *removeIgnoredBtn = new QPushButton(QString::fromUtf8("\xe7\xa7\xbb\xe9\x99\xa4"), this);
    connect(removeIgnoredBtn, &QPushButton::clicked, this, &SettingsDialog::onRemoveIgnored);
    ignoredRow->addWidget(removeIgnoredBtn);
    appLayout->addLayout(ignoredRow);

    QLabel *aliasLabel = new QLabel(QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe5\x90\x8d\xe7\xa7\xb0\xe5\x88\xab\xe5\x90\x8d"), this);
    aliasLabel->setFont(appFont(12, QFont::Medium));
    appLayout->addWidget(aliasLabel);

    m_aliasTable = new QTableWidget(0, 3, this);
    m_aliasTable->setHorizontalHeaderLabels({
        QString::fromUtf8("\xe8\xbf\x9b\xe7\xa8\x8b\xe5\x90\x8d"),
        QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba\xe5\x90\x8d"),
        QString::fromUtf8("\xe6\x93\x8d\xe4\xbd\x9c")
    });
    m_aliasTable->horizontalHeader()->setStretchLastSection(true);
    m_aliasTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_aliasTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    appLayout->addWidget(m_aliasTable);

    QHBoxLayout *aliasBtnRow = new QHBoxLayout();
    QPushButton *addAliasBtn = new QPushButton(QString::fromUtf8("\xe6\xb7\xbb\xe5\x8a\xa0\xe5\x88\xab\xe5\x90\x8d"), this);
    connect(addAliasBtn, &QPushButton::clicked, this, &SettingsDialog::onAddAlias);
    aliasBtnRow->addWidget(addAliasBtn);

    QPushButton *editAliasBtn = new QPushButton(QString::fromUtf8("\xe7\xbc\x96\xe8\xbe\x91"), this);
    connect(editAliasBtn, &QPushButton::clicked, this, &SettingsDialog::onEditAlias);
    aliasBtnRow->addWidget(editAliasBtn);

    QPushButton *deleteAliasBtn = new QPushButton(QString::fromUtf8("\xe5\x88\xa0\xe9\x99\xa4"), this);
    connect(deleteAliasBtn, &QPushButton::clicked, this, &SettingsDialog::onDeleteAlias);
    aliasBtnRow->addWidget(deleteAliasBtn);
    aliasBtnRow->addStretch();
    appLayout->addLayout(aliasBtnRow);

    m_tabWidget->addTab(appTab, QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe7\xae\xa1\xe7\x90\x86"));

    // ---- Tab 2: Tracking Settings ----
    QWidget *trackTab = new QWidget();
    QVBoxLayout *trackLayout = new QVBoxLayout(trackTab);
    trackLayout->setContentsMargins(8, 8, 8, 8);
    trackLayout->setSpacing(16);

    m_trackingEnabled = new QCheckBox(QString::fromUtf8("\xe5\x90\xaf\xe7\x94\xa8\xe8\xbf\xbd\xe8\xb8\xaa"), this);
    m_trackingEnabled->setFont(appFont(13));
    trackLayout->addWidget(m_trackingEnabled);

    QHBoxLayout *pollRow = new QHBoxLayout();
    pollRow->addWidget(new QLabel(QString::fromUtf8("\xe8\xbd\xae\xe8\xaf\xa2\xe9\x97\xb4\xe9\x9a\x94\xef\xbc\x88\xe7\xa7\x92\xef\xbc\x89:"), this));
    m_pollInterval = new QSpinBox(this);
    m_pollInterval->setRange(1, 10);
    m_pollInterval->setValue(1);
    pollRow->addWidget(m_pollInterval);
    pollRow->addStretch();
    trackLayout->addLayout(pollRow);

    QHBoxLayout *idleRow = new QHBoxLayout();
    idleRow->addWidget(new QLabel(QString::fromUtf8("\xe7\xa9\xba\xe9\x97\xb2\xe5\x88\xa4\xe5\xae\x9a\xe6\x97\xb6\xe9\x97\xb4\xef\xbc\x88\xe7\xa7\x92\xef\xbc\x89:"), this));
    m_idleThreshold = new QSpinBox(this);
    m_idleThreshold->setRange(10, 600);
    m_idleThreshold->setValue(60);
    idleRow->addWidget(m_idleThreshold);
    idleRow->addStretch();
    trackLayout->addLayout(idleRow);

    trackLayout->addStretch();
    m_tabWidget->addTab(trackTab, QString::fromUtf8("\xe8\xbf\xbd\xe8\xb8\xaa\xe8\xae\xbe\xe7\xbd\xae"));

    // ---- Tab 3: Personalization ----
    QWidget *personalTab = new QWidget();
    QVBoxLayout *personalLayout = new QVBoxLayout(personalTab);
    personalLayout->setContentsMargins(8, 8, 8, 8);
    personalLayout->setSpacing(16);

    m_autoStart = new QCheckBox(QString::fromUtf8("\xe5\xbc\x80\xe6\x9c\xba\xe8\x87\xaa\xe5\x90\xaf"), this);
    m_autoStart->setFont(appFont(13));
    QLabel *autoStartNote = new QLabel(QString::fromUtf8("\xe6\x9a\x82\xe6\x9c\xaa\xe5\xae\x9e\xe7\x8e\xb0\xe5\x90\x8e\xe7\xab\xaf\xef\xbc\x8c\xe4\xbb\x85\xe4\xbf\x9d\xe5\xad\x98\xe9\x85\x8d\xe7\xbd\xae"), this);
    autoStartNote->setStyleSheet("color: #9CA3AF; font-size: 11px;");
    personalLayout->addWidget(m_autoStart);
    personalLayout->addWidget(autoStartNote);
    personalLayout->addStretch();
    m_tabWidget->addTab(personalTab, QString::fromUtf8("\xe4\xb8\xaa\xe6\x80\xa7\xe5\x8c\x96"));

    // ---- Bottom Buttons ----
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    QPushButton *cancelBtn = new QPushButton(QString::fromUtf8("\xe5\x8f\x96\xe6\xb6\x88"), this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    QPushButton *saveBtn = new QPushButton(QString::fromUtf8("\xe4\xbf\x9d\xe5\xad\x98"), this);
    saveBtn->setStyleSheet(
        "QPushButton { background-color: #6366F1; color: white; border: none; border-radius: 6px; "
        "padding: 6px 16px; font-size: 13px; }"
        "QPushButton:hover { background-color: #818CF8; }"
        "QPushButton:pressed { background-color: #4F46E5; }");
    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        saveSettings();
        emit settingsChanged();
        accept();
    });
    btnLayout->addWidget(saveBtn);

    mainLayout->addLayout(btnLayout);

    loadSettings();
}

void SettingsDialog::loadSettings()
{
    m_trackingEnabled->setChecked(m_db->getSetting("tracking_enabled", "true") == "true");
    m_pollInterval->setValue(m_db->getSetting("poll_interval", "1").toInt());
    m_idleThreshold->setValue(m_db->getSetting("idle_threshold", "60").toInt());
    m_autoStart->setChecked(m_db->getSetting("auto_start", "false") == "true");
    refreshKnownAppsList();
    refreshIgnoredList();
    refreshAliasTable();
}

void SettingsDialog::saveSettings()
{
    m_db->setSetting("tracking_enabled", m_trackingEnabled->isChecked() ? "true" : "false");
    m_db->setSetting("poll_interval", QString::number(m_pollInterval->value()));
    m_db->setSetting("idle_threshold", QString::number(m_idleThreshold->value()));
    m_db->setSetting("auto_start", m_autoStart->isChecked() ? "true" : "false");
}

void SettingsDialog::refreshKnownAppsList()
{
    m_knownAppsList->clear();
    QStringList processNames = m_db->getAllKnownProcessNames();
    QMap<int, QString> ignored = m_db->getIgnoredApps();
    QSet<QString> ignoredNames(ignored.values().begin(), ignored.values().end());
    for (const QString &name : processNames) {
        QListWidgetItem *item = new QListWidgetItem(name);
        if (ignoredNames.contains(name))
            item->setForeground(QColor("#9CA3AF"));
        m_knownAppsList->addItem(item);
    }
}

void SettingsDialog::refreshIgnoredList()
{
    m_ignoredAppsList->clear();
    QMap<int, QString> ignored = m_db->getIgnoredApps();
    for (auto it = ignored.begin(); it != ignored.end(); ++it) {
        QListWidgetItem *item = new QListWidgetItem(it.value());
        item->setData(Qt::UserRole, it.key());
        m_ignoredAppsList->addItem(item);
    }
}

void SettingsDialog::refreshAliasTable()
{
    m_aliasTable->setRowCount(0);
    QMap<QString, QString> aliases = m_db->getAppAliases();
    for (auto it = aliases.begin(); it != aliases.end(); ++it) {
        int row = m_aliasTable->rowCount();
        m_aliasTable->insertRow(row);
        m_aliasTable->setItem(row, 0, new QTableWidgetItem(it.key()));
        m_aliasTable->setItem(row, 1, new QTableWidgetItem(it.value()));
    }
}

void SettingsDialog::onAddIgnored()
{
    QList<QListWidgetItem *> selected = m_knownAppsList->selectedItems();
    for (QListWidgetItem *item : selected) {
        m_db->addIgnoredApp(item->text());
    }
    refreshKnownAppsList();
    refreshIgnoredList();
}

void SettingsDialog::onRemoveIgnored()
{
    QListWidgetItem *item = m_ignoredAppsList->currentItem();
    if (!item) {
        QMessageBox::warning(this,
            QString::fromUtf8("\xe6\xb2\xa1\xe6\x9c\x89\xe9\x80\x89\xe6\x8b\xa9"),
            QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe9\x80\x89\xe6\x8b\xa9\xe8\xa6\x81\xe7\xa7\xbb\xe9\x99\xa4\xe7\x9a\x84\xe5\xba\x94\xe7\x94\xa8"));
        return;
    }
    int id = item->data(Qt::UserRole).toInt();
    m_db->removeIgnoredApp(id);
    refreshIgnoredList();
    refreshKnownAppsList();
}

void SettingsDialog::onAddAlias()
{
    bool ok;
    QString processName = QInputDialog::getText(this,
        QString::fromUtf8("\xe6\xb7\xbb\xe5\x8a\xa0\xe5\x88\xab\xe5\x90\x8d"),
        QString::fromUtf8("\xe8\xbf\x9b\xe7\xa8\x8b\xe5\x90\x8d (e.g. code.exe):"),
        QLineEdit::Normal, "", &ok);
    if (!ok || processName.isEmpty()) return;

    QString displayName = QInputDialog::getText(this,
        QString::fromUtf8("\xe6\xb7\xbb\xe5\x8a\xa0\xe5\x88\xab\xe5\x90\x8d"),
        QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba\xe5\x90\x8d:"),
        QLineEdit::Normal, "", &ok);
    if (!ok || displayName.isEmpty()) return;

    m_db->setAppAlias(processName, displayName);
    refreshAliasTable();
}

void SettingsDialog::onEditAlias()
{
    int row = m_aliasTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this,
            QString::fromUtf8("\xe6\xb2\xa1\xe6\x9c\x89\xe9\x80\x89\xe6\x8b\xa9"),
            QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe9\x80\x89\xe6\x8b\xa9\xe8\xa6\x81\xe7\xbc\x96\xe8\xbe\x91\xe7\x9a\x84\xe5\x88\xab\xe5\x90\x8d"));
        return;
    }
    QString processName = m_aliasTable->item(row, 0)->text();
    QString currentDisplay = m_aliasTable->item(row, 1)->text();
    bool ok;
    QString newDisplay = QInputDialog::getText(this,
        QString::fromUtf8("\xe7\xbc\x96\xe8\xbe\x91\xe5\x88\xab\xe5\x90\x8d"),
        QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba\xe5\x90\x8d:"),
        QLineEdit::Normal, currentDisplay, &ok);
    if (!ok || newDisplay.isEmpty()) return;
    m_db->setAppAlias(processName, newDisplay);
    refreshAliasTable();
}

void SettingsDialog::onDeleteAlias()
{
    int row = m_aliasTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this,
            QString::fromUtf8("\xe6\xb2\xa1\xe6\x9c\x89\xe9\x80\x89\xe6\x8b\xa9"),
            QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe9\x80\x89\xe6\x8b\xa9\xe8\xa6\x81\xe5\x88\xa0\xe9\x99\xa4\xe7\x9a\x84\xe5\x88\xab\xe5\x90\x8d"));
        return;
    }
    QString processName = m_aliasTable->item(row, 0)->text();
    m_db->removeAppAliasByProcessName(processName);
    refreshAliasTable();
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

In `src/CMakeLists.txt`, add to SOURCES:

```
    ui/settings_dialog.cpp
```

Add to HEADERS:

```
    ui/settings_dialog.h
```

- [ ] **Step 4: Build**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

Expected: compiles and links successfully.

- [ ] **Step 5: Commit**

```bash
git add src/ui/settings_dialog.h src/ui/settings_dialog.cpp src/CMakeLists.txt
git commit -m "feat: add SettingsDialog with app management, tracking, and personalization tabs"
```

---

### Task 5: MainWindow settings button

**Files:**
- Modify: `src/ui/main_window.h` (add include, slot, member)
- Modify: `src/ui/main_window.cpp` (add button and slot implementation)

**Interfaces:**
- Consumes: `SettingsDialog` constructor, `settingsChanged` signal

- [ ] **Step 1: Add to main_window.h**

Add include at top:

```cpp
#include <QDialog>
```

Add private slot after `onExport`:

```cpp
    void onSettings();
```

- [ ] **Step 2: Add button and slot in main_window.cpp**

Add include at top:

```cpp
#include "ui/settings_dialog.h"
```

In the constructor, after `headerLayout->addWidget(titleLabel)`, add:

```cpp
    QPushButton *settingsBtn = new QPushButton(QString::fromUtf8("\xe2\x9a\x99"), this);
    settingsBtn->setStyleSheet(
        "QPushButton { background-color: transparent; color: #4B5563; border: none; "
        "font-size: 20px; padding: 4px; }"
        "QPushButton:hover { background-color: #E5E7EB; border-radius: 6px; }");
    settingsBtn->setToolTip(QString::fromUtf8("\xe8\xae\xbe\xe7\xbd\xae"));
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettings);
    headerLayout->addWidget(settingsBtn);
```

After `onExport()` method body, add:

```cpp
void MainWindow::onSettings()
{
    SettingsDialog dialog(m_db, this);
    dialog.exec();
    if (dialog.result() == QDialog::Accepted)
        refreshData();
}
```

- [ ] **Step 3: Build**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

Expected: compiles and links.

- [ ] **Step 4: Commit**

```bash
git add src/ui/main_window.h src/ui/main_window.cpp
git commit -m "feat: add settings gear button to main window header"
```

---

### Task 6: WindowTracker filtering + aliases + reloadSettings

**Files:**
- Modify: `src/tracker/window_tracker.h` (add member variables and method)
- Modify: `src/tracker/window_tracker.cpp` (add filtering logic, alias lookup, reloadSettings)

- [ ] **Step 1: Add to window_tracker.h**

In the private section of `WindowTracker`, add after `m_isIdle`:

```cpp
    QSet<QString> m_ignoredApps;
    QMap<QString, QString> m_aliases;
    bool m_trackingEnabled = true;
    float m_pollInterval = POLL_INTERVAL;
    float m_idleThreshold = IDLE_THRESHOLD;
```

Add in public section after `classifyApp`:

```cpp
    void reloadSettings();
```

- [ ] **Step 2: Add reloadSettings and modify classifyApp in window_tracker.cpp**

In `src/tracker/window_tracker.cpp`, add at the end of the file:

```cpp
void WindowTracker::reloadSettings()
{
    m_trackingEnabled = (m_db->getSetting("tracking_enabled", "true") == "true");
    m_pollInterval = m_db->getSetting("poll_interval", "1").toFloat();
    m_idleThreshold = m_db->getSetting("idle_threshold", "60").toFloat();

    QMap<int, QString> ignored = m_db->getIgnoredApps();
    m_ignoredApps = QSet<QString>(ignored.values().begin(), ignored.values().end());

    m_aliases = m_db->getAppAliases();
}
```

Modify the `classifyApp` method to check aliases first:

```cpp
QString WindowTracker::classifyApp(const QString &processName)
{
    // Check aliases first
    auto alias = m_aliases.find(processName);
    if (alias != m_aliases.end())
        return alias.value();

    int pos = processName.lastIndexOf('\\');
    QString name = (pos >= 0) ? processName.mid(pos + 1) : processName;
    auto it = APP_NAME_MAP.find(name);
    if (it != APP_NAME_MAP.end())
        return it.value();
    int dotPos = name.lastIndexOf('.');
    return (dotPos >= 0) ? name.left(dotPos) : name;
}
```

Modify the `tick()` method to check tracking_enabled and ignored apps. Add at the top of `tick()` after `WindowInfo info = getForegroundWindowInfo();`:

```cpp
    if (!m_trackingEnabled)
        return;

    if (info.pid != 0) {
        int pos = info.processName.lastIndexOf('\\');
        QString name = (pos >= 0) ? info.processName.mid(pos + 1) : info.processName;
        if (m_ignoredApps.contains(name))
            return;
    }
```

Modify the constructor `WindowTracker::WindowTracker` to call reloadSettings:

```cpp
WindowTracker::WindowTracker(DatabaseManager *db, QObject *parent)
    : QThread(parent), m_db(db)
{
    reloadSettings();
}
```

- [ ] **Step 3: Build**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

Expected: compiles and links.

- [ ] **Step 4: Commit**

```bash
git add src/tracker/window_tracker.h src/tracker/window_tracker.cpp
git commit -m "feat: add app filtering, aliasing, and reloadSettings to WindowTracker"
```

---

### Task 7: Wire settingsChanged signal in main.cpp

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Connect signal in main.cpp**

In `src/main.cpp`, after the tray `quitApp` connection, add:

```cpp
    tracker.reloadSettings();  // Load initial settings
```

After `window.refreshData()`, add nothing further needed — the MainWindow already has `onSettings()` which connects internally via the dialog's `settingsChanged` signal. However, since the SettingsDialog signal needs to reach the tracker, we connect it inside `MainWindow::onSettings()` or add a direct connection.

To keep it simple, modify `MainWindow::onSettings()` in `main_window.cpp` (already written in Task 5) to also reload the tracker. But `MainWindow` doesn't have access to `WindowTracker`. Instead, wire it in `main.cpp`.

Better approach: modify `MainWindow` to emit a new signal when settings change, and connect that in `main.cpp`.

Add to `MainWindow` class in `main_window.h`:

```cpp
signals:
    void settingsChanged();
```

Modify `MainWindow::onSettings()` in `main_window.cpp`:

```cpp
void MainWindow::onSettings()
{
    SettingsDialog dialog(m_db, this);
    if (dialog.exec() == QDialog::Accepted) {
        refreshData();
        emit settingsChanged();
    }
}
```

In `main.cpp`, after `window.refreshData()` add:

```cpp
    QObject::connect(&window, &MainWindow::settingsChanged, [&]() {
        tracker.reloadSettings();
    });
```

And also call reloadSettings initially (since tracker constructor already does it, this is a safety net):

```cpp
    tracker.reloadSettings();
```

- [ ] **Step 2: Build**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

Expected: compiles and links.

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp src/ui/main_window.h src/ui/main_window.cpp
git commit -m "feat: wire settingsChanged signal from MainWindow to WindowTracker"
```

---

### Task 8: Full integration build and run

- [ ] **Step 1: Clean rebuild**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

Expected: clean compile and link.

- [ ] **Step 2: Run tests**

```powershell
$env:QT_PLUGIN_PATH = "D:\AICOP\requirements\QT6\6.11.1\mingw_64\plugins"
.\build\tests\test_database.exe
```

Expected: all tests pass.

- [ ] **Step 3: Verify no crash on launch**

```powershell
Stop-Process -Name "TimeMaster" -Force -ErrorAction SilentlyContinue
# Run the app briefly to verify it starts
Start-Process -FilePath ".\build\src\TimeMaster.exe" -NoNewWindow
Start-Sleep -Seconds 2
Get-Process "TimeMaster" -ErrorAction SilentlyContinue
# Check for crashes in Event Log
Get-WinEvent -FilterHashtable @{LogName='Application'; Level=2; StartTime=(Get-Date).AddMinutes(-2)} -MaxEvents 5 -ErrorAction SilentlyContinue | Where-Object { $_.Id -eq 1000 -and (( [xml]$_.ToXml()).Event.EventData.Data)[0].'#text' -like "*TimeMaster*" } | Format-Table TimeCreated, @{N='Fault';E={(([xml]$_.ToXml()).Event.EventData.Data)[4].'#text'}} -AutoSize
Stop-Process -Name "TimeMaster" -Force -ErrorAction SilentlyContinue
```
