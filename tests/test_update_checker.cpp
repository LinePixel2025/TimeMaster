#include <cassert>
#include <iostream>
#include <QtTest>
#include <QCoreApplication>
#include <QTemporaryFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QSignalSpy>
#include <QJsonObject>
#include <QJsonDocument>
#include <QVariant>
#include "database/database_manager.h"
#include "update/update_checker.h"

namespace {

/// 最小化的 GitHub Releases 假服务：对任意 GET 请求返回预置响应。
class FakeGitHubServer : public QObject
{
public:
    explicit FakeGitHubServer(QObject *parent = nullptr) : QObject(parent)
    {
        m_server = new QTcpServer(this);
        assert(m_server->listen(QHostAddress::LocalHost, 0));
        connect(m_server, &QTcpServer::newConnection, this, [this]() {
            QTcpSocket *socket = m_server->nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                while (socket->canReadLine()) {
                    if (socket->readLine() == "\r\n") { // 请求头结束
                        const QByteArray statusLine =
                            m_status == 200
                                ? QByteArray("HTTP/1.1 200 OK\r\n")
                                : QByteArray("HTTP/1.1 404 Not Found\r\n");
                        socket->write(statusLine);
                        socket->write("Content-Type: application/json\r\n");
                        socket->write("Content-Length: "
                                      + QByteArray::number(m_body.size())
                                      + "\r\n");
                        socket->write("Connection: close\r\n\r\n");
                        socket->write(m_body);
                        socket->flush();
                        socket->disconnectFromHost();
                        return;
                    }
                }
            });
        });
    }

    QString baseUrl() const
    {
        return QString("http://127.0.0.1:%1").arg(m_server->serverPort());
    }

    void setResponse(const QByteArray &body, int status = 200)
    {
        m_body = body;
        m_status = status;
    }

private:
    QTcpServer *m_server = nullptr;
    QByteArray m_body;
    int m_status = 200;
};

/// 构造一份完整的 releases/latest 响应 JSON（v9.9.9 + exe 资产）。
QByteArray buildReleaseJson(const QString &tag, const QString &exeAsset = QString())
{
    QJsonObject asset;
    if (!exeAsset.isEmpty()) {
        asset = QJsonObject{{"name", exeAsset},
                            {"browser_download_url",
                             "https://github.com/LinePixel2025/TimeMaster/releases/download/"
                                 + tag + "/" + exeAsset}};
    }
    QJsonObject obj{
        {"tag_name", tag},
        {"name", QString("Time Master ") + tag},
        {"body", "## 新功能\n- 更新检查\n- **自动提醒**\n"},
        {"html_url",
         QString("https://github.com/LinePixel2025/TimeMaster/releases/tag/") + tag},
        {"published_at", "2026-09-06T03:22:31Z"},
    };
    if (!exeAsset.isEmpty())
        obj.insert("assets", QJsonArray{asset});
    else
        obj.insert("assets", QJsonArray{});
    return QJsonDocument(obj).toJson();
}

/// 创建临时数据库路径（文件由 QTemporaryFile 创建并关闭，DatabaseManager 复用）。
QString makeTempDbPath()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();
    return path;
}

void test_is_newer_version()
{
    // 普通递增
    assert(UpdateChecker::isNewerVersion("5.6.5", "5.6.4"));
    assert(!UpdateChecker::isNewerVersion("5.6.4", "5.6.4"));
    assert(!UpdateChecker::isNewerVersion("5.6.3", "5.6.4"));
    // 段数不等与十位进位
    assert(UpdateChecker::isNewerVersion("5.6.10", "5.6.9"));
    assert(UpdateChecker::isNewerVersion("5.6.1", "5.6"));
    assert(!UpdateChecker::isNewerVersion("5.6", "5.6.1"));
    // v 前缀
    assert(UpdateChecker::isNewerVersion("v5.6.5", "5.6.4"));
    // 非数字段无法比较：视为无更新
    assert(!UpdateChecker::isNewerVersion("5.6.4-rc1", "5.6.4"));
    assert(!UpdateChecker::isNewerVersion("", "5.6.4"));
    assert(!UpdateChecker::isNewerVersion("5.6.4", ""));
    assert(!UpdateChecker::isNewerVersion("abc", "5.6.4"));
    std::cout << "test_is_newer_version PASS" << std::endl;
}

void test_parse_release()
{
    bool ok = false;
    // 完整 JSON：exe 资产被选中，字段正确解析。
    UpdateInfo info = UpdateChecker::parseRelease(
        buildReleaseJson("v9.9.9", "TimeMaster-Setup-9.9.9.exe"), &ok);
    assert(ok);
    assert(info.version == "9.9.9");
    assert(info.tagName == "v9.9.9");
    assert(info.notes.contains("更新检查"));
    assert(info.releaseUrl.endsWith("/releases/tag/v9.9.9"));
    assert(info.publishedAt == "2026-09-06");
    assert(info.downloadUrl.endsWith("TimeMaster-Setup-9.9.9.exe"));

    // 无资产：downloadUrl 为空但解析成功。
    info = UpdateChecker::parseRelease(buildReleaseJson("v9.9.9"), &ok);
    assert(ok);
    assert(info.version == "9.9.9");
    assert(info.downloadUrl.isEmpty());

    // 坏 JSON / 缺 tag_name：失败。
    info = UpdateChecker::parseRelease("not json", &ok);
    assert(!ok);
    info = UpdateChecker::parseRelease("{\"name\":\"x\"}", &ok);
    assert(!ok);
    std::cout << "test_parse_release PASS" << std::endl;
}

/// 自动检查发现新版本：信号、缓存写入、notified 去重（同版本不再提醒）。
void test_check_new_version_signals_and_cache()
{
    DatabaseManager db(makeTempDbPath());
    FakeGitHubServer server;
    server.setResponse(buildReleaseJson("v9.9.9", "TimeMaster-Setup-9.9.9.exe"));

    UpdateChecker updater(&db);
    updater.setApiUrlForTest(server.baseUrl() + "/releases/latest");
    QSignalSpy checkSpy(&updater, &UpdateChecker::checkFinished);
    QSignalSpy availSpy(&updater, &UpdateChecker::updateAvailable);

    assert(updater.checkNow(false));
    assert(checkSpy.wait(5000));
    assert(checkSpy.count() == 1);
    assert(checkSpy[0].at(0).toBool()); // hasUpdate
    const UpdateInfo info =
        qvariant_cast<UpdateInfo>(checkSpy[0].at(1));
    assert(info.version == "9.9.9");
    assert(info.downloadUrl.endsWith("TimeMaster-Setup-9.9.9.exe"));
    assert(availSpy.count() == 1);

    // 缓存与去重键写入。
    assert(db.getSetting("update_latest_version", "") == "9.9.9");
    assert(db.getSetting("update_notified_version", "") == "9.9.9");
    assert(!db.getSetting("update_last_check", "").isEmpty());

    // 同一版本再次自动检查：checkFinished 照发，updateAvailable 去重不发。
    assert(updater.checkNow(false));
    assert(checkSpy.wait(5000));
    assert(checkSpy.count() == 2);
    assert(availSpy.count() == 1);

    std::cout << "test_check_new_version_signals_and_cache PASS" << std::endl;
}

/// 手动检查（设置页）：只发 checkFinished，不产生 updateAvailable（避免与设置页弹窗重复）。
void test_check_manual_no_auto_notify()
{
    DatabaseManager db(makeTempDbPath());
    FakeGitHubServer server;
    server.setResponse(buildReleaseJson("v9.8.0", "TimeMaster-Setup-9.8.0.exe"));

    UpdateChecker updater(&db);
    updater.setApiUrlForTest(server.baseUrl() + "/releases/latest");
    QSignalSpy checkSpy(&updater, &UpdateChecker::checkFinished);
    QSignalSpy availSpy(&updater, &UpdateChecker::updateAvailable);

    assert(updater.checkNow(true));
    assert(checkSpy.wait(5000));
    assert(checkSpy.count() == 1);
    assert(checkSpy[0].at(0).toBool());
    assert(availSpy.count() == 0);
    // 手动检查不写 notified 键，自动检查仍会提醒一次。
    assert(db.getSetting("update_notified_version", "").isEmpty());
    std::cout << "test_check_manual_no_auto_notify PASS" << std::endl;
}

/// 远程版本不高于本地：hasUpdate=false，无 updateAvailable。
void test_check_older_version()
{
    DatabaseManager db(makeTempDbPath());
    FakeGitHubServer server;
    server.setResponse(buildReleaseJson("v5.0.0"));

    UpdateChecker updater(&db);
    updater.setApiUrlForTest(server.baseUrl() + "/releases/latest");
    QSignalSpy checkSpy(&updater, &UpdateChecker::checkFinished);
    QSignalSpy availSpy(&updater, &UpdateChecker::updateAvailable);

    assert(updater.checkNow(false));
    assert(checkSpy.wait(5000));
    assert(checkSpy.count() == 1);
    assert(!checkSpy[0].at(0).toBool());
    assert(availSpy.count() == 0);
    std::cout << "test_check_older_version PASS" << std::endl;
}

/// 网络/HTTP 失败：checkFinished 携带错误、无更新信号、缓存不被污染。
void test_check_failure()
{
    DatabaseManager db(makeTempDbPath());
    FakeGitHubServer server;
    server.setResponse("Not Found", 404);

    UpdateChecker updater(&db);
    updater.setApiUrlForTest(server.baseUrl() + "/releases/latest");
    QSignalSpy checkSpy(&updater, &UpdateChecker::checkFinished);
    QSignalSpy availSpy(&updater, &UpdateChecker::updateAvailable);

    assert(updater.checkNow(false));
    assert(checkSpy.wait(5000));
    assert(checkSpy.count() == 1);
    assert(!checkSpy[0].at(0).toBool());
    assert(!checkSpy[0].at(2).toString().isEmpty());
    assert(availSpy.count() == 0);
    assert(db.getSetting("update_last_check", "").isEmpty());
    assert(db.getSetting("update_latest_version", "").isEmpty());

    // 坏 JSON（200 但不可解析）同样走失败路径。
    server.setResponse("{broken", 200);
    assert(updater.checkNow(false));
    assert(checkSpy.wait(5000));
    assert(checkSpy.count() == 2);
    assert(!checkSpy[1].at(0).toBool());
    assert(!checkSpy[1].at(2).toString().isEmpty());
    std::cout << "test_check_failure PASS" << std::endl;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    test_is_newer_version();
    test_parse_release();
    test_check_new_version_signals_and_cache();
    test_check_manual_no_auto_notify();
    test_check_older_version();
    test_check_failure();
    std::cout << "All update checker tests passed!" << std::endl;
    return 0;
}
