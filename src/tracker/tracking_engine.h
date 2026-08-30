#ifndef TRACKING_ENGINE_H
#define TRACKING_ENGINE_H

#include <QDateTime>
#include <QSet>
#include <QString>

#include "tracker/tracking_store.h"

struct TrackingConfig {
    bool enabled = true;
    int pollIntervalMs = 1000;
    int idleThresholdMs = 60000;
    // 用户最短追踪时长。即使为 0,引擎仍内置一个轮询周期级的 settle-in 防抖
    // (见 TrackingEngine::activationThresholdMs),存活不足一个轮询周期的
    // 瞬时窗口不会落库。
    int minTrackingMs = 0;
    // 活跃会话写库周期:活跃期间不每次轮询都写库,间隔达到该值时
    // 才调用 updateSessionDuration 持久化一次,降低 SQLite 写入频率。
    int persistIntervalMs = 30000;
    QSet<QString> ignoredProcessKeys;
};

struct TrackingSample {
    QDateTime wallTime;
    qint64 monotonicMs = 0;
    qint64 idleMs = 0;
    bool foregroundValid = false;
    QString processName;
    QString processKey;
    QString windowTitle;
    QString appName;
};

class TrackingEngine
{
public:
    enum class State { Stopped, Pending, Active, Idle };

    explicit TrackingEngine(TrackingStore *store);

    void process(const TrackingSample &sample, const TrackingConfig &config);
    /// 收尾当前会话。返回 false 表示持久化收尾失败,会话状态被保留,
    /// 调用方应以新时刻重试。
    bool stop(const QDateTime &wallTime, qint64 monotonicMs);
    State state() const;
    QString currentProcessKey() const;

private:
    static int activationThresholdMs(const TrackingConfig &config);

    void beginCandidate(const TrackingSample &sample);
    void activatePending(const TrackingSample &sample);
    /// 关闭活跃会话并落库;返回 false 表示写库失败,状态保持不变以便重试。
    bool finishActive(const QDateTime &endTime, qint64 endMonotonicMs);
    /// 跨午夜切分。atEnd=true 表示这是收尾调用:当切分点与结束时刻重合时,
    /// 不再插入 0 秒尾段(置 m_sessionId=-1 让 finishActive 守卫跳过补写)。
    void splitAtMidnights(const QDateTime &endTime, qint64 endMonotonicMs, bool atEnd);
    bool insertActive(int durationSeconds);
    void resetSession(State state = State::Stopped);
    int elapsedSeconds(qint64 endMonotonicMs) const;

    TrackingStore *m_store;
    State m_state = State::Stopped;
    qint64 m_sessionId = -1;
    QString m_processName;
    QString m_processKey;
    QString m_windowTitle;
    QString m_appName;
    QDateTime m_startTime;
    qint64 m_startMonotonicMs = 0;
    // 上次调用 updateSessionDuration 持久化时的单调时钟值,用于周期 flush
    qint64 m_lastPersistMonotonicMs = 0;
};

#endif // TRACKING_ENGINE_H
