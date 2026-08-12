#include <cassert>
#include <iostream>
#include <QCoreApplication>
#include <QDateTime>
#include <QSignalSpy>
#include <QTemporaryFile>

#include "ai/ai_client.h"
#include "database/database_manager.h"

// 插入一条今天的会话，使每日报告数据源非空。
static void seedTodaySession(DatabaseManager &db)
{
    const QDateTime now = QDateTime::currentDateTime();
    db.insertSession(QStringLiteral("notepad.exe"), QStringLiteral("test"),
                     QStringLiteral("记事本"),
                     now, now.addSecs(120), 120);
}

void test_disabled_no_request()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("ai_enabled", "false");
    db.setSetting("ai_api_key", "sk-test");
    db.setSetting("ai_api_endpoint", "http://127.0.0.1:1");
    seedTodaySession(db);

    AiClient ai(&db);
    ai.reloadSettings();
    QSignalSpy readySpy(&ai, &AiClient::reportReady);
    QSignalSpy failSpy(&ai, &AiClient::reportFailed);

    assert(!ai.isConfigured());
    assert(!ai.generateReport("daily"));
    assert(readySpy.count() == 0);
    assert(failSpy.count() == 0);
    std::cout << "test_disabled_no_request PASS" << std::endl;
}

void test_missing_config_no_request()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("ai_enabled", "true");
    db.setSetting("ai_api_key", "");
    db.setSetting("ai_api_endpoint", "");
    seedTodaySession(db);

    AiClient ai(&db);
    ai.reloadSettings();
    QSignalSpy readySpy(&ai, &AiClient::reportReady);
    QSignalSpy failSpy(&ai, &AiClient::reportFailed);

    assert(!ai.isConfigured());
    assert(!ai.generateReport("daily"));
    assert(readySpy.count() == 0);
    assert(failSpy.count() == 0);
    std::cout << "test_missing_config_no_request PASS" << std::endl;
}

void test_no_data_no_request()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path); // 空数据库：无任何会话
    db.setSetting("ai_enabled", "true");
    db.setSetting("ai_api_key", "sk-test");
    db.setSetting("ai_api_endpoint", "http://127.0.0.1:1");

    AiClient ai(&db);
    ai.reloadSettings();
    QSignalSpy readySpy(&ai, &AiClient::reportReady);
    QSignalSpy failSpy(&ai, &AiClient::reportFailed);

    assert(ai.isConfigured());
    assert(!ai.generateReport("daily"));
    assert(!ai.generateReport("weekly"));
    assert(readySpy.count() == 0);
    assert(failSpy.count() == 0);
    std::cout << "test_no_data_no_request PASS" << std::endl;
}

void test_invalid_endpoint_fails()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("ai_enabled", "true");
    db.setSetting("ai_api_key", "sk-test");
    db.setSetting("ai_api_endpoint", "http://127.0.0.1:1");
    seedTodaySession(db);

    AiClient ai(&db);
    ai.reloadSettings();
    QSignalSpy readySpy(&ai, &AiClient::reportReady);
    QSignalSpy failSpy(&ai, &AiClient::reportFailed);

    assert(ai.generateReport("daily"));
    // 本机无效端口应快速失败并发出 reportFailed。
    assert(failSpy.wait(10000));
    assert(readySpy.count() == 0);
    std::cout << "test_invalid_endpoint_fails PASS" << std::endl;
}

void test_cache_read()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("ai_report_daily_text", "\xe4\xbb\x8a\xe6\x97\xa5\xe6\x8a\xa5\xe5\x91\x8a");
    db.setSetting("ai_report_daily_date", "2026-08-12");
    db.setSetting("ai_report_weekly_text", "\xe6\x9c\xac\xe5\x91\xa8\xe6\x8a\xa5\xe5\x91\x8a");
    db.setSetting("ai_report_weekly_date", "2026-08-10");

    AiClient ai(&db);
    ai.reloadSettings();
    assert(ai.cachedReport(AiPeriod::daily()) == "\xe4\xbb\x8a\xe6\x97\xa5\xe6\x8a\xa5\xe5\x91\x8a");
    assert(ai.cachedReportDate(AiPeriod::daily()) == "2026-08-12");
    assert(ai.cachedReport(AiPeriod::weekly()) == "\xe6\x9c\xac\xe5\x91\xa8\xe6\x8a\xa5\xe5\x91\x8a");
    assert(ai.cachedReportDate(AiPeriod::weekly()) == "2026-08-10");
    assert(ai.cachedReport("other").isEmpty());
    assert(ai.cachedReportDate("other").isEmpty());
    std::cout << "test_cache_read PASS" << std::endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    test_disabled_no_request();
    test_missing_config_no_request();
    test_no_data_no_request();
    test_invalid_endpoint_fails();
    test_cache_read();
    std::cout << "All AI client tests passed!" << std::endl;
    return 0;
}
