#include <cassert>
#include <iostream>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "ai/ai_client.h"
#include "database/database_manager.h"
#include "reminder/weekly_report_manager.h"

// 插入一条上周的会话，使上周数据非空。seconds 默认 3600（1 小时）。
static void seedLastWeekSession(DatabaseManager &db, int seconds = 3600)
{
    const QDate today = QDate::currentDate();
    const QDate lastMonday = today.addDays(-((today.dayOfWeek() - 1) % 7) - 7);
    const QDateTime start(lastMonday, QTime(10, 0));
    db.insertSession(QStringLiteral("notepad.exe"), QStringLiteral("test"),
                     QStringLiteral("记事本"),
                     start, start.addSecs(seconds), seconds);
}

// 把生成日设为「今天」，使 checkNow(QTime(9,0)) 能命中周几条件。
static void setReportDayToToday(DatabaseManager &db)
{
    db.setSetting("weekly_report_day",
                  QString::number(QDate::currentDate().dayOfWeek()));
}

void test_disabled_no_report()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    seedLastWeekSession(db);
    db.setSetting("weekly_report_enabled", "false");

    AiClient ai(&db);
    WeeklyReportManager weekly(&db, &ai);
    weekly.reloadSettings();
    QSignalSpy readySpy(&weekly, &WeeklyReportManager::weeklyReportReady);

    weekly.checkNow(QTime(9, 0)); // 命中时间也无生成
    assert(readySpy.count() == 0);
    std::cout << "test_disabled_no_report PASS" << std::endl;
}

void test_wrong_day_or_time_no_report()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    seedLastWeekSession(db);
    db.setSetting("weekly_report_enabled", "true");
    db.setSetting("weekly_report_day", "1"); // 周一
    db.setSetting("weekly_report_time", "09:00");

    AiClient ai(&db);
    WeeklyReportManager weekly(&db, &ai);
    weekly.reloadSettings();
    QSignalSpy readySpy(&weekly, &WeeklyReportManager::weeklyReportReady);

    // 今天的实际星期几必然与 dayOfWeek 相同，所以只测时间不匹配。
    weekly.checkNow(QTime(8, 0));
    assert(readySpy.count() == 0);
    std::cout << "test_wrong_time_no_report PASS" << std::endl;
}

void test_no_data_no_report()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path); // 空数据库：上周无数据
    db.setSetting("weekly_report_enabled", "true");

    AiClient ai(&db);
    WeeklyReportManager weekly(&db, &ai);
    weekly.reloadSettings();
    QSignalSpy readySpy(&weekly, &WeeklyReportManager::weeklyReportReady);

    weekly.checkNow(QTime(9, 0));
    assert(readySpy.count() == 0);
    std::cout << "test_no_data_no_report PASS" << std::endl;
}

void test_generates_html_once()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    seedLastWeekSession(db);
    db.setSetting("weekly_report_enabled", "true");
    setReportDayToToday(db);

    QTemporaryDir outDir;
    assert(outDir.isValid());

    AiClient ai(&db); // 未启用 AI → 本地小结
    WeeklyReportManager weekly(&db, &ai);
    weekly.setOutputDir(outDir.path());
    weekly.reloadSettings();
    QSignalSpy readySpy(&weekly, &WeeklyReportManager::weeklyReportReady);

    weekly.checkNow(QTime(9, 0));
    assert(readySpy.count() == 1);

    const QString htmlPath = readySpy.first().at(0).toString();
    assert(QFile::exists(htmlPath));
    QFile file(htmlPath);
    assert(file.open(QIODevice::ReadOnly));
    const QByteArray content = file.readAll();
    assert(content.contains("Time Master"));
    assert(content.contains("AI")); // 「AI 分析」区块
    assert(content.contains("上周总时长"));
    file.close();

    // 同周再次 checkNow 不应重复生成（last_generated 去重）。
    readySpy.clear();
    weekly.checkNow(QTime(9, 0));
    assert(readySpy.count() == 0);
    assert(db.getSetting("weekly_report_last_generated", "") != "");
    std::cout << "test_generates_html_once PASS" << std::endl;
}

void test_ai_failure_falls_back_to_local()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    seedLastWeekSession(db);
    db.setSetting("weekly_report_enabled", "true");
    db.setSetting("ai_enabled", "true");
    db.setSetting("ai_api_key", "sk-test");
    db.setSetting("ai_api_endpoint", "http://127.0.0.1:1"); // 无效端点
    setReportDayToToday(db);

    QTemporaryDir outDir;
    assert(outDir.isValid());

    AiClient ai(&db);
    ai.reloadSettings();
    WeeklyReportManager weekly(&db, &ai);
    weekly.setOutputDir(outDir.path());
    weekly.reloadSettings();
    QSignalSpy readySpy(&weekly, &WeeklyReportManager::weeklyReportReady);

    weekly.checkNow(QTime(9, 0));
    // AI 请求失败后应回退本地小结并照常生成。
    assert(readySpy.wait(10000));
    assert(readySpy.count() >= 1);
    const QString htmlPath = readySpy.first().at(0).toString();
    assert(QFile::exists(htmlPath));
    QFile file(htmlPath);
    assert(file.open(QIODevice::ReadOnly));
    assert(file.readAll().contains("AI"));
    file.close();
    std::cout << "test_ai_failure_falls_back_to_local PASS" << std::endl;
}

// 防回归：start() 必须启动 30 秒轮询定时器（曾与提醒调度器同因漏掉
// m_timer->start() 而只在启动瞬间检查一次）。
void test_start_activates_timer()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    AiClient ai(&db);
    WeeklyReportManager weekly(&db, &ai);
    assert(!weekly.isRunning());

    weekly.start(); // 未启用配置下 checkNow 直接返回，无副作用。
    assert(weekly.isRunning());

    weekly.stop();
    assert(!weekly.isRunning());
    std::cout << "test_start_activates_timer PASS" << std::endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    test_start_activates_timer();
    test_disabled_no_report();
    test_wrong_day_or_time_no_report();
    test_no_data_no_report();
    test_generates_html_once();
    test_ai_failure_falls_back_to_local();
    std::cout << "All weekly report tests passed!" << std::endl;
    return 0;
}
