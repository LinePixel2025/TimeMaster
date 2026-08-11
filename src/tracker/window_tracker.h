#ifndef WINDOW_TRACKER_H
#define WINDOW_TRACKER_H

#include <QThread>
#include <QDateTime>
#include <QString>
#include <QMap>
#include <QAtomicInt>
#include <QMutex>
#include <QWaitCondition>
#include "tracker/tracking_engine.h"

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
    struct WindowInfo {
        unsigned long pid;
        QString processName;
        QString processKey;
        QString windowTitle;
        QString appName;
    };
    WindowInfo getForegroundWindowInfo();
    qint64 getIdleMilliseconds() const;

    DatabaseManager *m_db;
    // QAtomicInt 默认构造不置零,而 run() 用 testAndSetOrdered(0,1) 抢占运行权,必须显式初始化为 0
    QAtomicInt m_running = 0;
    TrackingConfig m_config;
    QMap<QString, QString> m_aliases;
    QMutex m_settingsMutex;
    QWaitCondition m_waitCondition;
    quint64 m_configRevision = 0;
};

#endif // WINDOW_TRACKER_H
