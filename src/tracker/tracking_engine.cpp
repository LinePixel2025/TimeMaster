#include "tracking_engine.h"

#include <QTimeZone>
#include <QtGlobal>

// 双时钟约定:时长以单调时钟(QElapsedTimer)为权威,墙钟只用于
// 归属日期。若墙钟相对"开始锚点 + 单调增量"的偏差超过该容忍值(如 NTP 校正
// 或用户改系统时间),则认为墙钟被跳变,改用单调推算的时间,避免时长被摊到
// 错误日期或产生荒谬的跨天切分。
static const qint64 kWallClockDriftToleranceMs = 5000;
// 单次跨天切分的最大段数上限,防极端日期差(如墙钟前跳数天/数年)导致
// splitAtMidnights 无界循环或产生巨量 session;达到上限后剩余时长由收尾
// 的 updateSessionEnd 守恒写入最后一段。
static const int kMaxMidnightSplits = 366;

namespace {
// 锁屏/UAC 等无人交互桌面下停留在前台的系统进程:虽有有效窗口与 pid,
// 但不存在真实使用行为。截断当前会话并进入 Idle,不新建追踪
// (空闲阈值只是间接兜底,阈值调大时长假时长就会累积)。
const QSet<QString> kUnattendedProcessKeys = {
    QStringLiteral("logonui.exe"),
    QStringLiteral("winlogon.exe"),
};

// 双时钟跳变防护:时长以单调时钟为权威,墙钟只负责归属日期。若传入的墙钟
// 相对"开始锚点 + 单调增量"偏差超过容忍值(系统时间被 NTP 校正或手动调整),
// 采用单调推算的墙钟,避免时长被摊到错误日期或产生荒谬的跨天切分。
QDateTime sanitizeWallTime(const QDateTime &wallTime, const QDateTime &startTime,
                           qint64 startMonotonicMs, qint64 endMonotonicMs)
{
    const QDateTime safeEndTime = wallTime < startTime ? startTime : wallTime;
    const qint64 safeEndMonotonicMs = qMax(endMonotonicMs, startMonotonicMs);
    const QDateTime endByMonotonic =
        startTime.addMSecs(safeEndMonotonicMs - startMonotonicMs);
    return qAbs(endByMonotonic.msecsTo(safeEndTime)) > kWallClockDriftToleranceMs
               ? endByMonotonic
               : safeEndTime;
}
} // namespace

TrackingEngine::TrackingEngine(TrackingStore *store)
    : m_store(store)
{
}

int TrackingEngine::activationThresholdMs(const TrackingConfig &config)
{
    // settle-in 防抖:通知/弹窗/输入法等瞬时前台窗口往往只被采样一两次,
    // 即使 min_tracking_seconds=0 也要求同一前台至少连续两个采样点可见
    // (间隔达到一个轮询周期)才落库;start 锚点仍取首个样本时刻,
    // 激活后的会话时长不会缩水。
    return qMax(config.minTrackingMs, config.pollIntervalMs);
}

void TrackingEngine::process(const TrackingSample &sample, const TrackingConfig &config)
{
    // 以下分支的公共模式:finishActive 返回 false 表示收尾写库失败,
    // 本轮保持会话状态不动、不做 reset/begin,下一轮以新时刻重试。
    // 代价是切换推迟一个轮询周期(宁可下一段起点晚 1s,不丢上一段收尾)。
    if (!config.enabled) {
        if (!finishActive(sample.wallTime, sample.monotonicMs))
            return;
        resetSession();
        return;
    }

    if (sample.idleMs >= config.idleThresholdMs) {
        const qint64 effectiveIdleMs = qMin(sample.idleMs, sample.monotonicMs);
        if (!finishActive(sample.wallTime.addMSecs(-effectiveIdleMs),
                          sample.monotonicMs - effectiveIdleMs))
            return;
        resetSession(State::Idle);
        return;
    }

    if (!sample.foregroundValid)
        return;

    if (config.ignoredProcessKeys.contains(sample.processKey)) {
        if (!finishActive(sample.wallTime, sample.monotonicMs))
            return;
        resetSession();
        return;
    }

    if (kUnattendedProcessKeys.contains(sample.processKey)) {
        if (!finishActive(sample.wallTime, sample.monotonicMs))
            return;
        resetSession(State::Idle);
        return;
    }

    if (m_state == State::Active && sample.processKey == m_processKey) {
        // 活跃期间的跨天切分同样应用双时钟防护(墙钟跳变时按单调推算切分)
        const QDateTime effectiveEndTime = sanitizeWallTime(
            sample.wallTime, m_startTime, m_startMonotonicMs, sample.monotonicMs);
        splitAtMidnights(effectiveEndTime, sample.monotonicMs, false);
        // 周期 flush:同窗口持续活跃时不每次轮询都写库,达到 persistIntervalMs
        // 才持久化一次。窗口切换 / idle / 禁用 / 忽略 / stop() 仍会走
        // finishActive 完整落库,唯一丢失窗口是崩溃,最多丢一个 flush 周期。
        if (m_sessionId >= 0 &&
            sample.monotonicMs - m_lastPersistMonotonicMs >= config.persistIntervalMs) {
            m_store->updateSessionDuration(m_sessionId, elapsedSeconds(sample.monotonicMs));
            m_lastPersistMonotonicMs = sample.monotonicMs;
        }
        return;
    }

    if (m_state == State::Pending && sample.processKey == m_processKey) {
        if (sample.monotonicMs - m_startMonotonicMs >= activationThresholdMs(config))
            activatePending(sample);
        return;
    }

    if (!finishActive(sample.wallTime, sample.monotonicMs))
        return;
    resetSession();
    beginCandidate(sample);
}

bool TrackingEngine::stop(const QDateTime &wallTime, qint64 monotonicMs)
{
    if (!finishActive(wallTime, monotonicMs))
        return false;
    resetSession();
    return true;
}

TrackingEngine::State TrackingEngine::state() const
{
    return m_state;
}

QString TrackingEngine::currentProcessKey() const
{
    return m_processKey;
}

void TrackingEngine::beginCandidate(const TrackingSample &sample)
{
    m_processName = sample.processName;
    m_processKey = sample.processKey;
    m_windowTitle = sample.windowTitle;
    m_appName = sample.appName;
    m_startTime = sample.wallTime;
    m_startMonotonicMs = sample.monotonicMs;
    // 一律先进入 Pending:是否落库由 activationThresholdMs 的 settle-in 判定,
    // 避免默认配置下每个一闪而过的窗口都留下 0~1 秒碎行
    m_state = State::Pending;
}

void TrackingEngine::activatePending(const TrackingSample &sample)
{
    const int duration = elapsedSeconds(sample.monotonicMs);
    if (insertActive(duration)) {
        m_state = State::Active;
        // Pending 期间墙钟同样可能跳变,切分前必须先做双时钟防护
        const QDateTime effectiveEndTime = sanitizeWallTime(
            sample.wallTime, m_startTime, m_startMonotonicMs, sample.monotonicMs);
        splitAtMidnights(effectiveEndTime, sample.monotonicMs, false);
    }
}

bool TrackingEngine::finishActive(const QDateTime &endTime, qint64 endMonotonicMs)
{
    if (m_state != State::Active || m_sessionId < 0)
        return true;

    const qint64 safeEndMonotonicMs = qMax(endMonotonicMs, m_startMonotonicMs);
    const QDateTime finalEndTime = sanitizeWallTime(
        endTime, m_startTime, m_startMonotonicMs, endMonotonicMs);

    splitAtMidnights(finalEndTime, safeEndMonotonicMs, true);
    if (m_sessionId >= 0)
        return m_store->updateSessionEnd(m_sessionId, finalEndTime,
                                         elapsedSeconds(safeEndMonotonicMs));
    return true;
}

void TrackingEngine::splitAtMidnights(const QDateTime &endTime, qint64 endMonotonicMs,
                                      bool atEnd)
{
    int splits = 0;
    while (m_sessionId >= 0 && m_startTime.date() < endTime.date() &&
           splits < kMaxMidnightSplits) {
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
        // 新段从自己的起始时刻重新累计 flush 间隔,避免刚跨天就立即触发一次写库
        m_lastPersistMonotonicMs = boundaryMonotonic;
        ++splits;
        if (atEnd && endMonotonicMs - boundaryMonotonic < 1000) {
            // 收尾时刻落在午夜切分点附近、剩余时长不足 1 秒(整秒截断为 0):
            // 不再开 0 秒尾段。m_sessionId=-1 让 finishActive 的守卫跳过
            // 对已闭段的重复补写。
            m_sessionId = -1;
            return;
        }
        m_sessionId = m_store->insertSession(
            m_processName, m_windowTitle, m_appName, m_startTime,
            QDateTime(), 0);
        if (m_sessionId < 0)
            m_state = State::Pending;
    }
}

bool TrackingEngine::insertActive(int durationSeconds)
{
    m_sessionId = m_store->insertSession(
        m_processName, m_windowTitle, m_appName, m_startTime,
        QDateTime(), durationSeconds);
    if (m_sessionId < 0)
        return false;
    // 新会话从自己的起始时刻重新累计 flush 间隔
    m_lastPersistMonotonicMs = m_startMonotonicMs;
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
