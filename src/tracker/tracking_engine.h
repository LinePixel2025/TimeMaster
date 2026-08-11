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
    int minTrackingMs = 0;
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
    void stop(const QDateTime &wallTime, qint64 monotonicMs);
    State state() const;
    QString currentProcessKey() const;

private:
    void beginCandidate(const TrackingSample &sample, const TrackingConfig &config);
    void activatePending(const TrackingSample &sample);
    void finishActive(const QDateTime &endTime, qint64 endMonotonicMs);
    void splitAtMidnights(const QDateTime &endTime, qint64 endMonotonicMs);
    bool insertActive(const QDateTime &startTime, qint64 startMonotonicMs, int durationSeconds);
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
};

#endif // TRACKING_ENGINE_H
