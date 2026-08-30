#ifndef WINDOW_TRACKER_H
#define WINDOW_TRACKER_H

#include <QThread>
#include <QDateTime>
#include <QString>
#include <QMap>
#include <QSet>
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
    /// 前台应用变化。appName 为空表示空闲、忽略或追踪已关闭。
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
    /// 带超时保护的前台窗口标题读取(参数为 HWND,以 void* 出现避免头文件依赖 windows.h)
    static QString readWindowTitle(void *hwnd);
    qint64 getIdleMilliseconds() const;

    DatabaseManager *m_db;
    // 上一轮前台窗口的 HWND(以 void* 存储)与 pid:两者同时相同即可判定
    // 为同一活进程的同一窗口,复用其进程名/进程键/应用分类,跳过
    // OpenProcess、路径查询与文件 I/O。仅轮询线程访问,无需加锁。
    void *m_lastHwnd = nullptr;
    unsigned long m_lastPid = 0;
    QString m_lastProcessName;
    QString m_lastProcessKey;
    QString m_lastAppName;
    // QAtomicInt 默认构造不置零,而 run() 用 testAndSetOrdered(0,1) 抢占运行权,必须显式初始化为 0
    QAtomicInt m_running = 0;
    TrackingConfig m_config;
    QMap<QString, QString> m_aliases;
    // 进程键 → exe 版本资源 FileDescription 的缓存,避免轮询时反复读取同一文件
    QMap<QString, QString> m_descriptionCache;
    // 已确认没有 FileDescription 的进程键,防止对同一 exe 反复失败重试
    QSet<QString> m_descriptionMissed;
    QMutex m_settingsMutex;
    QWaitCondition m_waitCondition;
    quint64 m_configRevision = 0;
};

#endif // WINDOW_TRACKER_H
