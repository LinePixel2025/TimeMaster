#include <cassert>
#include <iostream>
#include <QCoreApplication>
#include <QTemporaryFile>
#include <QSignalSpy>
#include "database/database_manager.h"
#include "push/lineweb_pusher.h"

void test_push_signals_on_invalid_endpoint()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("lineweb_enabled", "true");
    db.setSetting("lineweb_token", "st_test_token");
    db.setSetting("lineweb_endpoint", "http://127.0.0.1:1");
    db.setSetting("lineweb_interval", "10");

    LineWebPusher pusher(&db);
    pusher.start();
    QSignalSpy failedSpy(&pusher, &LineWebPusher::pushFailed);

    pusher.pushNow();

    assert(failedSpy.count() == 1);
    std::cout << "test_push_signals_on_invalid_endpoint PASS" << std::endl;
}

void test_disabled_does_not_start_timer()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("lineweb_enabled", "false");
    db.setSetting("lineweb_token", "st_test");
    db.setSetting("lineweb_endpoint", "http://example.com");
    db.setSetting("lineweb_interval", "10");

    LineWebPusher pusher(&db);
    pusher.start();

    QSignalSpy successSpy(&pusher, &LineWebPusher::pushSucceeded);
    QSignalSpy failSpy(&pusher, &LineWebPusher::pushFailed);

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    assert(successSpy.count() == 0);
    assert(failSpy.count() == 0);
    std::cout << "test_disabled_does_not_start_timer PASS" << std::endl;
}

void test_missing_token_no_push()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("lineweb_enabled", "true");
    db.setSetting("lineweb_token", "");
    db.setSetting("lineweb_endpoint", "http://example.com");
    db.setSetting("lineweb_interval", "10");

    LineWebPusher pusher(&db);
    pusher.start();

    QSignalSpy successSpy(&pusher, &LineWebPusher::pushSucceeded);
    QSignalSpy failSpy(&pusher, &LineWebPusher::pushFailed);

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    assert(successSpy.count() == 0);
    assert(failSpy.count() == 0);
    std::cout << "test_missing_token_no_push PASS" << std::endl;
}

void test_request_body_format()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("lineweb_enabled", "true");
    db.setSetting("lineweb_token", "st_test");
    db.setSetting("lineweb_endpoint", "http://127.0.0.1:2");
    db.setSetting("lineweb_interval", "10");

    LineWebPusher pusher(&db);
    pusher.start();
    QSignalSpy failedSpy(&pusher, &LineWebPusher::pushFailed);

    pusher.pushNow();
    assert(failedSpy.count() == 1);
    std::cout << "test_request_body_format PASS" << std::endl;
}

void test_pushnow_timeout()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("lineweb_enabled", "true");
    db.setSetting("lineweb_token", "st_test");
    db.setSetting("lineweb_endpoint", "http://10.255.255.1");
    db.setSetting("lineweb_interval", "10");

    LineWebPusher pusher(&db);
    pusher.start();
    QSignalSpy failedSpy(&pusher, &LineWebPusher::pushFailed);

    pusher.pushNow();

    assert(failedSpy.count() >= 1);
    std::cout << "test_pushnow_timeout PASS" << std::endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    test_push_signals_on_invalid_endpoint();
    test_disabled_does_not_start_timer();
    test_missing_token_no_push();
    test_request_body_format();
    test_pushnow_timeout();
    std::cout << "All LineWeb pusher tests passed!" << std::endl;
    return 0;
}
