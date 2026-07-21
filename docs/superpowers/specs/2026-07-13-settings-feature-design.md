# Settings Feature Design

## Overview

为 Time Master 添加设置功能：主界面 header 增加齿轮图标 → 弹出模态设置对话框 → 用户可配置应用屏蔽、自定义名称、追踪参数、个性化选项。所有设置持久化到 SQLite。

---

## 1. Database Schema

在 `DatabaseManager::migrate()` 中新增 3 张 `IF NOT EXISTS` 表：

```sql
CREATE TABLE IF NOT EXISTS settings (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS ignored_apps (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    process_name TEXT NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS app_aliases (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    process_name TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL
);
```

### 默认设置（首次迁移时插入）

| key | value |
|-----|-------|
| `tracking_enabled` | `true` |
| `poll_interval` | `1` |
| `idle_threshold` | `60` |
| `auto_start` | `false` |

用 `INSERT OR IGNORE` 避免覆盖已有值。

### DatabaseManager 新增方法

所有方法遵循现有模式：`QMutexLocker lock(&m_mutex)` + QSqlQuery。

```cpp
// 通用设置
QString getSetting(const QString &key, const QString &defaultValue = QString());
void setSetting(const QString &key, const QString &value);

// 屏蔽应用
QSet<QString> getIgnoredApps();
int addIgnoredApp(const QString &processName);
void removeIgnoredApp(int id);

// 应用别名
QMap<QString, QString> getAppAliases();
int setAppAlias(const QString &processName, const QString &displayName);
void removeAppAlias(int id);

// 已知应用列表
QStringList getAllKnownProcessNames();
```

---

## 2. New Files

| 文件 | 作用 |
|------|------|
| `src/ui/settings_dialog.h` | SettingsDialog 声明 |
| `src/ui/settings_dialog.cpp` | SettingsDialog 实现 |

无新增类（无需 SettingsManager），设置读写直接通过 DatabaseManager。

---

## 3. UI Structure

### 3.1 主窗口 Header

在导出按钮前插入设置按钮。文字 `⚙`，透明背景 + 圆形悬停效果。点击弹出 SettingsDialog。

### 3.2 SettingsDialog

- 继承 `QDialog`，500×450，居中于父窗口
- `QTabWidget` 分 3 个标签页
- Font: `Microsoft YaHei`, 背景: `#F0F2F5`

#### Tab 1: 应用管理
- 已知应用列表（`QListWidget`，多选）+ "加入屏蔽清单" 按钮
- 已屏蔽应用列表（每行带移除按钮）
- 应用别名表（`QTableWidget`）+ 添加别名按钮

#### Tab 2: 追踪设置
- 启用追踪（`QCheckBox`）
- 轮询间隔（`QSpinBox`，1-10s）
- 空闲判定时间（`QSpinBox`，10-600s）

#### Tab 3: 个性化
- 开机自启（`QCheckBox`，后端暂未实现）

### 底部按钮栏
[取消] [保存]

---

## 4. Integration

### 4.1 WindowTracker 变更

新增成员变量作为运行时缓存：
- `m_ignoredApps` (QSet<QString>)
- `m_aliases` (QMap<QString, QString>)
- `m_trackingEnabled` (bool)
- `m_pollInterval` / `m_idleThreshold` 替代静态常量

新增 `reloadSettings()` 方法从 DB 重新加载配置。

`tick()` 中检查 tracking_enabled 和屏蔽清单。
`classifyApp()` 优先查别名缓存再回退到硬编码映射。

### 4.2 信号连接

`SettingsDialog` 保存后发出 `settingsChanged` → `tracker.reloadSettings()` → `window.refreshData()`。

### 4.3 UI 组件

AppRankWidget / StatsWidget 无需修改——屏蔽在追踪层处理，别名字段在写入时已替换。

---

## 5. Files Changed

| 文件 | 变更类型 |
|------|----------|
| `src/database/database_manager.h` | 新增方法声明 |
| `src/database/database_manager.cpp` | 新增 Schema + 方法实现 |
| `src/tracker/window_tracker.h` | 新增缓存成员 + reloadSettings() |
| `src/tracker/window_tracker.cpp` | 过滤逻辑 + 别名逻辑 + classifyApp 修改 |
| `src/ui/main_window.h` | 新增 onSettings() slot |
| `src/ui/main_window.cpp` | 设置按钮 + onSettings 实现 |
| `src/main.cpp` | 信号连接 |
| `src/ui/settings_dialog.h` | 新文件 |
| `src/ui/settings_dialog.cpp` | 新文件 |
| `src/CMakeLists.txt` | 添加 settings_dialog.cpp |
