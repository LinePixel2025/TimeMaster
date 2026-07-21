# LineWeb 推送功能 — 设计规格

日期：2026-07-14

## 概述

为 Time Master 添加定时推送每日屏幕使用时间到 LineWeb 的功能。基于 HTTP POST，通过 `X-Screen-Time-Token` 头认证，每 N 分钟发送 `{totalSeconds, date}` 到指定端点。

## 推送协议

```
POST /api/health/push
Content-Type: application/json
X-Screen-Time-Token: st_<64 hex chars>

{"totalSeconds": 12345, "date": "2026-07-14"}
```

- 成功：200 `{"message": "已同步"}`
- 400：参数非法（totalSeconds 超出范围或 date 格式错误）
- 401：Token 无效或过期

## 新增模块：`src/push/`

### 文件

- `src/push/lineweb_pusher.h`
- `src/push/lineweb_pusher.cpp`

### 类接口

```cpp
class LineWebPusher : public QObject {
    Q_OBJECT
public:
    explicit LineWebPusher(DatabaseManager *db, QObject *parent = nullptr);

    void start();          // 从 DB 读取配置，若启用则启动定时器
    void stop();           // 停止定时器
    void pushNow();        // 立即推送一次（同步，带 5 秒超时）
    void reloadSettings(); // 热加载配置，配置变更时重启定时器

signals:
    void pushSucceeded();
    void pushFailed(const QString &error);

private:
    void doPush();  // 执行 HTTP POST

    DatabaseManager *m_db;
    QNetworkAccessManager *m_nam;
    QTimer *m_timer;
    QString m_token;
    QString m_endpoint;
    int m_intervalMinutes = 10;
    bool m_enabled = false;
};
```

### 行为

- **`doPush()`**：读取 `m_db->getTodayTotal()`，构造 JSON body，POST 到 `{endpoint}/api/health/push`，设置 `X-Screen-Time-Token` 头。
- **失败处理**：仅 `qWarning()` 打日志 + emit `pushFailed()`，不重试。下次定时器触发时自然覆盖。
- **退出流程**：`stop()` 停定时器 → `pushNow()` 使用 `QEventLoop` + 5 秒超时做最终同步推送，失败不阻塞退出。
- **线程安全**：所有操作在主线程执行，`doPush()` 中调用 `m_db->getTodayTotal()` 走 DB 的 `QMutexLocker`。

## 设置 UI：新增「云端同步」标签页

在 `SettingsDialog` 中添加第 4 个 QTabWidget 页面。

### 控件

| 控件 | 类型 | 说明 |
|------|------|------|
| 启用推送 | `QCheckBox` | 勾选后开启定时推送 |
| API 地址 | `QLineEdit` | placeholder: `https://your-server.com` |
| Token | `QLineEdit` + `QPushButton` | `echoMode=Password`，按钮切换显示/隐藏 |
| 推送间隔 | `QSpinBox` | 范围 5-30，默认 10，单位分钟 |
| 连接测试 | `QPushButton` | 即时 POST 一次，`QMessageBox` 提示成功/失败 |
| 状态标签 | `QLabel` | 显示上次推送时间和结果文本 |

### DB Setting Keys

| Key | 默认值 |
|-----|--------|
| `lineweb_enabled` | `"false"` |
| `lineweb_endpoint` | `""` |
| `lineweb_token` | `""` |
| `lineweb_interval` | `"10"` |

### 行为

- `loadSettings()` 从 DB 读取并填充控件
- `saveSettings()` 将控件值写入 DB
- 「连接测试」按钮：直接创建临时 `QNetworkReply`，POST 测试请求到配置的端点
- 「显示/隐藏」按钮切换 Token 输入框的 `echoMode`

## main.cpp 集成

### 创建与信号

```cpp
LineWebPusher pusher(&db);

// 设置变更时热加载推送配置
QObject::connect(&window, &MainWindow::settingsChanged,
                 &pusher, &LineWebPusher::reloadSettings);

// 退出流程
QObject::connect(&tray, &TrayManager::quitApp, [&]() {
    pusher.stop();
    pusher.pushNow();    // 最终同步推送
    tracker.stop();
    tracker.wait(10000);
    db.close();
    app.quit();
});
```

### 启动

`main()` 末尾调用 `pusher.start()`，在 `window.refreshData()` 之后。

### 退出序

1. `pusher.stop()` — 停止定时器
2. `pusher.pushNow()` — 最终推送（同步，5 秒超时）
3. `tracker.stop()` + `wait()`
4. `db.close()`
5. `app.quit()`

## CMakeLists.txt 变更

```
add: push/lineweb_pusher.cpp (SOURCES)
add: push/lineweb_pusher.h   (HEADERS)
add: Qt6::Network            (target_link_libraries)
```

## 测试

在 `tests/` 下新增 `test_lineweb_pusher.cpp`，测试要点：

1. 构造有效请求体（totalSeconds 和 date 格式）
2. Token header 格式正确
3. 无效 endpoint 时 pushFailed 信号触发
4. 配置为 disabled 时 start() 不启动定时器
5. pushNow() 在 5 秒内超时

使用 `QTemporaryFile` 数据库 + `QCoreApplication`，同现有测试模式。

## 已知约束

- 不引入新的三方库（Qt Network 模块已够用）
- Token 存储在 SQLite 中（明文，本地数据库已受操作系统权限保护）
- 推送失败不计入错误计数，不展示用户可见的弹窗（仅托盘 tooltip 可选提示）
