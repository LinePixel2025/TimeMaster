# Task 6: WindowTracker filtering + aliases + reloadSettings

**Files:**
- Modify: `src/tracker/window_tracker.h` — add member variables and method declaration
- Modify: `src/tracker/window_tracker.cpp` — add reloadSettings, modify classifyApp, modify tick(), modify constructor

## Requirement

### 1. window_tracker.h changes

In the `private:` section, after `bool m_isIdle = false;`, add:

```cpp
    QSet<QString> m_ignoredApps;
    QMap<QString, QString> m_aliases;
    bool m_trackingEnabled = true;
    float m_pollInterval = POLL_INTERVAL;
    float m_idleThreshold = IDLE_THRESHOLD;
```

In the `public:` section, after `static QString classifyApp(...);`, add:

```cpp
    void reloadSettings();
```

### 2. window_tracker.cpp changes

Modify the constructor to call reloadSettings. Change:

```cpp
WindowTracker::WindowTracker(DatabaseManager *db, QObject *parent)
    : QThread(parent), m_db(db)
{
}
```

To:

```cpp
WindowTracker::WindowTracker(DatabaseManager *db, QObject *parent)
    : QThread(parent), m_db(db)
{
    reloadSettings();
}
```

Modify `classifyApp` to check aliases first. Replace:

```cpp
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
```

With:

```cpp
QString WindowTracker::classifyApp(const QString &processName)
{
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

Modify `tick()` to check tracking_enabled and ignored apps. After `WindowInfo info = getForegroundWindowInfo();` add:

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

At the end of the file, add `reloadSettings`:

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

## Verification

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

Expected: compiles and links.

## Global Constraints

- All `DatabaseManager` methods must acquire `QMutexLocker lock(&m_mutex)`
- Font: `Microsoft YaHei`, background: `#F0F2F5`
- No MICA/transparent backgrounds
