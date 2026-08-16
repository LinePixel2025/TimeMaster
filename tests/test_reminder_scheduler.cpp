#include <cassert>
#include <iostream>
#include <QCoreApplication>
#include <QDateTime>
#include <QSignalSpy>
#include <QTemporaryFile>

#include "ai/ai_client.h"
#include "database/database_manager.h"
#include "reminder/reminder_scheduler.h"

// 插入一条今天的会话，使今日总时长非空。
static void seedTodaySession(DatabaseManager &db, int seconds = 3600)
{
    const QDateTime now = QDateTime::currentDateTime();
    db.insertSession(QStringLiteral("notepad.exe"), QStringLiteral("test"),
                     QStringLiteral("记事本"),
                     now, now.addSecs(seconds), seconds);
}

void test_disabled_no_reminder()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    seedTodaySession(db);
    db.setSetting("reminder_enabled", "false");
    db.setSetting("reminder_times", "23:59");

    AiClient ai(&db);
    ReminderScheduler scheduler(&db, &ai);
    scheduler.reloadSettings();
    QSignalSpy dueSpy(&scheduler, &ReminderScheduler::reminderDue);

    scheduler.checkNow(QDateTime(QDate::currentDate(), QTime(23, 59)));
    assert(dueSpy.count() == 0);
    std::cout << "test_disabled_no_reminder PASS" << std::endl;
}

void test_no_match_no_reminder()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    seedTodaySession(db);
    db.setSetting("reminder_enabled", "true");
    db.setSetting("reminder_times", "23:59");

    AiClient ai(&db);
    ReminderScheduler scheduler(&db, &ai);
    scheduler.reloadSettings();
    QSignalSpy dueSpy(&scheduler, &ReminderScheduler::reminderDue);

    scheduler.checkNow(QDateTime(QDate::currentDate(), QTime(8, 0))); // 不在配置列表
    assert(dueSpy.count() == 0);
    std::cout << "test_no_match_no_reminder PASS" << std::endl;
}

void test_no_data_explains_not_fired()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path); // 空数据库：今日无数据
    db.setSetting("reminder_enabled", "true");
    db.setSetting("reminder_times", "23:59");

    AiClient ai(&db);
    ReminderScheduler scheduler(&db, &ai);
    scheduler.reloadSettings();
    QSignalSpy dueSpy(&scheduler, &ReminderScheduler::reminderDue);

    // 今日无数据：不发真实提醒，但弹一条说明，避免"配置了却不响"的困惑。
    scheduler.checkNow(QDateTime(QDate::currentDate(), QTime(23, 59)));
    assert(dueSpy.count() == 1);
    const QString message = dueSpy.first().at(1).toString();
    assert(message.contains(QStringLiteral("今日暂无使用记录")));
    assert(db.getSetting("reminder_last_fired", "") != "");
    std::cout << "test_no_data_explains_not_fired PASS" << std::endl;
}

void test_hit_local_template_once()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    seedTodaySession(db);
    db.setSetting("reminder_enabled", "true");
    db.setSetting("reminder_times", "23:59");

    AiClient ai(&db); // 未启用 AI → 走本地模板
    ReminderScheduler scheduler(&db, &ai);
    scheduler.reloadSettings();
    QSignalSpy dueSpy(&scheduler, &ReminderScheduler::reminderDue);

    scheduler.checkNow(QDateTime(QDate::currentDate(), QTime(23, 59)));
    scheduler.checkNow(QDateTime(QDate::currentDate(), QTime(23, 59))); // 同分钟再次触发不应重复
    scheduler.checkNow(QDateTime(QDate::currentDate(), QTime(23, 59)));
    assert(dueSpy.count() == 1);

    const QList<QVariant> args = dueSpy.takeFirst();
    const QString message = args.at(1).toString();
    assert(message.contains(QStringLiteral("今日已使用")));
    std::cout << "test_hit_local_template_once PASS" << std::endl;
}

void test_ai_failure_falls_back_to_local()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    seedTodaySession(db);
    db.setSetting("reminder_enabled", "true");
    db.setSetting("reminder_times", "23:59");
    db.setSetting("ai_enabled", "true");
    db.setSetting("ai_api_key", "sk-test");
    db.setSetting("ai_api_endpoint", "http://127.0.0.1:1"); // 无效端点

    AiClient ai(&db);
    ai.reloadSettings();
    ReminderScheduler scheduler(&db, &ai);
    scheduler.reloadSettings();
    QSignalSpy dueSpy(&scheduler, &ReminderScheduler::reminderDue);

    scheduler.checkNow(QDateTime(QDate::currentDate(), QTime(23, 59)));
    // 请求失败后应回退本地模板并发出提醒。
    assert(dueSpy.wait(10000));
    assert(dueSpy.count() >= 1);
    const QString message = dueSpy.first().at(1).toString();
    assert(message.contains(QStringLiteral("今日已使用")));
    std::cout << "test_ai_failure_falls_back_to_local PASS" << std::endl;
}

// 防回归：start() 必须启动 30 秒轮询定时器（曾因漏掉 m_timer->start()
// 导致只在启动瞬间检查一次，到点从不触发）。
void test_start_activates_timer()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    AiClient ai(&db);
    ReminderScheduler scheduler(&db, &ai);
    assert(!scheduler.isRunning());

    scheduler.start(); // 未启用配置下 checkNow 直接返回，无副作用。
    assert(scheduler.isRunning());

    scheduler.stop();
    assert(!scheduler.isRunning());
    std::cout << "test_start_activates_timer PASS" << std::endl;
}

// 间隔提醒：首次检查仅锚定起点，未到间隔不触发，到达间隔触发一次，
// 同一时刻的重复检查不再触发（锚点已先行更新）。
void test_interval_fires_after_elapsed()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    seedTodaySession(db);
    db.setSetting("reminder_interval_enabled", "true");
    db.setSetting("reminder_interval_minutes", "45");

    AiClient ai(&db); // 未启用 AI → 走本地模板
    ReminderScheduler scheduler(&db, &ai);
    scheduler.reloadSettings();
    QSignalSpy dueSpy(&scheduler, &ReminderScheduler::reminderDue);

    const QDateTime base = QDateTime::currentDateTime();
    scheduler.checkNow(base);                   // 锚定起点，不触发
    scheduler.checkNow(base.addSecs(44 * 60));  // 未到 45 分钟
    assert(dueSpy.count() == 0);
    scheduler.checkNow(base.addSecs(45 * 60));  // 到达间隔
    assert(dueSpy.count() == 1);
    scheduler.checkNow(base.addSecs(45 * 60));  // 锚点已更新，同刻不重复
    assert(dueSpy.count() == 1);

    const QString message = dueSpy.first().at(1).toString();
    assert(message.contains(QStringLiteral("今日已使用")));
    assert(db.getSetting("reminder_last_fired", "")
               .contains(QStringLiteral("间隔到点")));
    std::cout << "test_interval_fires_after_elapsed PASS" << std::endl;
}

// 间隔提醒禁用时，即使远超间隔也不触发。
void test_interval_disabled_no_reminder()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    seedTodaySession(db);
    db.setSetting("reminder_interval_enabled", "false");
    db.setSetting("reminder_interval_minutes", "45");

    AiClient ai(&db);
    ReminderScheduler scheduler(&db, &ai);
    scheduler.reloadSettings();
    QSignalSpy dueSpy(&scheduler, &ReminderScheduler::reminderDue);

    const QDateTime base = QDateTime::currentDateTime();
    scheduler.checkNow(base);
    scheduler.checkNow(base.addSecs(10 * 60 * 60)); // 远超间隔
    assert(dueSpy.count() == 0);
    std::cout << "test_interval_disabled_no_reminder PASS" << std::endl;
}

// 间隔到点但今日无数据：不发真实提醒，弹说明并记录触发痕迹（与时间点提醒一致）。
void test_interval_no_data_explains_not_fired()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path); // 空数据库：今日无数据
    db.setSetting("reminder_interval_enabled", "true");
    db.setSetting("reminder_interval_minutes", "5");

    AiClient ai(&db);
    ReminderScheduler scheduler(&db, &ai);
    scheduler.reloadSettings();
    QSignalSpy dueSpy(&scheduler, &ReminderScheduler::reminderDue);

    const QDateTime base = QDateTime::currentDateTime();
    scheduler.checkNow(base);                // 锚定起点
    scheduler.checkNow(base.addSecs(5 * 60)); // 到达间隔
    assert(dueSpy.count() == 1);
    const QString message = dueSpy.first().at(1).toString();
    assert(message.contains(QStringLiteral("今日暂无使用记录")));
    assert(db.getSetting("reminder_last_fired", "") != "");
    std::cout << "test_interval_no_data_explains_not_fired PASS" << std::endl;
}

// 间隔配置变化时 reloadSettings 重新计时；间隔配置未变时的 reload 不应误重置计时。
void test_interval_reload_resets_anchor()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    seedTodaySession(db);
    db.setSetting("reminder_interval_enabled", "true");
    db.setSetting("reminder_interval_minutes", "45");

    AiClient ai(&db);
    ReminderScheduler scheduler(&db, &ai);
    scheduler.reloadSettings();
    QSignalSpy dueSpy(&scheduler, &ReminderScheduler::reminderDue);

    const QDateTime base = QDateTime::currentDateTime();
    scheduler.checkNow(base); // 锚定起点

    db.setSetting("reminder_interval_minutes", "30");
    scheduler.reloadSettings(); // 配置变化 → 清空锚点重新计时
    scheduler.checkNow(base.addSecs(35 * 60)); // 相对旧锚点已超 30 分钟，但重新计时后仅锚定
    assert(dueSpy.count() == 0);
    scheduler.checkNow(base.addSecs(65 * 60)); // 新锚点 +30 分钟 → 触发
    assert(dueSpy.count() == 1);

    scheduler.reloadSettings(); // 间隔配置未变 → 不重置计时
    scheduler.checkNow(base.addSecs(80 * 60)); // 距上次触发仅 15 分钟
    assert(dueSpy.count() == 1);
    std::cout << "test_interval_reload_resets_anchor PASS" << std::endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    test_start_activates_timer();
    test_disabled_no_reminder();
    test_no_match_no_reminder();
    test_no_data_explains_not_fired();
    test_hit_local_template_once();
    test_ai_failure_falls_back_to_local();
    test_interval_fires_after_elapsed();
    test_interval_disabled_no_reminder();
    test_interval_no_data_explains_not_fired();
    test_interval_reload_resets_anchor();
    std::cout << "All reminder scheduler tests passed!" << std::endl;
    return 0;
}
