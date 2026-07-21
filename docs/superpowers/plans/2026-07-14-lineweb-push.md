# LineWeb 推送功能 — 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 Time Master 添加定时推送每日屏幕使用时间到 LineWeb 的功能

**Architecture:** 新建 `src/push/` 模块，`LineWebPusher` 类维护 QTimer + QNetworkAccessManager；在 `main.cpp` 中创建并连接生命周期信号；在设置对话框新增「云端同步」标签页

**Tech Stack:** C++17, Qt6 Widgets + Sql + Network, CMake + Ninja, MinGW

## Global Constraints

- 包含路径使用 `database/database_manager.h` 格式（不加 `src/` 前缀）
- `Q_OBJECT` 类依赖 AUTOMOC，需在 CMakeLists 的 SOURCES/HEADERS 中注册
- 所有 `m_db` 访问由 `DatabaseManager` 内部 `QMutexLocker` 保护
- 推送失败不重试、不弹窗（仅 `qWarning()` 打日志）
- 不引入第三方库（Qt6::Network 已够用）
- 字体使用 `Microsoft YaHei`

---

### Task 1: 创建 `LineWebPusher` 类

**Files:**
- Create: `src/push/lineweb_pusher.h`
- Create: `src/push/lineweb_pusher.cpp`

**Interfaces:**
- Produces: `class LineWebPusher : public QObject` — `start()`, `stop()`, `pushNow()`, `reloadSettings()`, `doPush()` (private), signals `pushSucceeded()`, `pushFailed(const QString &)`

- [ ] **Step 1: 创建头文件 `src/push/lineweb_pusher.h`**

```cpp
#ifndef LINEWEB_PUSHER_H
#define LINEWEB_PUSHER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QNetworkAccessManager>

class DatabaseManager;

class LineWebPusher : public QObject
{
    Q_OBJECT
public:
    explicit LineWebPusher(DatabaseManager *db, QObject *parent = nullptr);

    void start();
    void stop();
    void pushNow();
    void reloadSettings();

signals:
    void pushSucceeded();
    void pushFailed(const QString &error);

private:
    void doPush();

    DatabaseManager *m_db;
    QNetworkAccessManager *m_nam;
    QTimer *m_timer;
    QString m_token;
    QString m_endpoint;
    int m_intervalMinutes = 10;
    bool m_enabled = false;
};

#endif // LINEWEB_PUSHER_H
```

- [ ] **Step 2: 创建实现文件 `src/push/lineweb_pusher.cpp`**

```cpp
#include "lineweb_pusher.h"
#include "database/database_manager.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDate>
#include <QUrl>
#include <QEventLoop>
#include <QTimer>

LineWebPusher::LineWebPusher(DatabaseManager *db, QObject *parent)
    : QObject(parent), m_db(db)
{
    m_nam = new QNetworkAccessManager(this);
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &LineWebPusher::doPush);
}

void LineWebPusher::start()
{
    reloadSettings();
}

void LineWebPusher::stop()
{
    m_timer->stop();
}

void LineWebPusher::reloadSettings()
{
    bool wasEnabled = m_enabled;
    QString oldToken = m_token;
    QString oldEndpoint = m_endpoint;
    int oldInterval = m_intervalMinutes;

    m_enabled = (m_db->getSetting("lineweb_enabled", "false") == "true");
    m_token = m_db->getSetting("lineweb_token", "");
    m_endpoint = m_db->getSetting("lineweb_endpoint", "");
    m_intervalMinutes = m_db->getSetting("lineweb_interval", "10").toInt();

    if (m_enabled != wasEnabled
        || m_token != oldToken
        || m_endpoint != oldEndpoint
        || m_intervalMinutes != oldInterval)
    {
        m_timer->stop();
        if (m_enabled && !m_token.isEmpty() && !m_endpoint.isEmpty())
            m_timer->start(m_intervalMinutes * 60 * 1000);
    }
}

void LineWebPusher::doPush()
{
    int totalSeconds = m_db->getTodayTotal();
    QDate today = QDate::currentDate();

    QJsonObject body;
    body["totalSeconds"] = totalSeconds;
    body["date"] = today.toString("yyyy-MM-dd");

    QUrl url(m_endpoint + "/api/health/push");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("X-Screen-Time-Token", m_token.toUtf8());

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "[LineWeb] 推送成功";
            emit pushSucceeded();
        } else {
            QString err = reply->readAll();
            qWarning() << "[LineWeb] 推送失败:" << err;
            emit pushFailed(err);
        }
        reply->deleteLater();
    });
}

void LineWebPusher::pushNow()
{
    int totalSeconds = m_db->getTodayTotal();
    QDate today = QDate::currentDate();

    QJsonObject body;
    body["totalSeconds"] = totalSeconds;
    body["date"] = today.toString("yyyy-MM-dd");

    QUrl url(m_endpoint + "/api/health/push");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("X-Screen-Time-Token", m_token.toUtf8());

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(body).toJson());

    QEventLoop loop;
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->isFinished()) {
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "[LineWeb] 最终推送成功";
            emit pushSucceeded();
        } else {
            QString err = reply->readAll();
            qWarning() << "[LineWeb] 最终推送失败:" << err;
            emit pushFailed(err);
        }
    } else {
        reply->abort();
        qWarning() << "[LineWeb] 最终推送超时";
        emit pushFailed("timeout");
    }
    reply->deleteLater();
}
```

- [ ] **Step 3: 记录 — Task 1 不需要单独构建验证，待后续 task 集成后一并验证。**

---

### Task 2: 更新 `src/CMakeLists.txt` 添加新文件和 Qt6::Network

**Files:**
- Modify: `src/CMakeLists.txt`

**Interfaces:**
- Consumes: `src/push/lineweb_pusher.h`, `src/push/lineweb_pusher.cpp`
- Produces: CMake 构建目标包含新源文件和 Qt6::Network 链接

- [ ] **Step 1: 在 `src/CMakeLists.txt` 的 SOURCES 中添加新文件**

```cmake
set(SOURCES
    main.cpp
    database/database_manager.cpp
    tracker/window_tracker.cpp
    icon/app_icon_provider.cpp
    ui/main_window.cpp
    ui/stats_widget.cpp
    ui/app_rank_widget.cpp
    ui/settings_dialog.cpp
    ui/tray_manager.cpp
    export/exporter.cpp
    export/xlsx_writer.cpp
    utility/autostart_helper.cpp
    push/lineweb_pusher.cpp
    ${CMAKE_SOURCE_DIR}/third_party/miniz/miniz_all.cpp
)
```

- [ ] **Step 2: 在 `src/CMakeLists.txt` 的 HEADERS 中添加新文件**

```cmake
set(HEADERS
    database/database_manager.h
    tracker/window_tracker.h
    icon/app_icon_provider.h
    ui/main_window.h
    ui/stats_widget.h
    ui/app_rank_widget.h
    ui/settings_dialog.h
    ui/tray_manager.h
    export/exporter.h
    export/xlsx_writer.h
    utility/autostart_helper.h
    push/lineweb_pusher.h
)
```

- [ ] **Step 3: 在 `target_link_libraries` 中添加 `Qt6::Network`**

```cmake
target_link_libraries(TimeMaster PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Svg
    Qt6::Network
    dwmapi
    shell32
    gdi32
)
```

- [ ] **Step 4: 验证构建通过**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --preset mingw
cmake --build build
```

Expected: 构建通过，无编译/链接错误。

- [ ] **Step 5: 提交**

```bash
git add src/push/lineweb_pusher.h src/push/lineweb_pusher.cpp src/CMakeLists.txt
git commit -m "feat: add LineWebPusher class and CMake integration"
```

---

### Task 3: 在 SettingsDialog 新增「云端同步」标签页

**Files:**
- Modify: `src/ui/settings_dialog.h:41-54` — 新增 LineWeb 成员
- Modify: `src/ui/settings_dialog.cpp:207-257` — 新增标签页构造、loadSettings/saveSettings 扩增

**Interfaces:**
- Consumes: `DatabaseManager::getSetting()` / `setSetting()` 对于 `lineweb_enabled`, `lineweb_endpoint`, `lineweb_token`, `lineweb_interval`
- Produces: `emit settingsChanged()` 已存在，无需新增

- [ ] **Step 1: 修改 `src/ui/settings_dialog.h` — 在 private 区域末尾添加新成员**

在 `m_knownSearch` / `m_ignoredSearch` 之后（第 53 行附近），添加：

```cpp
    QCheckBox *m_linewebEnabled;
    QLineEdit *m_linewebEndpoint;
    QLineEdit *m_linewebToken;
    QPushButton *m_linewebTokenToggle;
    QSpinBox *m_linewebInterval;
    QPushButton *m_linewebTestBtn;
    QLabel *m_linewebStatus;
```

- [ ] **Step 2: 在构造函数中添加第 4 个标签页代码**

找到 `// ---- Tab 3: Personalization ----` 区域的结尾（`m_tabWidget->addTab(personalTab, ...)` 之后），在此前或此后添加新标签页。为简洁，在 Tab 3 的 `addTab` 调用之后、`// ---- Bottom Buttons ----` 之前插入：

```cpp
    // ---- Tab 4: Cloud Sync ----
    QWidget *cloudTab = new QWidget();
    QVBoxLayout *cloudLayout = new QVBoxLayout(cloudTab);
    cloudLayout->setContentsMargins(8, 8, 8, 8);
    cloudLayout->setSpacing(16);

    m_linewebEnabled = new QCheckBox(QString::fromUtf8("\xe5\x90\xaf\xe7\x94\xa8\xe6\x8e\xa8\xe9\x80\x81"), this);
    m_linewebEnabled->setFont(appFont(13));
    cloudLayout->addWidget(m_linewebEnabled);

    QHBoxLayout *endpointRow = new QHBoxLayout();
    endpointRow->addWidget(new QLabel(QString::fromUtf8("API \xe5\x9c\xb0\xe5\x9d\x80:"), this));
    m_linewebEndpoint = new QLineEdit(this);
    m_linewebEndpoint->setPlaceholderText("https://your-server.com");
    endpointRow->addWidget(m_linewebEndpoint, 1);
    cloudLayout->addLayout(endpointRow);

    QHBoxLayout *tokenRow = new QHBoxLayout();
    tokenRow->addWidget(new QLabel("Token:", this));
    m_linewebToken = new QLineEdit(this);
    m_linewebToken->setEchoMode(QLineEdit::Password);
    m_linewebToken->setPlaceholderText("st_...");
    tokenRow->addWidget(m_linewebToken, 1);
    m_linewebTokenToggle = new QPushButton(QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba"), this);
    connect(m_linewebTokenToggle, &QPushButton::clicked, this, [this]() {
        if (m_linewebToken->echoMode() == QLineEdit::Password) {
            m_linewebToken->setEchoMode(QLineEdit::Normal);
            m_linewebTokenToggle->setText(QString::fromUtf8("\xe9\x9a\x90\xe8\x97\x8f"));
        } else {
            m_linewebToken->setEchoMode(QLineEdit::Password);
            m_linewebTokenToggle->setText(QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba"));
        }
    });
    tokenRow->addWidget(m_linewebTokenToggle);
    cloudLayout->addLayout(tokenRow);

    QHBoxLayout *intervalRow = new QHBoxLayout();
    intervalRow->addWidget(new QLabel(QString::fromUtf8("\xe6\x8e\xa8\xe9\x80\x81\xe9\x97\xb4\xe9\x9a\x94\xef\xbc\x88\xe5\x88\x86\xe9\x92\x9f\xef\xbc\x89:"), this));
    m_linewebInterval = new QSpinBox(this);
    m_linewebInterval->setRange(5, 30);
    m_linewebInterval->setValue(10);
    intervalRow->addWidget(m_linewebInterval);
    intervalRow->addStretch();
    cloudLayout->addLayout(intervalRow);

    QHBoxLayout *testRow = new QHBoxLayout();
    m_linewebTestBtn = new QPushButton(QString::fromUtf8("\xe8\xbf\x9e\xe6\x8e\xa5\xe6\xb5\x8b\xe8\xaf\x95"), this);
    connect(m_linewebTestBtn, &QPushButton::clicked, this, [this]() {
        QString token = m_linewebToken->text().trimmed();
        QString endpoint = m_linewebEndpoint->text().trimmed();
        if (token.isEmpty() || endpoint.isEmpty()) {
            QMessageBox::warning(this,
                QString::fromUtf8("\xe9\x85\x8d\xe7\xbd\xae\xe4\xb8\x8d\xe5\xae\x8c\xe6\x95\xb4"),
                QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe5\xa1\xab\xe5\x86\x99 API \xe5\x9c\xb0\xe5\x9d\x80\xe5\x92\x8c Token"));
            return;
        }

        QJsonObject body;
        body["totalSeconds"] = m_db->getTodayTotal();
        body["date"] = QDate::currentDate().toString("yyyy-MM-dd");

        QUrl url(endpoint + "/api/health/push");
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req.setRawHeader("X-Screen-Time-Token", token.toUtf8());

        QNetworkAccessManager *nam = new QNetworkAccessManager(this);
        QNetworkReply *reply = nam->post(req, QJsonDocument(body).toJson());
        connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
            reply->deleteLater();
            nam->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                QMessageBox::information(this,
                    QString::fromUtf8("\xe6\xb5\x8b\xe8\xaf\x95\xe6\x88\x90\xe5\x8a\x9f"),
                    QString::fromUtf8("\xe8\xbf\x9e\xe6\x8e\xa5\xe6\xb5\x8b\xe8\xaf\x95\xe6\x88\x90\xe5\x8a\x9f\xef\xbc\x81"));
            } else {
                QMessageBox::warning(this,
                    QString::fromUtf8("\xe6\xb5\x8b\xe8\xaf\x95\xe5\xa4\xb1\xe8\xb4\xa5"),
                    QString::fromUtf8("\xe8\xbf\x9e\xe6\x8e\xa5\xe5\xa4\xb1\xe8\xb4\xa5\xef\xbc\x9a") + reply->errorString());
            }
        });
    });
    testRow->addWidget(m_linewebTestBtn);
    m_linewebStatus = new QLabel(this);
    m_linewebStatus->setStyleSheet("color: #6B7280; font-size: 12px;");
    testRow->addWidget(m_linewebStatus);
    testRow->addStretch();
    cloudLayout->addLayout(testRow);

    cloudLayout->addStretch();
    m_tabWidget->addTab(cloudTab, QString::fromUtf8("\xe4\xba\x91\xe7\xab\xaf\xe5\x90\x8c\xe6\xad\xa5"));
```

注意：需要在 `settings_dialog.cpp` 顶部添加 include：

```cpp
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
```

- [ ] **Step 3: 在 `loadSettings()` 中添加读取 LineWeb 配置**

在 `loadSettings()` 末尾（`refreshAliasTable();` 之后）添加：

```cpp
    m_linewebEnabled->setChecked(m_db->getSetting("lineweb_enabled", "false") == "true");
    m_linewebEndpoint->setText(m_db->getSetting("lineweb_endpoint", ""));
    m_linewebToken->setText(m_db->getSetting("lineweb_token", ""));
    m_linewebInterval->setValue(m_db->getSetting("lineweb_interval", "10").toInt());
```

- [ ] **Step 4: 在 `saveSettings()` 中添加写入 LineWeb 配置**

在 `saveSettings()` 末尾（`AutoStartHelper::setAutoStart(...)` 之后）添加：

```cpp
    m_db->setSetting("lineweb_enabled", m_linewebEnabled->isChecked() ? "true" : "false");
    m_db->setSetting("lineweb_endpoint", m_linewebEndpoint->text().trimmed());
    m_db->setSetting("lineweb_token", m_linewebToken->text().trimmed());
    m_db->setSetting("lineweb_interval", QString::number(m_linewebInterval->value()));
```

- [ ] **Step 5: 重新构建，确保编译通过**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

Expected: 构建通过。

- [ ] **Step 6: 提交**

```bash
git add src/ui/settings_dialog.h src/ui/settings_dialog.cpp
git commit -m "feat: add LineWeb cloud sync tab in settings dialog"
```

---

### Task 4: 在 `main.cpp` 集成 LineWebPusher

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `LineWebPusher::start()`, `stop()`, `pushNow()`, `reloadSettings()`
- Produces: 完整的推送生命周期

- [ ] **Step 1: 添加 include**

在 `#include "utility/autostart_helper.h"` 之后添加：
```cpp
#include "push/lineweb_pusher.h"
```

- [ ] **Step 2: 在 `DatabaseManager db;` 之后创建 LineWebPusher**

在 `MainWindow window(&db);` 行之前插入：
```cpp
    LineWebPusher pusher(&db);
```

- [ ] **Step 3: 添加设置变更连接**

找到现有的 `QObject::connect(&window, &MainWindow::settingsChanged, [&]() { tracker.reloadSettings(); });` 行（约第 66-68 行），在其**之前**插入：
```cpp
    QObject::connect(&window, &MainWindow::settingsChanged, &pusher, &LineWebPusher::reloadSettings);
```

注意：需要把现有的 lambda connect 保持独立，不能合并。最终该区域应为：

```cpp
    QObject::connect(&window, &MainWindow::settingsChanged, &pusher, &LineWebPusher::reloadSettings);

    QObject::connect(&window, &MainWindow::settingsChanged, [&]() {
        tracker.reloadSettings();
    });
```

- [ ] **Step 4: 修改退出流程 — 在 tracker.stop() 之前添加 pusher.stop() + pushNow()**

找到 `tray.quitApp` 的 lambda 体：

```cpp
    QObject::connect(&tray, &TrayManager::quitApp, [&]() {
        tracker.stop();
        tracker.wait(10000);
        db.close();
        app.quit();
    });
```

改为：

```cpp
    QObject::connect(&tray, &TrayManager::quitApp, [&]() {
        pusher.stop();
        pusher.pushNow();
        tracker.stop();
        tracker.wait(10000);
        db.close();
        app.quit();
    });
```

- [ ] **Step 5: 在 `main()` 末尾添加 `pusher.start()`**

在 `return app.exec();` 之前（`window.refreshData()` 调用和 exit connect 之后）插入：
```cpp
    pusher.start();
```

最终 `main()` 的结尾区域应为：
```cpp
    tray.show();
    window.refreshData();

    QObject::connect(&window, &MainWindow::settingsChanged, &pusher, &LineWebPusher::reloadSettings);

    QObject::connect(&window, &MainWindow::settingsChanged, [&]() {
        tracker.reloadSettings();
    });

    pusher.start();

    return app.exec();
```

- [ ] **Step 6: 重新构建并验证**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

Expected: 构建通过。

- [ ] **Step 7: 提交**

```bash
git add src/main.cpp
git commit -m "feat: integrate LineWebPusher into main lifecycle"
```

---

### Task 5: 编写测试

**Files:**
- Create: `tests/test_lineweb_pusher.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `LineWebPusher::pushNow()`, `pushFailed(const QString &)`, `pushSucceeded()`, `start()`, `stop()`
- Consumes: `DatabaseManager::setSetting()`, `getSetting()`, `getTodayTotal()`

- [ ] **Step 1: 创建 `tests/test_lineweb_pusher.cpp`**

```cpp
#include <cassert>
#include <iostream>
#include <QCoreApplication>
#include <QTemporaryFile>
#include <QSignalSpy>
#include "database/database_manager.h"
#include "push/lineweb_pusher.h"

void test_push_signals_on_invalid_endpoint()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("lineweb_enabled", "true");
    db.setSetting("lineweb_token", "st_test_token");
    db.setSetting("lineweb_endpoint", "http://127.0.0.1:1");
    db.setSetting("lineweb_interval", "10");

    LineWebPusher pusher(&db);
    QSignalSpy failedSpy(&pusher, &LineWebPusher::pushFailed);

    pusher.pushNow();

    assert(failedSpy.count() == 1);
    std::cout << "test_push_signals_on_invalid_endpoint PASS" << std::endl;
}

void test_disabled_does_not_start_timer()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("lineweb_enabled", "false");
    db.setSetting("lineweb_token", "st_test");
    db.setSetting("lineweb_endpoint", "http://example.com");
    db.setSetting("lineweb_interval", "10");

    LineWebPusher pusher(&db);
    pusher.start();

    QSignalSpy successSpy(&pusher, &LineWebPusher::pushSucceeded);
    QSignalSpy failSpy(&pusher, &LineWebPusher::pushFailed);

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    assert(successSpy.count() == 0);
    assert(failSpy.count() == 0);
    std::cout << "test_disabled_does_not_start_timer PASS" << std::endl;
}

void test_missing_token_no_push()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("lineweb_enabled", "true");
    db.setSetting("lineweb_token", "");
    db.setSetting("lineweb_endpoint", "http://example.com");
    db.setSetting("lineweb_interval", "10");

    LineWebPusher pusher(&db);
    pusher.start();

    QSignalSpy successSpy(&pusher, &LineWebPusher::pushSucceeded);
    QSignalSpy failSpy(&pusher, &LineWebPusher::pushFailed);

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    assert(successSpy.count() == 0);
    assert(failSpy.count() == 0);
    std::cout << "test_missing_token_no_push PASS" << std::endl;
}

void test_request_body_format()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("lineweb_enabled", "true");
    db.setSetting("lineweb_token", "st_test");
    db.setSetting("lineweb_endpoint", "http://127.0.0.1:2");
    db.setSetting("lineweb_interval", "10");

    LineWebPusher pusher(&db);
    QSignalSpy failedSpy(&pusher, &LineWebPusher::pushFailed);

    pusher.pushNow();
    assert(failedSpy.count() == 1);
    std::cout << "test_request_body_format PASS" << std::endl;
}

void test_pushnow_timeout()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("lineweb_enabled", "true");
    db.setSetting("lineweb_token", "st_test");
    db.setSetting("lineweb_endpoint", "http://10.255.255.1");
    db.setSetting("lineweb_interval", "10");

    LineWebPusher pusher(&db);
    QSignalSpy failedSpy(&pusher, &LineWebPusher::pushFailed);

    pusher.pushNow();

    assert(failedSpy.count() >= 1);
    std::cout << "test_pushnow_timeout PASS" << std::endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    test_push_signals_on_invalid_endpoint();
    test_disabled_does_not_start_timer();
    test_missing_token_no_push();
    test_request_body_format();
    test_pushnow_timeout();
    std::cout << "All LineWeb pusher tests passed!" << std::endl;
    return 0;
}
```

- [ ] **Step 2: 在 `tests/CMakeLists.txt` 添加测试目标**

在 `endif()` 之前添加：

```cmake
    add_executable(test_lineweb_pusher
        test_lineweb_pusher.cpp
        ${CMAKE_SOURCE_DIR}/src/database/database_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/push/lineweb_pusher.cpp
    )
    target_include_directories(test_lineweb_pusher PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_lineweb_pusher PRIVATE Qt6::Core Qt6::Sql Qt6::Network)
    add_test(NAME test_lineweb_pusher COMMAND test_lineweb_pusher)
```

- [ ] **Step 3: 构建测试**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --preset mingw -DBUILD_TESTING=ON
cmake --build build
```

- [ ] **Step 4: 运行测试**

```powershell
$env:QT_PLUGIN_PATH = "D:\AICOP\requirements\QT6\6.11.1\mingw_64\plugins"
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
.\build\tests\test_lineweb_pusher.exe
```

Expected: 5/5 测试 PASS。

- [ ] **Step 5: 提交**

```bash
git add tests/test_lineweb_pusher.cpp tests/CMakeLists.txt
git commit -m "feat: add LineWeb pusher tests"
```
