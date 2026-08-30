#include <cassert>
#include <iostream>
#include <QCoreApplication>
#include <QVector>

#include "tracker/tracking_engine.h"

struct StoredSession {
    qint64 id;
    QString processName;
    QString title;
    QString appName;
    QDateTime start;
    QDateTime end;
    int duration = 0;
};

class FakeStore : public TrackingStore
{
public:
    qint64 insertSession(const QString &processName, const QString &windowTitle,
                         const QString &appName, const QDateTime &startTime,
                         const QDateTime &endTime, int durationSeconds) override
    {
        if (failNextInsert) {
            failNextInsert = false;
            return -1;
        }
        const qint64 id = nextId++;
        sessions.append({id, processName, windowTitle, appName,
                         startTime, endTime, durationSeconds});
        return id;
    }

    bool updateSessionEnd(qint64 sessionId, const QDateTime &endTime,
                          int durationSeconds) override
    {
        if (failNextUpdateEnd) {
            failNextUpdateEnd = false;
            return false;
        }
        StoredSession *session = find(sessionId);
        if (!session)
            return false;
        session->end = endTime;
        session->duration = durationSeconds;
        return true;
    }

    bool updateSessionDuration(qint64 sessionId, int durationSeconds) override
    {
        ++durationUpdates;
        StoredSession *session = find(sessionId);
        if (!session)
            return false;
        session->duration = durationSeconds;
        return true;
    }

    StoredSession *find(qint64 id)
    {
        for (StoredSession &session : sessions) {
            if (session.id == id)
                return &session;
        }
        return nullptr;
    }

    QVector<StoredSession> sessions;
    bool failNextInsert = false;
    // 注入一次 updateSessionEnd 失败,验证收尾写库失败后的推迟-重试语义
    bool failNextUpdateEnd = false;
    // updateSessionDuration 的调用次数,用于验证周期 flush 的写库频率
    int durationUpdates = 0;
    qint64 nextId = 1;
};

static TrackingSample sample(const QDateTime &time, qint64 monotonicMs,
                             const QString &key = "code.exe",
                             const QString &title = "File A",
                             qint64 idleMs = 0)
{
    TrackingSample value;
    value.wallTime = time;
    value.monotonicMs = monotonicMs;
    value.idleMs = idleMs;
    value.foregroundValid = !key.isEmpty();
    value.processName = "C:\\Apps\\" + key;
    value.processKey = key;
    value.windowTitle = title;
    value.appName = key == "code.exe" ? "VS Code" : "Chrome";
    return value;
}

void test_title_changes_do_not_split()
{
    FakeStore store;
    TrackingEngine engine(&store);
    TrackingConfig config;
    const QDateTime start(QDate(2026, 8, 11), QTime(10, 0));

    engine.process(sample(start, 0), config);
    engine.process(sample(start.addSecs(10), 10000, "code.exe", "File B"), config);
    engine.stop(start.addSecs(20), 20000);

    assert(store.sessions.size() == 1);
    assert(store.sessions[0].title == "File A");
    assert(store.sessions[0].duration == 20);
    std::cout << "test_title_changes_do_not_split PASS\n";
}

void test_pending_threshold_and_switch()
{
    FakeStore store;
    TrackingEngine engine(&store);
    TrackingConfig config;
    config.minTrackingMs = 5000;
    const QDateTime start(QDate(2026, 8, 11), QTime(10, 0));

    engine.process(sample(start, 0), config);
    engine.process(sample(start.addSecs(2), 2000, "chrome.exe"), config);
    assert(store.sessions.isEmpty());
    engine.process(sample(start.addSecs(8), 8000, "chrome.exe"), config);
    assert(store.sessions.size() == 1);
    assert(store.sessions[0].start == start.addSecs(2));
    assert(store.sessions[0].duration == 6);
    std::cout << "test_pending_threshold_and_switch PASS\n";
}

void test_disable_and_ignore_close_sessions()
{
    FakeStore store;
    TrackingEngine engine(&store);
    TrackingConfig config;
    const QDateTime start(QDate(2026, 8, 11), QTime(10, 0));

    // 需连续两个采样点越过 settle-in 门槛后才会落库,与 1Hz 轮询的真实节奏一致
    engine.process(sample(start, 0), config);
    engine.process(sample(start.addSecs(2), 2000), config);
    config.enabled = false;
    engine.process(sample(start.addSecs(10), 10000), config);
    assert(store.sessions[0].duration == 10);

    config.enabled = true;
    engine.process(sample(start.addSecs(20), 20000), config);
    engine.process(sample(start.addSecs(22), 22000), config);
    config.ignoredProcessKeys.insert("chrome.exe");
    engine.process(sample(start.addSecs(25), 25000, "chrome.exe"), config);
    assert(store.sessions.size() == 2);
    assert(store.sessions[1].duration == 5);
    std::cout << "test_disable_and_ignore_close_sessions PASS\n";
}

void test_idle_excludes_time_and_resumes_same_app()
{
    FakeStore store;
    TrackingEngine engine(&store);
    TrackingConfig config;
    config.idleThresholdMs = 60000;
    const QDateTime start(QDate(2026, 8, 11), QTime(10, 0));

    engine.process(sample(start, 0), config);
    engine.process(sample(start.addSecs(2), 2000), config);
    engine.process(sample(start.addSecs(120), 120000, "code.exe", "File", 60000), config);
    assert(store.sessions.size() == 1);
    assert(store.sessions[0].duration == 60);
    assert(store.sessions[0].end == start.addSecs(60));

    engine.process(sample(start.addSecs(121), 121000), config);
    engine.process(sample(start.addSecs(123), 123000), config);
    engine.stop(start.addSecs(131), 131000);
    assert(store.sessions.size() == 2);
    assert(store.sessions[1].start == start.addSecs(121));
    assert(store.sessions[1].duration == 10);
    std::cout << "test_idle_excludes_time_and_resumes_same_app PASS\n";
}

void test_invalid_foreground_keeps_session()
{
    FakeStore store;
    TrackingEngine engine(&store);
    TrackingConfig config;
    const QDateTime start(QDate(2026, 8, 11), QTime(10, 0));

    engine.process(sample(start, 0), config);
    engine.process(sample(start.addSecs(2), 2000), config);
    engine.process(sample(start.addSecs(5), 5000, ""), config);
    engine.stop(start.addSecs(10), 10000);
    assert(store.sessions.size() == 1);
    assert(store.sessions[0].duration == 10);
    std::cout << "test_invalid_foreground_keeps_session PASS\n";
}

void test_midnight_split_and_wall_clock_rollback()
{
    FakeStore store;
    TrackingEngine engine(&store);
    TrackingConfig config;
    const QDateTime start(QDate(2026, 8, 11), QTime(23, 59, 50));

    engine.process(sample(start, 0), config);
    engine.process(sample(start.addSecs(20), 20000), config);
    engine.stop(start.addSecs(30), 30000);
    assert(store.sessions.size() == 2);
    assert(store.sessions[0].duration == 10);
    assert(store.sessions[0].end == QDateTime(QDate(2026, 8, 12), QTime(0, 0)));
    assert(store.sessions[1].duration == 20);

    FakeStore rollbackStore;
    TrackingEngine rollbackEngine(&rollbackStore);
    const QDateTime rollbackStart(QDate(2026, 8, 11), QTime(10, 0));
    rollbackEngine.process(sample(rollbackStart, 0), config);
    rollbackEngine.process(sample(rollbackStart.addSecs(2), 2000), config);
    rollbackEngine.stop(rollbackStart.addSecs(-30), 10000);
    assert(rollbackStore.sessions[0].duration == 10);
    // 墙钟回拨时双时钟防护生效:end 由"开始锚点 + 单调增量"推算,
    // 与 duration=10 自洽,而不是停留在被回拨的墙钟 safeEndTime 上
    assert(rollbackStore.sessions[0].end == rollbackStart.addSecs(10));
    std::cout << "test_midnight_split_and_wall_clock_rollback PASS\n";
}

void test_insert_failure_retries()
{
    FakeStore store;
    store.failNextInsert = true;
    TrackingEngine engine(&store);
    TrackingConfig config;
    const QDateTime start(QDate(2026, 8, 11), QTime(10, 0));

    engine.process(sample(start, 0), config);
    assert(store.sessions.isEmpty());
    // 越过 settle-in 门槛尝试激活,但 insert 失败,继续保持 Pending
    engine.process(sample(start.addSecs(1), 1000), config);
    assert(store.sessions.isEmpty());
    // 下一个采样点重试 insert:成功且 start 锚点仍取首个候选样本
    engine.process(sample(start.addSecs(2), 2000), config);
    assert(store.sessions.size() == 1);
    assert(store.sessions[0].start == start);
    engine.stop(start.addSecs(3), 3000);
    assert(store.sessions[0].duration == 3);
    std::cout << "test_insert_failure_retries PASS\n";
}

void test_periodic_persist()
{
    FakeStore store;
    TrackingEngine engine(&store);
    TrackingConfig config;
    config.persistIntervalMs = 30000;
    const QDateTime start(QDate(2026, 8, 11), QTime(10, 0));

    // flush 间隔(30 秒)内的持续活跃不触发 updateSessionDuration
    engine.process(sample(start, 0), config);
    engine.process(sample(start.addSecs(10), 10000), config);
    engine.process(sample(start.addSecs(25), 25000), config);
    assert(store.durationUpdates == 0);

    // 达到间隔后持久化一次
    engine.process(sample(start.addSecs(35), 35000), config);
    assert(store.durationUpdates == 1);

    // 收尾落库,最终时长完整
    engine.stop(start.addSecs(45), 45000);
    assert(store.sessions.size() == 1);
    assert(store.sessions[0].duration == 45);
    std::cout << "test_periodic_persist PASS\n";
}

void test_wall_clock_forward_jump_uses_monotonic()
{
    FakeStore store;
    TrackingEngine engine(&store);
    TrackingConfig config;
    config.persistIntervalMs = 1000;
    const QDateTime start(QDate(2026, 8, 11), QTime(23, 30));

    // 墙钟前跳 2 天而单调只过 70 分钟:双时钟防护以单调为权威,
    // 跨午夜切分边界按"开始锚点 + 单调增量"推算(23:30 + 70min = 次日 00:40),
    // 时长不摊到跳变后的日期
    engine.process(sample(start, 0), config);
    engine.process(sample(start.addDays(2).addSecs(2400), 4200000), config);
    assert(store.sessions.size() == 2);
    assert(store.sessions[0].duration == 1800);
    assert(store.sessions[0].end == QDateTime(QDate(2026, 8, 12), QTime(0, 0)));

    engine.stop(start.addDays(2).addSecs(2700), 4500000);
    assert(store.sessions[1].duration == 2700);
    assert(store.sessions[1].end == QDateTime(QDate(2026, 8, 12), QTime(0, 45)));
    std::cout << "test_wall_clock_forward_jump_uses_monotonic PASS\n";
}

void test_extreme_midnight_split_bounded()
{
    FakeStore store;
    TrackingEngine engine(&store);
    TrackingConfig config;
    const QDateTime start(QDate(2026, 8, 11), QTime(10, 0));
    const qint64 extremeMonotonicMs = 400LL * 86400 * 1000; // 单调 400 天,与墙钟一致无跳变

    // 单调跨度 400 天:跨天切分受 kMaxMidnightSplits 上限约束,
    // 不会无界循环或产生巨量 session,收尾落库后所有段时长之和守恒
    engine.process(sample(start, 0), config);
    engine.process(sample(start.addDays(400), extremeMonotonicMs), config);
    engine.stop(start.addDays(400).addSecs(1), extremeMonotonicMs + 1000);

    qint64 total = 0;
    for (const StoredSession &session : store.sessions)
        total += session.duration;
    assert(total == 400LL * 86400 + 1);
    std::cout << "test_extreme_midnight_split_bounded PASS\n";
}

void test_settle_in_discards_fleeting_windows()
{
    FakeStore store;
    TrackingEngine engine(&store);
    TrackingConfig config; // 默认 pollIntervalMs=1000, minTrackingMs=0
    const QDateTime start(QDate(2026, 8, 11), QTime(10, 0));

    // code.exe 只被采样一次(<1 个轮询周期)即被弹窗顶掉:整段丢弃不落库
    engine.process(sample(start, 0), config);
    engine.process(sample(start.addSecs(1), 1000, "chrome.exe", "Notification"), config);
    assert(store.sessions.isEmpty());

    // chrome 连续第二个采样点越过 settle 门槛后才落库,
    // start 锚点仍是它的第一个候选样本(1s),时长不缩水
    engine.process(sample(start.addSecs(2), 2000, "chrome.exe"), config);
    assert(store.sessions.size() == 1);
    assert(store.sessions[0].start == start.addSecs(1));
    assert(store.sessions[0].duration == 1);

    engine.stop(start.addSecs(5), 5000);
    assert(store.sessions[0].duration == 4);
    std::cout << "test_settle_in_discards_fleeting_windows PASS\n";
}

void test_midnight_finish_no_zero_tail()
{
    FakeStore store;
    TrackingEngine engine(&store);
    TrackingConfig config;
    const QDateTime start(QDate(2026, 8, 11), QTime(23, 59, 50));

    // 收尾时刻恰好落在午夜切分点:只闭合并保留第一段,不产生 0 秒尾段
    engine.process(sample(start, 0), config);
    engine.process(sample(start.addSecs(5), 5000), config);
    engine.stop(start.addSecs(10), 10000);
    assert(store.sessions.size() == 1);
    assert(store.sessions[0].duration == 10);
    assert(store.sessions[0].end == QDateTime(QDate(2026, 8, 12), QTime(0, 0)));

    // 收尾越过午夜仍有剩余时长时,尾段正常建立
    FakeStore crossStore;
    TrackingEngine crossEngine(&crossStore);
    crossEngine.process(sample(start, 0), config);
    crossEngine.process(sample(start.addSecs(5), 5000), config);
    crossEngine.stop(start.addSecs(25), 25000);
    assert(crossStore.sessions.size() == 2);
    assert(crossStore.sessions[0].duration == 10);
    assert(crossStore.sessions[1].duration == 15);
    std::cout << "test_midnight_finish_no_zero_tail PASS\n";
}

void test_finish_retry_on_update_end_failure()
{
    FakeStore store;
    TrackingEngine engine(&store);
    TrackingConfig config;
    const QDateTime start(QDate(2026, 8, 11), QTime(10, 0));

    engine.process(sample(start, 0), config);
    engine.process(sample(start.addSecs(1), 1000), config);
    assert(store.sessions.size() == 1);

    // 切应用时收尾写库失败:本轮推迟切换,旧段保持打开状态等待重试
    store.failNextUpdateEnd = true;
    engine.process(sample(start.addSecs(2), 2000, "chrome.exe"), config);
    assert(store.sessions.size() == 1);
    assert(!store.sessions[0].end.isValid());

    // 下一轮重试收尾成功:旧段以新时刻闭合(收尾时长不丢),切换完成
    engine.process(sample(start.addSecs(3), 3000, "chrome.exe"), config);
    assert(store.sessions[0].end == start.addSecs(3));
    assert(store.sessions[0].duration == 3);

    // 新应用照常走 settle-in,anchor 为切换成功后的首个样本
    engine.process(sample(start.addSecs(4), 4000, "chrome.exe"), config);
    engine.stop(start.addSecs(5), 5000);
    assert(store.sessions.size() == 2);
    assert(store.sessions[1].start == start.addSecs(3));
    assert(store.sessions[1].duration == 2);
    std::cout << "test_finish_retry_on_update_end_failure PASS\n";
}

void test_unattended_logon_screen_truncates()
{
    FakeStore store;
    TrackingEngine engine(&store);
    TrackingConfig config;
    const QDateTime start(QDate(2026, 8, 11), QTime(10, 0));

    engine.process(sample(start, 0), config);
    engine.process(sample(start.addSecs(2), 2000), config);

    // 锁屏:LogonUI 截断当前会话并进入 Idle,不为其新建会话
    engine.process(sample(start.addSecs(5), 5000, "logonui.exe"), config);
    assert(store.sessions.size() == 1);
    assert(store.sessions[0].duration == 5);
    assert(store.sessions[0].end == start.addSecs(5));
    assert(engine.state() == TrackingEngine::State::Idle);

    engine.stop(start.addSecs(600), 600000);
    assert(store.sessions.size() == 1);
    std::cout << "test_unattended_logon_screen_truncates PASS\n";
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    test_title_changes_do_not_split();
    test_pending_threshold_and_switch();
    test_disable_and_ignore_close_sessions();
    test_idle_excludes_time_and_resumes_same_app();
    test_invalid_foreground_keeps_session();
    test_midnight_split_and_wall_clock_rollback();
    test_insert_failure_retries();
    test_periodic_persist();
    test_wall_clock_forward_jump_uses_monotonic();
    test_extreme_midnight_split_bounded();
    test_settle_in_discards_fleeting_windows();
    test_midnight_finish_no_zero_tail();
    test_finish_retry_on_update_end_failure();
    test_unattended_logon_screen_truncates();
    std::cout << "All tracking engine tests passed!\n";
    return 0;
}
