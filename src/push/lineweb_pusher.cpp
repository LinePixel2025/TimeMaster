#include "lineweb_pusher.h"
#include "database/database_manager.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDateTime>
#include <QUrl>
#include <QEventLoop>

LineWebPusher::LineWebPusher(DatabaseManager *db, QObject *parent)
    : QObject(parent), m_db(db)
{
    m_nam = new QNetworkAccessManager(this);
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &LineWebPusher::doPush);
}

void LineWebPusher::start()
{
    reloadSettings();
    // 启动即拉取一次云端目标与今日时长，推送仍等第一个周期。
    fetchCloudState();
}

void LineWebPusher::stop()
{
    m_timer->stop();
}

void LineWebPusher::reloadSettings()
{
    bool wasEnabled = m_enabled;
    QString oldToken = m_token;
    QString oldEndpoint = m_endpoint;
    int oldInterval = m_intervalMinutes;

    m_enabled = (m_db->getSetting("lineweb_enabled", "false") == "true");
    m_token = m_db->getSetting("lineweb_token", "");
    m_endpoint = m_db->getSetting("lineweb_endpoint", "");
    m_intervalMinutes = m_db->getSetting("lineweb_interval", "10").toInt();
    m_pendingPushDate = m_db->getSetting("lineweb_pending_push", "");

    if (m_enabled != wasEnabled
        || m_token != oldToken
        || m_endpoint != oldEndpoint
        || m_intervalMinutes != oldInterval)
    {
        m_timer->stop();
        if (m_enabled && !m_token.isEmpty() && !m_endpoint.isEmpty())
            m_timer->start(m_intervalMinutes * 60 * 1000);
    }
}

void LineWebPusher::doPush()
{
    if (!m_enabled || m_token.isEmpty() || m_endpoint.isEmpty())
        return;

    const QDate today = QDate::currentDate();    const QString todayStr = today.toString(Qt::ISODate);

    // 1. 补推上次失败保留的日期（服务端按日期覆盖写入，重复推送无害）。
    if (!m_pendingPushDate.isEmpty() && m_pendingPushDate != todayStr)
        pushDate(QDate::fromString(m_pendingPushDate, Qt::ISODate));

    // 2. 跨天补推昨日，保证历史逐日累积；pending 已覆盖昨日时不重复。
    const QDate yesterday = today.addDays(-1);
    const QString yesterdayStr = yesterday.toString(Qt::ISODate);
    if (m_lastPushedDate != todayStr && m_pendingPushDate != yesterdayStr)
        pushDate(yesterday);

    // 3. 推送今日累计值；今日推送成功回调中顺带拉取云端状态。
    sendPush(today, qBound(0, m_db->getTodayTotal(), kMaxTotalSeconds));

    m_lastPushedDate = todayStr;
}

bool LineWebPusher::syncNow()
{
    if (!m_enabled || m_token.isEmpty() || m_endpoint.isEmpty())
        return false;
    doPush();
    return true;
}

void LineWebPusher::sendPush(const QDate &date, int totalSeconds)
{
    const QString dateStr = date.toString(Qt::ISODate);

    QJsonObject body;
    body["totalSeconds"] = qBound(0, totalSeconds, kMaxTotalSeconds);
    body["date"] = dateStr;

    QUrl url(normalizeLineWebEndpoint(m_endpoint) + "/api/health/push");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("X-Screen-Time-Token", m_token.toUtf8());

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, dateStr]() {
        const QString todayStr = QDate::currentDate().toString(Qt::ISODate);
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "[LineWeb] 推送成功:" << dateStr;
            if (m_pendingPushDate == dateStr) {
                m_pendingPushDate.clear();
                m_db->setSetting("lineweb_pending_push", "");
            }
            m_db->setSetting("lineweb_last_push",
                QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " 成功");
            emit pushSucceeded();
            // 今日推送成功后顺带拉取云端目标与今日时长（绑定推送周期）。
            if (dateStr == todayStr)
                fetchCloudState();
        } else {
            QString err = reply->errorString();
            qWarning() << "[LineWeb] 推送失败:" << dateStr << err;
            if (dateStr == todayStr) {
                m_pendingPushDate = dateStr;
                m_db->setSetting("lineweb_pending_push", dateStr);
            }
            m_db->setSetting("lineweb_last_push",
                QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " 失败");
            emit pushFailed(err);
        }
        reply->deleteLater();
    });
}

void LineWebPusher::pushDate(const QDate &date)
{
    const QString dateStr = date.toString(Qt::ISODate);
    int total = 0;
    const auto rows = m_db->getDailySummaries(dateStr, dateStr);
    for (const auto &row : rows)
        total += row["total_seconds"].toInt();
    if (total <= 0)
        return; // 该日无记录，无需推送。
    sendPush(date, qBound(0, total, kMaxTotalSeconds));
}

void LineWebPusher::fetchCloudState()
{
    if (!m_enabled || m_token.isEmpty() || m_endpoint.isEmpty())
        return;

    // 读取云端每日目标（st_ Token），有值则覆盖本地 daily_goal；null 时不覆盖（本地兜底）。
    QUrl goalUrl(normalizeLineWebEndpoint(m_endpoint) + "/api/health/daily-goal/data");
    QNetworkRequest goalReq(goalUrl);
    goalReq.setRawHeader("X-Screen-Time-Token", m_token.toUtf8());
    QNetworkReply *goalReply = m_nam->get(goalReq);
    connect(goalReply, &QNetworkReply::finished, this, [this, goalReply]() {
        if (goalReply->error() == QNetworkReply::NoError) {
            const QJsonObject obj = QJsonDocument::fromJson(goalReply->readAll()).object();
            if (obj.contains("dailyGoalSeconds") && obj["dailyGoalSeconds"].isDouble()) {
                const int goal = qBound(0, obj["dailyGoalSeconds"].toInt(), kMaxTotalSeconds);
                if (goal > 0) {
                    m_db->setSetting("daily_goal", QString::number(goal));
                    emit goalUpdated(goal);
                    // 超目标提醒：当日仅一次。
                    const int todayTotal = m_db->getTodayTotal();
                    const QString todayStr = QDate::currentDate().toString(Qt::ISODate);
                    if (todayTotal > goal && m_goalExceededDate != todayStr) {
                        m_goalExceededDate = todayStr;
                        emit goalExceeded((todayTotal - goal) / 60);
                    }
                }
            }
        }
        goalReply->deleteLater();
        emit cloudStateUpdated();
    });

    // 读取云端今日屏幕时长，写入 last_fetch 供设置界面展示。
    QUrl stUrl(normalizeLineWebEndpoint(m_endpoint) + "/api/health/screen-time/data");
    QNetworkRequest stReq(stUrl);
    stReq.setRawHeader("X-Screen-Time-Token", m_token.toUtf8());
    QNetworkReply *stReply = m_nam->get(stReq);
    connect(stReply, &QNetworkReply::finished, this, [this, stReply]() {
        if (stReply->error() == QNetworkReply::NoError) {
            const QJsonObject obj = QJsonDocument::fromJson(stReply->readAll()).object();
            if (obj.contains("totalSeconds") && obj["totalSeconds"].isDouble()) {
                const int cloudTotal = qBound(0, obj["totalSeconds"].toInt(), kMaxTotalSeconds);
                const int h = cloudTotal / 3600;
                const int m = (cloudTotal % 3600) / 60;
                m_db->setSetting("lineweb_last_fetch",
                    QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                        + QString::fromUtf8(" \xc2\xb7 \xe4\xba\x91\xe7\xab\xaf %1h %2m").arg(h).arg(m));
            }
        }
        stReply->deleteLater();
    });
}

void LineWebPusher::pushNow()
{
    if (!m_enabled || m_token.isEmpty() || m_endpoint.isEmpty())
        return;

    const QDate today = QDate::currentDate();
    const QString todayStr = today.toString(Qt::ISODate);

    QJsonObject body;
    body["totalSeconds"] = qBound(0, m_db->getTodayTotal(), kMaxTotalSeconds);
    body["date"] = todayStr;

    QUrl url(normalizeLineWebEndpoint(m_endpoint) + "/api/health/push");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("X-Screen-Time-Token", m_token.toUtf8());

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(body).toJson());

    QEventLoop loop;
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->isFinished()) {
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "[LineWeb] 最终推送成功";
            if (m_pendingPushDate == todayStr) {
                m_pendingPushDate.clear();
                m_db->setSetting("lineweb_pending_push", "");
            }
            m_db->setSetting("lineweb_last_push",
                QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " 成功");
            emit pushSucceeded();
        } else {
            QString err = reply->errorString();
            qWarning() << "[LineWeb] 最终推送失败:" << err;
            // 退出前最终推送失败 → 记录待补推日期，下次启动自动补推，防数据丢失。
            m_pendingPushDate = todayStr;
            m_db->setSetting("lineweb_pending_push", todayStr);
            m_db->setSetting("lineweb_last_push",
                QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " 失败");
            emit pushFailed(err);
        }
    } else {
        reply->abort();
        qWarning() << "[LineWeb] 最终推送超时";
        m_pendingPushDate = todayStr;
        m_db->setSetting("lineweb_pending_push", todayStr);
        m_db->setSetting("lineweb_last_push",
            QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " 超时");
        emit pushFailed("timeout");
    }
    reply->deleteLater();
}
