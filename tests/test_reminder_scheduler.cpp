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

    scheduler.checkNow(QTime(23, 59));
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

    scheduler.checkNow(QTime(8, 0)); // 不在配置列表
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
    scheduler.checkNow(QTime(23, 59));
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

    scheduler.checkNow(QTime(23, 59));
    scheduler.checkNow(QTime(23, 59)); // 同分钟再次触发不应重复
    scheduler.checkNow(QTime(23, 59));
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

    scheduler.checkNow(QTime(23, 59));
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

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    test_start_activates_timer();
    test_disabled_no_reminder();
    test_no_match_no_reminder();
    test_no_data_explains_not_fired();
    test_hit_local_template_once();
    test_ai_failure_falls_back_to_local();
    std::cout << "All reminder scheduler tests passed!" << std::endl;
    return 0;
}
