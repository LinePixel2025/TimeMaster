#include "tracking_engine.h"

#include <QTimeZone>
#include <QtGlobal>

TrackingEngine::TrackingEngine(TrackingStore *store)
    : m_store(store)
{
}

void TrackingEngine::process(const TrackingSample &sample, const TrackingConfig &config)
{
    if (!config.enabled) {
        finishActive(sample.wallTime, sample.monotonicMs);
        resetSession();
        return;
    }

    if (sample.idleMs >= config.idleThresholdMs) {
        const qint64 effectiveIdleMs = qMin(sample.idleMs, sample.monotonicMs);
        finishActive(sample.wallTime.addMSecs(-effectiveIdleMs),
                     sample.monotonicMs - effectiveIdleMs);
        resetSession(State::Idle);
        return;
    }

    if (!sample.foregroundValid)
        return;

    if (config.ignoredProcessKeys.contains(sample.processKey)) {
        finishActive(sample.wallTime, sample.monotonicMs);
        resetSession();
        return;
    }

    if (m_state == State::Active && sample.processKey == m_processKey) {
        splitAtMidnights(sample.wallTime, sample.monotonicMs);
        if (m_sessionId >= 0)
            m_store->updateSessionDuration(m_sessionId, elapsedSeconds(sample.monotonicMs));
        return;
    }

    if (m_state == State::Pending && sample.processKey == m_processKey) {
        if (sample.monotonicMs - m_startMonotonicMs >= config.minTrackingMs)
            activatePending(sample);
        return;
    }

    finishActive(sample.wallTime, sample.monotonicMs);
    resetSession();
    beginCandidate(sample, config);
}

void TrackingEngine::stop(const QDateTime &wallTime, qint64 monotonicMs)
{
    finishActive(wallTime, monotonicMs);
    resetSession();
}

TrackingEngine::State TrackingEngine::state() const
{
    return m_state;
}

QString TrackingEngine::currentProcessKey() const
{
    return m_processKey;
}

void TrackingEngine::beginCandidate(const TrackingSample &sample, const TrackingConfig &config)
{
    m_processName = sample.processName;
    m_processKey = sample.processKey;
    m_windowTitle = sample.windowTitle;
    m_appName = sample.appName;
    m_startTime = sample.wallTime;
    m_startMonotonicMs = sample.monotonicMs;

    if (config.minTrackingMs <= 0) {
        if (insertActive(m_startTime, m_startMonotonicMs, 0))
            m_state = State::Active;
        else
            m_state = State::Pending;
    } else {
        m_state = State::Pending;
    }
}

void TrackingEngine::activatePending(const TrackingSample &sample)
{
    const int duration = qMax(0, static_cast<int>(
        (sample.monotonicMs - m_startMonotonicMs) / 1000));
    if (insertActive(m_startTime, m_startMonotonicMs, duration)) {
        m_state = State::Active;
        splitAtMidnights(sample.wallTime, sample.monotonicMs);
    }
}

void TrackingEngine::finishActive(const QDateTime &endTime, qint64 endMonotonicMs)
{
    if (m_state != State::Active || m_sessionId < 0)
        return;

    const QDateTime safeEndTime = endTime < m_startTime ? m_startTime : endTime;
    const qint64 safeEndMonotonicMs = qMax(endMonotonicMs, m_startMonotonicMs);
    splitAtMidnights(safeEndTime, safeEndMonotonicMs);
    if (m_sessionId >= 0)
        m_store->updateSessionEnd(m_sessionId, safeEndTime,
                                  elapsedSeconds(safeEndMonotonicMs));
}

void TrackingEngine::splitAtMidnights(const QDateTime &endTime, qint64 endMonotonicMs)
{
    while (m_sessionId >= 0 && m_startTime.date() < endTime.date()) {
        const QDateTime midnight(m_startTime.date().addDays(1), QTime(0, 0),
                                 m_startTime.timeZone());
        const qint64 wallMs = qMax<qint64>(0, m_startTime.msecsTo(midnight));
        const qint64 boundaryMonotonic = qMin(endMonotonicMs,
            m_startMonotonicMs + wallMs);
        const int duration = qMax(0, static_cast<int>(
            (boundaryMonotonic - m_startMonotonicMs) / 1000));
        if (!m_store->updateSessionEnd(m_sessionId, midnight, duration))
            return;

        m_startTime = midnight;
        m_startMonotonicMs = boundaryMonotonic;
        m_sessionId = m_store->insertSession(
            m_processName, m_windowTitle, m_appName, m_startTime,
            QDateTime(), 0);
        if (m_sessionId < 0)
            m_state = State::Pending;
    }
}

bool TrackingEngine::insertActive(const QDateTime &startTime,
                                  qint64 startMonotonicMs,
                                  int durationSeconds)
{
    m_sessionId = m_store->insertSession(
        m_processName, m_windowTitle, m_appName, startTime,
        QDateTime(), durationSeconds);
    if (m_sessionId < 0)
        return false;
    m_startTime = startTime;
    m_startMonotonicMs = startMonotonicMs;
    return true;
}

void TrackingEngine::resetSession(State state)
{
    m_state = state;
    m_sessionId = -1;
    m_processName.clear();
    m_processKey.clear();
    m_windowTitle.clear();
    m_appName.clear();
    m_startTime = QDateTime();
    m_startMonotonicMs = 0;
}

int TrackingEngine::elapsedSeconds(qint64 endMonotonicMs) const
{
    return qMax(0, static_cast<int>(
        (endMonotonicMs - m_startMonotonicMs) / 1000));
}
