#include <cassert>
#include <iostream>
#include <QCoreApplication>
#include <QTemporaryFile>
#include <QDateTime>
#include "database/database_manager.h"

void test_create_table()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    QVector<QVariantMap> sessions = db.getAllSessions();
    assert(sessions.isEmpty());
    std::cout << "test_create_table PASS" << std::endl;
}

void test_insert_and_query()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    QDateTime now = QDateTime::currentDateTime();
    qint64 sid = db.insertSession("chrome.exe", "Google", "Chrome", now, now, 60);
    assert(sid > 0);

    QVector<QVariantMap> summary = db.getTodaySummary();
    assert(summary.size() == 1);
    assert(summary[0]["app_name"].toString() == "Chrome");
    assert(summary[0]["total_seconds"].toInt() == 60);
    std::cout << "test_insert_and_query PASS" << std::endl;
}

void test_week_summary()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    QDateTime now = QDateTime::currentDateTime();
    db.insertSession("Code.exe", "test.py", "VS Code", now, now, 120);
    QVector<QVariantMap> week = db.getWeekSummary();
    assert(week.size() >= 1);
    assert(week[0]["total_seconds"].toInt() == 120);
    std::cout << "test_week_summary PASS" << std::endl;
}

void test_app_rank()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    QDateTime now = QDateTime::currentDateTime();
    db.insertSession("chrome.exe", "Google", "Chrome", now, now, 60);
    db.insertSession("Code.exe", "test.py", "VS Code", now, now, 120);
    QVector<QVariantMap> rank = db.getAppRank();
    assert(rank.size() == 2);
    assert(rank[0]["app_name"].toString() == "VS Code");
    assert(rank[0]["total_seconds"].toInt() == 120);
    std::cout << "test_app_rank PASS" << std::endl;
}

void test_update_session_end()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    QDateTime now = QDateTime::currentDateTime();
    qint64 sid = db.insertSession("chrome.exe", "Google", "Chrome", now, QDateTime(), 0);
    assert(sid > 0);

    QDateTime newEnd = now.addSecs(3600);
    db.updateSessionEnd(sid, newEnd, 3600);

    QVector<QVariantMap> all = db.getAllSessions();
    assert(all.size() == 1);
    assert(all[0]["end_time"].toString() == newEnd.toString(Qt::ISODate));
    assert(all[0]["duration_seconds"].toInt() == 3600);
    std::cout << "test_update_session_end PASS" << std::endl;
}

void test_settings_default()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    QString val = db.getSetting("tracking_enabled", "false");
    assert(val == "true");
    std::cout << "test_settings_default PASS" << std::endl;
}

void test_settings_set_get()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("custom_key", "custom_value");
    QString val = db.getSetting("custom_key", "");
    assert(val == "custom_value");
    std::cout << "test_settings_set_get PASS" << std::endl;
}

void test_settings_missing_returns_default()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    QString val = db.getSetting("nonexistent", "fallback");
    assert(val == "fallback");
    std::cout << "test_settings_missing_returns_default PASS" << std::endl;
}

void test_ignored_apps()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);

    QMap<int, QString> empty = db.getIgnoredApps();
    assert(empty.isEmpty());

    int id1 = db.addIgnoredApp("chrome.exe");
    assert(id1 > 0);
    int id2 = db.addIgnoredApp("explorer.exe");
    assert(id2 > 0);

    QMap<int, QString> apps = db.getIgnoredApps();
    assert(apps.size() == 2);
    assert(apps.values().contains("chrome.exe"));
    assert(apps.values().contains("explorer.exe"));

    db.removeIgnoredApp(id1);
    apps = db.getIgnoredApps();
    assert(apps.size() == 1);
    assert(!apps.values().contains("chrome.exe"));

    std::cout << "test_ignored_apps PASS" << std::endl;
}

void test_ignored_filtered_from_stats()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    QDateTime now = QDateTime::currentDateTime();

    db.insertSession("C:\\Program Files\\Chrome\\chrome.exe", "Google", "Chrome", now, now, 100);
    db.insertSession("C:\\Program Files\\Code\\Code.exe", "VS Code", "VS Code", now, now, 200);

    assert(db.getTodayTotal() == 300);
    QVector<QVariantMap> rank = db.getAppRank();
    assert(rank.size() == 2);

    db.addIgnoredApp("chrome.exe");

    int filteredTotal = db.getTodayTotal();
    assert(filteredTotal == 200);

    QVector<QVariantMap> filteredRank = db.getAppRank();
    assert(filteredRank.size() == 1);
    assert(filteredRank[0]["app_name"].toString() == "VS Code");

    QVector<QVariantMap> filteredSummary = db.getTodaySummary();
    assert(filteredSummary.size() == 1);
    assert(filteredSummary[0]["app_name"].toString() == "VS Code");

    QMap<int, QString> apps = db.getIgnoredApps();
    assert(apps.size() == 1);
    int id = apps.firstKey();
    db.removeIgnoredApp(id);

    assert(db.getTodayTotal() == 300);
    rank = db.getAppRank();
    assert(rank.size() == 2);

    std::cout << "test_ignored_filtered_from_stats PASS" << std::endl;
}

void test_app_aliases()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);

    QMap<QString, QString> empty = db.getAppAliases();
    assert(empty.isEmpty());

    db.setAppAlias("code.exe", "VS Code Custom");
    db.setAppAlias("chrome.exe", "My Chrome");

    QMap<QString, QString> aliases = db.getAppAliases();
    assert(aliases.size() == 2);
    assert(aliases["code.exe"] == "VS Code Custom");
    assert(aliases["chrome.exe"] == "My Chrome");

    db.removeAppAliasByProcessName("chrome.exe");
    aliases = db.getAppAliases();
    assert(aliases.size() == 1);
    assert(!aliases.contains("chrome.exe"));

    std::cout << "test_app_aliases PASS" << std::endl;
}

void test_get_all_known_process_names()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);

    QStringList empty = db.getAllKnownProcessNames();
    assert(empty.isEmpty());

    QDateTime now = QDateTime::currentDateTime();
    db.insertSession("chrome.exe", "Google", "Chrome", now, now, 60);
    db.insertSession("code.exe", "test.py", "VS Code", now, now, 120);
    db.insertSession("chrome.exe", "YouTube", "Chrome", now, now, 30);

    QStringList names = db.getAllKnownProcessNames();
    assert(names.size() == 2);
    assert(names.contains("chrome.exe"));
    assert(names.contains("code.exe"));

    std::cout << "test_get_all_known_process_names PASS" << std::endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    test_create_table();
    test_insert_and_query();
    test_week_summary();
    test_app_rank();
    test_update_session_end();
    test_settings_default();
    test_settings_set_get();
    test_settings_missing_returns_default();
    test_ignored_apps();
    test_ignored_filtered_from_stats();
    test_app_aliases();
    test_get_all_known_process_names();
    std::cout << "All database tests passed!" << std::endl;
    return 0;
}
