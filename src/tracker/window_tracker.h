#ifndef WINDOW_TRACKER_H
#define WINDOW_TRACKER_H

#include <QThread>
#include <QDateTime>
#include <QString>
#include <QMap>
#include <QAtomicInt>
#include <QSet>
#include <QMutex>

class DatabaseManager;

class WindowTracker : public QThread
{
    Q_OBJECT
public:
    explicit WindowTracker(DatabaseManager *db, QObject *parent = nullptr);
    void stop();
    QString classifyApp(const QString &processName);
    void reloadSettings();

signals:
    void activeWindowChanged(const QString &processName, const QString &windowTitle,
                             const QString &appName);
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

    unsigned long m_pendingPid = 0;
    QString m_pendingTitle;
    QString m_pendingProcessName;
    QString m_pendingAppName;
    QDateTime m_pendingStart;

    QSet<QString> m_ignoredApps;
    QMap<QString, QString> m_aliases;
    bool m_trackingEnabled = true;
    float m_pollInterval = POLL_INTERVAL;
    float m_idleThreshold = IDLE_THRESHOLD;
    float m_minTrackingSeconds = 0;
    QMutex m_settingsMutex;

    static constexpr float POLL_INTERVAL = 1.0f;
    static constexpr float IDLE_THRESHOLD = 60.0f;
};

#endif // WINDOW_TRACKER_H
