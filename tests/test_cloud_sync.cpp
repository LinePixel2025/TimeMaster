#include <cassert>
#include <iostream>
#include <QtTest>
#include <QCoreApplication>
#include <QTemporaryFile>
#include <QSignalSpy>
#include <QDateTime>
#include <QDate>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include "database/database_manager.h"
#include "push/lineweb_pusher.h"

namespace {

/// 最小化的健康 API 假服务：返回预置 JSON，记录收到的请求，便于断言。
/// 每个连接独立状态（QHash<socket, State>），支持分片到达。
class FakeHealthServer : public QObject
{
public:
    explicit FakeHealthServer(QObject *parent = nullptr) : QObject(parent)
    {
        m_server = new QTcpServer(this);
        assert(m_server->listen(QHostAddress::LocalHost, 0));
        connect(m_server, &QTcpServer::newConnection, this, &FakeHealthServer::onNewConnection);
    }

    QString baseUrl() const
    {
        return QString("http://127.0.0.1:%1").arg(m_server->serverPort());
    }

    void setGoalJson(const QJsonObject &obj) { m_goalJson = obj; }

    /// 关闭 daily-goal/data 的响应（返回空对象），用于模拟"云端未设置目标"。
    void setGoalResponseEnabled(bool enabled) { m_goalResponseEnabled = enabled; }

    struct Request {
        QString path;
        QString token;
        QByteArray body;
    };

    QVector<Request> requests() const { return m_requests; }

private:
    struct State {
        QByteArray requestLine;
        QByteArray tokenHeader;
        QByteArray body;
        int contentLength = 0;
        bool headersDone = false;
    };

    void onNewConnection()
    {
        QTcpSocket *socket = m_server->nextPendingConnection();
        m_states.insert(socket, State());
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            State &st = m_states[socket];
            while (socket->canReadLine()) {
                const QByteArray line = socket->readLine();
                if (st.headersDone) {
                    st.body += line;
                    if (st.body.size() >= st.contentLength) {
                        handleRequest(socket, st);
                        m_states.remove(socket);
                        socket->disconnectFromHost();
                        return;
                    }
                } else if (line == "\r\n") {
                    st.headersDone = true;
                    if (st.contentLength <= 0) {
                        handleRequest(socket, st);
                        m_states.remove(socket);
                        socket->disconnectFromHost();
                        return;
                    }
                } else if (st.requestLine.isEmpty()) {
                    st.requestLine = line.trimmed();
                } else if (line.startsWith("X-Screen-Time-Token:")) {
                    st.tokenHeader = line.mid(20).trimmed();
                } else if (line.toLower().startsWith("content-length:")) {
                    st.contentLength = line.mid(15).trimmed().toInt();
                }
            }
        });
    }

    void handleRequest(QTcpSocket *socket, const State &st)
    {
        Request req;
        const QStringList parts = QString::fromUtf8(st.requestLine).split(' ');
        if (parts.size() >= 2)
            req.path = parts[1];
        req.token = QString::fromUtf8(st.tokenHeader);
        req.body = st.body;
        m_requests.append(req);

        QByteArray response;
        if (m_goalResponseEnabled && req.path.endsWith("/daily-goal/data"))
            response = QJsonDocument(m_goalJson).toJson();
        else
            response = "{}";

        socket->write("HTTP/1.1 200 OK\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: " + QByteArray::number(response.size()) + "\r\n"
                      "Connection: close\r\n\r\n");
        socket->write(response);
        socket->flush();
    }

    QTcpServer *m_server = nullptr;
    QJsonObject m_goalJson;
    bool m_goalResponseEnabled = true;
    QHash<QTcpSocket *, State> m_states;
    QVector<Request> m_requests;
};

/// 插入一条 startTime~endTime 的 session。
void insertSessionBetween(DatabaseManager &db, const QDateTime &start, const QDateTime &end)
{
    db.insertSession("notepad.exe", "Untitled - Notepad", "Notepad",
                     start, end, static_cast<int>(start.secsTo(end)));
}

/// 插入一条从今天 01:00 起、时长 durationSeconds 的记录，保证 date(start_time) 稳定归属今天。
void insertSession(DatabaseManager &db, int durationSeconds)
{
    const QDateTime start = QDate::currentDate().startOfDay().addSecs(3600);
    insertSessionBetween(db, start, start.addSecs(durationSeconds));
}

bool hasPushRequest(const QVector<FakeHealthServer::Request> &requests,
                    const QString &date, int totalSeconds)
{
    for (const auto &req : requests) {
        if (req.path != "/api/health/push")
            continue;
        const QJsonObject body = QJsonDocument::fromJson(req.body).object();
        if (body["date"].toString() == date && body["totalSeconds"].toInt() == totalSeconds)
            return true;
    }
    return false;
}

/// 模拟退出前最终推送失败场景：断言失败后写入待补推日期。
void test_pushnow_failure_marks_pending()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("lineweb_enabled", "true");
    db.setSetting("lineweb_token", "st_test");
    db.setSetting("lineweb_endpoint", "http://127.0.0.1:1"); // 连接被拒

    LineWebPusher pusher(&db);
    pusher.start();
    QSignalSpy failedSpy(&pusher, &LineWebPusher::pushFailed);

    pusher.pushNow();
    assert(failedSpy.count() == 1);
    assert(db.getSetting("lineweb_pending_push", "")
           == QDate::currentDate().toString(Qt::ISODate));
    std::cout << "test_pushnow_failure_marks_pending PASS" << std::endl;
}

/// 云端目标 >0：写回 daily_goal 并发出 goalUpdated；超目标时发出 goalExceeded，当日仅一次。
void test_fetch_goal_writes_back_and_exceed_signal()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("daily_goal", "28800"); // 本地默认 8h
    insertSession(db, 5400);              // 今日已用 1.5h
    db.setSetting("lineweb_enabled", "true");
    db.setSetting("lineweb_token", "st_test");

    FakeHealthServer server;
    server.setGoalJson(QJsonObject{{"dailyGoalSeconds", 3600}}); // 云端目标 1h
    db.setSetting("lineweb_endpoint", server.baseUrl());

    LineWebPusher pusher(&db);
    QSignalSpy goalSpy(&pusher, &LineWebPusher::goalUpdated);
    QSignalSpy exceedSpy(&pusher, &LineWebPusher::goalExceeded);
    pusher.start();

    assert(goalSpy.wait(3000));
    assert(goalSpy.count() == 1);
    assert(goalSpy.takeFirst().at(0).toInt() == 3600);
    assert(db.getSetting("daily_goal", "") == "3600");
    // goalExceeded 在 goalUpdated 同一回调栈内同步发出，wait 会错过，用 count 轮询。
    QTRY_VERIFY_WITH_TIMEOUT(exceedSpy.count() == 1, 3000);
    assert(exceedSpy.takeFirst().at(0).toInt() == 30); // 超 30 分钟

    // 同日再次拉取不重复提醒。
    server.setGoalResponseEnabled(false);
    pusher.doPush();
    QTRY_VERIFY_WITH_TIMEOUT(hasPushRequest(server.requests(),
        QDate::currentDate().toString(Qt::ISODate), 5400), 3000);
    assert(exceedSpy.count() == 0);
    std::cout << "test_fetch_goal_writes_back_and_exceed_signal PASS" << std::endl;
}

/// 云端目标为 null：不覆盖本地 daily_goal。
void test_fetch_goal_null_keeps_local()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("daily_goal", "28800");
    insertSession(db, 3600);
    db.setSetting("lineweb_enabled", "true");
    db.setSetting("lineweb_token", "st_test");

    FakeHealthServer server;
    server.setGoalJson(QJsonObject{{"dailyGoalSeconds", QJsonValue(QJsonValue::Null)}});
    db.setSetting("lineweb_endpoint", server.baseUrl());

    LineWebPusher pusher(&db);
    QSignalSpy goalSpy(&pusher, &LineWebPusher::goalUpdated);
    QSignalSpy cloudSpy(&pusher, &LineWebPusher::cloudStateUpdated);
    pusher.start();

    // 等一次拉取完成（cloudStateUpdated 在目标响应后发出）。
    QTRY_VERIFY_WITH_TIMEOUT(cloudSpy.count() > 0, 3000);
    assert(goalSpy.count() == 0); // null 不写回
    assert(db.getSetting("daily_goal", "") == "28800");
    std::cout << "test_fetch_goal_null_keeps_local PASS" << std::endl;
}

/// 昨日补推：跨天后 doPush 先补推昨日日期，再推今日。
void test_yesterday_backfill()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("lineweb_enabled", "true");
    db.setSetting("lineweb_token", "st_test");

    FakeHealthServer server;
    server.setGoalJson(QJsonObject{{"dailyGoalSeconds", 7200}});
    db.setSetting("lineweb_endpoint", server.baseUrl());

    // 昨日 2h 记录（昨日 01:00~03:00）。
    const QDateTime yesterdayStart = QDate::currentDate().startOfDay().addDays(-1).addSecs(3600);
    insertSessionBetween(db, yesterdayStart, yesterdayStart.addSecs(7200));
    // 今日 1h 记录（今日 01:00~02:00）。
    const QDateTime todayStart = QDate::currentDate().startOfDay().addSecs(3600);
    insertSessionBetween(db, todayStart, todayStart.addSecs(3600));

    LineWebPusher pusher(&db);
    pusher.start();
    pusher.doPush(); // m_lastPushedDate 为空 → 触发昨日补推 + 今日推送

    const QString yesterday = QDate::currentDate().addDays(-1).toString(Qt::ISODate);
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    QTRY_VERIFY_WITH_TIMEOUT(hasPushRequest(server.requests(), yesterday, 7200), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(hasPushRequest(server.requests(), today, 3600), 3000);
    std::cout << "test_yesterday_backfill PASS" << std::endl;
}

/// 今日累计超过 86400 时发送端 clamp 到上限。
void test_total_seconds_clamped()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("lineweb_enabled", "true");
    db.setSetting("lineweb_token", "st_test");

    // 今日两条记录（均从零点开始），累计 126000 秒 > 86400。
    const QDateTime midnight = QDate::currentDate().startOfDay();
    insertSessionBetween(db, midnight, midnight.addSecs(43200));
    insertSessionBetween(db, midnight, midnight.addSecs(82800));

    FakeHealthServer server;
    server.setGoalJson(QJsonObject{{"dailyGoalSeconds", 7200}});
    db.setSetting("lineweb_endpoint", server.baseUrl());

    LineWebPusher pusher(&db);
    pusher.start();
    pusher.doPush();

    QTRY_VERIFY_WITH_TIMEOUT(hasPushRequest(server.requests(),
        QDate::currentDate().toString(Qt::ISODate), 86400), 3000);
    std::cout << "test_total_seconds_clamped PASS" << std::endl;
}

/// 云端请求（GET）携带 X-Screen-Time-Token，且两个读取端点都被调用。
void test_get_requests_carry_token()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    db.setSetting("lineweb_enabled", "true");
    db.setSetting("lineweb_token", "st_sync_token_abc");

    FakeHealthServer server;
    server.setGoalJson(QJsonObject{{"dailyGoalSeconds", 7200}});
    db.setSetting("lineweb_endpoint", server.baseUrl());

    LineWebPusher pusher(&db);
    pusher.start();

    QTRY_VERIFY_WITH_TIMEOUT(server.requests().size() >= 2, 3000);
    bool sawGoal = false;
    bool sawScreenTime = false;
    for (const auto &req : server.requests()) {
        if (req.path == "/api/health/daily-goal/data") {
            assert(req.token == "st_sync_token_abc");
            sawGoal = true;
        } else if (req.path == "/api/health/screen-time/data") {
            assert(req.token == "st_sync_token_abc");
            sawScreenTime = true;
        }
    }
    assert(sawGoal);
    assert(sawScreenTime);
    std::cout << "test_get_requests_carry_token PASS" << std::endl;
}

/// normalizeLineWebEndpoint：剥离尾斜杠与 /api/health/push 后缀。
void test_normalize_endpoint()
{
    assert(normalizeLineWebEndpoint("  http://host:3001/  ")
           == "http://host:3001");
    assert(normalizeLineWebEndpoint("http://host:3001/api/health/push")
           == "http://host:3001");
    assert(normalizeLineWebEndpoint("http://host:3001")
           == "http://host:3001");
    std::cout << "test_normalize_endpoint PASS" << std::endl;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    test_normalize_endpoint();
    test_pushnow_failure_marks_pending();
    test_fetch_goal_writes_back_and_exceed_signal();
    test_fetch_goal_null_keeps_local();
    test_yesterday_backfill();
    test_total_seconds_clamped();
    test_get_requests_carry_token();
    std::cout << "All cloud sync tests passed!" << std::endl;
    return 0;
}
