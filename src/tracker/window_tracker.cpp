#include "window_tracker.h"
#include "database/database_manager.h"
#include "utility/process_identity.h"

#include <QElapsedTimer>
#include <windows.h>
#include <winver.h>
#include <cwchar>

static const QMap<QString, QString> APP_NAME_MAP = {
    {"chrome.exe", "Chrome"},
    {"msedge.exe", "Edge"},
    {"firefox.exe", "Firefox"},
    {"code.exe", "VS Code"},
    {"timemaster.exe", "Time Master"},
    {"wechat.exe", "WeChat"},
    {"wechatwork.exe", "WeCom"},
    {"sublime_text.exe", "Sublime Text"},
    {"cmd.exe", "Command Prompt"},
    {"windowsterminal.exe", "Terminal"},
    {"explorer.exe", "File Explorer"},
    {"outlook.exe", "Outlook"},
    {"winword.exe", "Word"},
    {"excel.exe", "Excel"},
    {"powerpnt.exe", "PowerPoint"},
    {"wmplayer.exe", "Windows Media Player"},
    {"spotify.exe", "Spotify"},
    {"obsidian.exe", "Obsidian"},
    {"notepad.exe", "Notepad"},
    {"notepad++.exe", "Notepad++"},
    {"pycharm64.exe", "PyCharm"},
    {"idea64.exe", "IntelliJ IDEA"},
    {"clion64.exe", "CLion"},
    {"goland64.exe", "GoLand"},
    {"devenv.exe", "Visual Studio"},
    {"typora.exe", "Typora"},
    {"foxmail.exe", "Foxmail"},
    {"dingtalk.exe", "DingTalk"},
    {"lark.exe", "Lark"},
    {"slack.exe", "Slack"},
    {"discord.exe", "Discord"},
    {"telegram.exe", "Telegram"},
    {"potplayer.exe", "PotPlayer"},
    {"eclipse.exe", "Eclipse"},
    {"androidstudio.exe", "Android Studio"},
    {"tableau.exe", "Tableau"},
    {"figma.exe", "Figma"},
    {"photoshop.exe", "Photoshop"},
};

static QString fallbackName(const QString &key)
{
    const int dotPos = key.lastIndexOf('.');
    return dotPos >= 0 ? key.left(dotPos) : key;
}

// 从 exe 的 Windows 版本资源中读取 FileDescription 字段,大多数应用自带该显示名
static QString readFileDescription(const QString &exePath)
{
    if (exePath.isEmpty())
        return {};

    const std::wstring path = exePath.toStdWString();
    DWORD versionHandle = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &versionHandle);
    if (size == 0)
        return {};

    QByteArray buffer(size, Qt::Uninitialized);
    if (!GetFileVersionInfoW(path.c_str(), versionHandle, size, buffer.data()))
        return {};

    // 版本资源可能按语言/代码页分块,遍历取第一条可用的 FileDescription
    struct LangAndCodePage { WORD language; WORD codePage; };
    LangAndCodePage *translations = nullptr;
    UINT translationBytes = 0;
    if (!VerQueryValueW(buffer.data(), L"\\VarFileInfo\\Translation",
                        reinterpret_cast<void **>(&translations), &translationBytes))
        return {};

    const UINT translationCount = translationBytes / sizeof(LangAndCodePage);
    for (UINT i = 0; i < translationCount; ++i) {
        wchar_t blockName[64] = {0};
        swprintf(blockName, 64, L"\\StringFileInfo\\%04x%04x\\FileDescription",
                 translations[i].language, translations[i].codePage);
        wchar_t *description = nullptr;
        UINT descriptionLen = 0;
        if (VerQueryValueW(buffer.data(), blockName,
                           reinterpret_cast<void **>(&description), &descriptionLen) &&
            description != nullptr) {
            const QString result = QString::fromWCharArray(description).trimmed();
            if (!result.isEmpty())
                return result;
        }
    }
    return {};
}

WindowTracker::WindowTracker(DatabaseManager *db, QObject *parent)
    : QThread(parent), m_db(db)
{
    reloadSettings();
}

void WindowTracker::stop()
{
    m_running.storeRelease(0);
    m_waitCondition.wakeAll();
}

QString WindowTracker::classifyApp(const QString &processName)
{
    const QString key = ProcessIdentity::normalizeKey(processName);
    {
        QMutexLocker lock(&m_settingsMutex);
        const auto alias = m_aliases.constFind(key);
        if (alias != m_aliases.cend())
            return alias.value();
    }

    const auto builtIn = APP_NAME_MAP.constFind(key);
    if (builtIn != APP_NAME_MAP.cend())
        return builtIn.value();

    // 版本资源读取结果按进程键缓存;已确认缺失的进程直接走兜底,不做重复 I/O
    {
        QMutexLocker lock(&m_settingsMutex);
        const auto cached = m_descriptionCache.constFind(key);
        if (cached != m_descriptionCache.cend())
            return cached.value();
        if (m_descriptionMissed.contains(key))
            return fallbackName(key);
    }

    // 文件 I/O 在锁外执行,避免持锁期间读取 exe 文件
    const QString description = readFileDescription(processName);
    QMutexLocker lock(&m_settingsMutex);
    if (!description.isEmpty()) {
        m_descriptionCache.insert(key, description);
        return description;
    }
    m_descriptionMissed.insert(key);
    return fallbackName(key);
}

void WindowTracker::run()
{
    if (!m_running.testAndSetOrdered(0, 1))
        return;

    DatabaseManager threadDb(m_db->databasePath());
    TrackingEngine engine(&threadDb);
    QElapsedTimer monotonicClock;
    monotonicClock.start();

    while (m_running.loadAcquire()) {
        TrackingConfig config;
        quint64 configRevision = 0;
        {
            QMutexLocker lock(&m_settingsMutex);
            config = m_config;
            configRevision = m_configRevision;
        }

        const WindowInfo info = getForegroundWindowInfo();
        TrackingSample sample;
        sample.wallTime = QDateTime::currentDateTime();
        sample.monotonicMs = monotonicClock.elapsed();
        sample.idleMs = getIdleMilliseconds();
        sample.foregroundValid = info.pid != 0;
        sample.processName = info.processName;
        sample.processKey = info.processKey;
        sample.windowTitle = info.windowTitle;
        sample.appName = info.appName;

        const QString oldProcessKey = engine.currentProcessKey();
        const TrackingEngine::State oldState = engine.state();
        engine.process(sample, config);
        const TrackingEngine::State newState = engine.state();
        if (newState == TrackingEngine::State::Active) {
            if (oldProcessKey != engine.currentProcessKey() ||
                oldState != TrackingEngine::State::Active)
                emit activeWindowChanged(info.processName, info.windowTitle, info.appName);
        } else if (oldState == TrackingEngine::State::Active) {
            emit activeWindowChanged(QString(), QString(), QString());
        }

        QMutexLocker lock(&m_settingsMutex);
        if (m_running.loadAcquire() && configRevision == m_configRevision)
            m_waitCondition.wait(&m_settingsMutex, config.pollIntervalMs);
    }

    // 收尾写库失败(busy 超时等)时短暂等待后重试一次;仍失败则接受丢失,
    // 与"崩溃最多丢一个 flush 周期"的既有语义一致
    if (!engine.stop(QDateTime::currentDateTime(), monotonicClock.elapsed())) {
        QThread::msleep(100);
        engine.stop(QDateTime::currentDateTime(), monotonicClock.elapsed());
    }
}

QString WindowTracker::readWindowTitle(void *hwndPointer)
{
    wchar_t title[512] = {0};
    DWORD_PTR result = 0;
    // 用 SendMessageTimeout 而非 GetWindowTextW:两者底层同为 WM_GETTEXT 跨进程
    // 读取,但后者会无限等待目标消息循环——前台程序未响应时会卡死整个轮询线程,
    // 导致会话无法收尾、状态芯片冻结。超时/挂起按空标题处理,由调用方兜底。
    if (SendMessageTimeoutW(static_cast<HWND>(hwndPointer), WM_GETTEXT, 512,
                           reinterpret_cast<LPARAM>(title),
                           SMTO_ABORTIFHUNG, 150, &result) == 0)
        return {};
    return QString::fromWCharArray(title);
}

WindowTracker::WindowInfo WindowTracker::getForegroundWindowInfo()
{
    const HWND hwnd = GetForegroundWindow();
    DWORD pid = 0;
    if (hwnd != nullptr)
        GetWindowThreadProcessId(hwnd, &pid);
    if (hwnd == nullptr || pid == 0) {
        m_lastHwnd = nullptr;
        m_lastPid = 0;
        return {0, "", "", "", ""};
    }

    // (hwnd, pid) 同时相同 → 同一活进程的同一窗口,复用上轮进程身份
    QString processName;
    QString processKey;
    QString appName;
    if (hwnd == m_lastHwnd && pid == m_lastPid) {
        processName = m_lastProcessName;
        processKey = m_lastProcessKey;
        appName = m_lastAppName;
    } else {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess) {
            wchar_t exePath[32768] = {0};
            DWORD size = 32768;
            if (QueryFullProcessImageNameW(hProcess, 0, exePath, &size))
                processName = QString::fromWCharArray(exePath, static_cast<qsizetype>(size));
            CloseHandle(hProcess);
        }
        if (processName.isEmpty())
            processName = QString("pid_%1").arg(pid);

        processKey = ProcessIdentity::normalizeKey(processName);
        appName = classifyApp(processName);
        m_lastHwnd = hwnd;
        m_lastPid = pid;
        m_lastProcessName = processName;
        m_lastProcessKey = processKey;
        m_lastAppName = appName;
    }

    QString windowTitle = readWindowTitle(hwnd);
    if (windowTitle.isEmpty())
        windowTitle = processName;

    return {pid, processName, processKey, windowTitle, appName};
}

qint64 WindowTracker::getIdleMilliseconds() const
{
    LASTINPUTINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetLastInputInfo(&info))
        return 0;
    const DWORD elapsed = GetTickCount() - info.dwTime;
    return static_cast<qint64>(elapsed);
}

void WindowTracker::reloadSettings()
{
    TrackingConfig config;
    config.enabled = m_db->getSetting("tracking_enabled", "true") == "true";
    config.pollIntervalMs = qMax(100, qRound(
        m_db->getSetting("poll_interval", "1").toDouble() * 1000.0));
    config.idleThresholdMs = qMax(1000, qRound64(
        m_db->getSetting("idle_threshold", "60").toDouble() * 1000.0));
    config.minTrackingMs = qMax(0, qRound64(
        m_db->getSetting("min_tracking_seconds", "0").toDouble() * 1000.0));

    const QMap<int, QString> ignored = m_db->getIgnoredApps();
    for (auto it = ignored.cbegin(); it != ignored.cend(); ++it)
        config.ignoredProcessKeys.insert(ProcessIdentity::normalizeKey(it.value()));
    const QMap<QString, QString> aliases = m_db->getAppAliases();

    {
        QMutexLocker lock(&m_settingsMutex);
        m_config = config;
        m_aliases = aliases;
        ++m_configRevision;
    }
    m_waitCondition.wakeAll();
}
