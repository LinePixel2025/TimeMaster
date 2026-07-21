#include "window_tracker.h"
#include "database/database_manager.h"

#include <windows.h>

static const QMap<QString, QString> APP_NAME_MAP = {
    {"chrome.exe", "Chrome"},
    {"msedge.exe", "Edge"},
    {"firefox.exe", "Firefox"},
    {"Code.exe", "VS Code"},
    {"WeChat.exe", "WeChat"},
    {"WeChatWork.exe", "WeCom"},
    {"sublime_text.exe", "Sublime Text"},
    {"cmd.exe", "Command Prompt"},
    {"WindowsTerminal.exe", "Terminal"},
    {"explorer.exe", "File Explorer"},
    {"OUTLOOK.EXE", "Outlook"},
    {"WINWORD.EXE", "Word"},
    {"EXCEL.EXE", "Excel"},
    {"POWERPNT.EXE", "PowerPoint"},
    {"wmplayer.exe", "Windows Media Player"},
    {"Spotify.exe", "Spotify"},
    {"Obsidian.exe", "Obsidian"},
    {"notepad.exe", "Notepad"},
    {"notepad++.exe", "Notepad++"},
    {"pycharm64.exe", "PyCharm"},
    {"idea64.exe", "IntelliJ IDEA"},
    {"clion64.exe", "CLion"},
    {"goland64.exe", "GoLand"},
    {"devenv.exe", "Visual Studio"},
    {"typora.exe", "Typora"},
    {"Foxmail.exe", "Foxmail"},
    {"DingTalk.exe", "DingTalk"},
    {"Lark.exe", "Lark"},
    {"Slack.exe", "Slack"},
    {"Discord.exe", "Discord"},
    {"Telegram.exe", "Telegram"},
    {"PotPlayer.exe", "PotPlayer"},
    {"eclipse.exe", "Eclipse"},
    {"AndroidStudio.exe", "Android Studio"},
    {"Tableau.exe", "Tableau"},
    {"Figma.exe", "Figma"},
    {"Photoshop.exe", "Photoshop"},
    {"winword.exe", "Word"},
    {"excel.exe", "Excel"},
    {"powerpnt.exe", "PowerPoint"},
    {"outlook.exe", "Outlook"},
};

WindowTracker::WindowTracker(DatabaseManager *db, QObject *parent)
    : QThread(parent), m_db(db)
{
    reloadSettings();
}

void WindowTracker::stop()
{
    m_running.storeRelaxed(0);
}

QString WindowTracker::classifyApp(const QString &processName)
{
    QMutexLocker lock(&m_settingsMutex);
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

void WindowTracker::run()
{
    if (!m_running.testAndSetOrdered(0, 1))
        return;
    while (m_running.loadRelaxed()) {
        tick();
        for (int i = 0; i < 5 && m_running.loadRelaxed(); ++i)
            msleep(static_cast<unsigned long>(m_pollInterval * 200));
    }
    closeCurrentSession(QDateTime::currentDateTime());
}

void WindowTracker::tick()
{
    QDateTime now = QDateTime::currentDateTime();
    WindowInfo info = getForegroundWindowInfo();

    bool trackingEnabled;
    bool isIgnored = false;
    {
        QMutexLocker lock(&m_settingsMutex);
        trackingEnabled = m_trackingEnabled;
        if (info.pid != 0) {
            int pos = info.processName.lastIndexOf('\\');
            QString name = (pos >= 0) ? info.processName.mid(pos + 1) : info.processName;
            isIgnored = m_ignoredApps.contains(name);
        }
    }

    if (!trackingEnabled) {
        m_pendingPid = 0;
        return;
    }

    if (info.pid != 0 && isIgnored)
        return;

    if (info.pid == 0) {
        m_idleSeconds += POLL_INTERVAL;
        if (m_idleSeconds >= m_idleThreshold && !m_isIdle) {
            m_isIdle = true;
            closeCurrentSession(now);
            m_pendingPid = 0;
        }
        return;
    }

    m_idleSeconds = 0;
    if (m_isIdle) {
        m_isIdle = false;
    }

    bool windowChanged = (info.pid != m_currentPid || info.windowTitle != m_currentTitle);
    bool hasPending = (m_pendingPid != 0);

    if (windowChanged) {
        closeCurrentSession(now);

        if (hasPending) {
            int pendingDuration = static_cast<int>(m_pendingStart.secsTo(now));
            if (pendingDuration > 0) {
                m_db->insertSession(
                    m_pendingProcessName, m_pendingTitle, m_pendingAppName,
                    m_pendingStart, now, pendingDuration);
            }
            m_pendingPid = 0;
        }

        if (m_minTrackingSeconds <= 0) {
            m_currentPid = info.pid;
            m_currentTitle = info.windowTitle;
            m_sessionStart = now;
            m_currentSessionId = m_db->insertSession(
                info.processName, info.windowTitle, info.appName,
                now, QDateTime(), 0);
            m_pendingPid = 0;
            emit activeWindowChanged(info.processName, info.windowTitle, info.appName);
        } else {
            m_currentPid = info.pid;
            m_currentTitle = info.windowTitle;
            m_pendingPid = info.pid;
            m_pendingTitle = info.windowTitle;
            m_pendingProcessName = info.processName;
            m_pendingAppName = info.appName;
            m_pendingStart = now;
        }
        return;
    }

    if (hasPending && info.pid == m_pendingPid && info.windowTitle == m_pendingTitle) {
        if (m_pendingStart.secsTo(now) >= m_minTrackingSeconds) {
            m_currentPid = m_pendingPid;
            m_currentTitle = m_pendingTitle;
            m_sessionStart = m_pendingStart;
            m_currentSessionId = m_db->insertSession(
                m_pendingProcessName, m_pendingTitle, m_pendingAppName,
                m_pendingStart, QDateTime(), static_cast<int>(m_pendingStart.secsTo(now)));
            m_pendingPid = 0;
            emit activeWindowChanged(m_pendingProcessName, m_pendingTitle, m_pendingAppName);
        }
        return;
    }

    if (m_currentSessionId >= 0) {
        int duration = static_cast<int>(m_sessionStart.secsTo(now));
        m_db->updateSessionDuration(m_currentSessionId, duration);
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

void WindowTracker::reloadSettings()
{
    QMutexLocker lock(&m_settingsMutex);
    m_trackingEnabled = (m_db->getSetting("tracking_enabled", "true") == "true");
    m_pollInterval = m_db->getSetting("poll_interval", "1").toFloat();
    m_idleThreshold = m_db->getSetting("idle_threshold", "60").toFloat();
    m_minTrackingSeconds = m_db->getSetting("min_tracking_seconds", "0").toFloat();

    QMap<int, QString> ignored = m_db->getIgnoredApps();
    m_ignoredApps.clear();
    for (auto it = ignored.begin(); it != ignored.end(); ++it)
        m_ignoredApps.insert(it.value());

    m_aliases = m_db->getAppAliases();
}
