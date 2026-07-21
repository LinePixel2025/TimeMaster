#include "lineweb_pusher.h"
#include "database/database_manager.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDate>
#include <QDateTime>
#include <QUrl>
#include <QEventLoop>
#include <QTimer>

namespace {

QString normalizeEndpoint(const QString &endpoint)
{
    QString result = endpoint.trimmed();
    while (result.endsWith('/'))
        result.chop(1);
    if (result.endsWith("/api/health/push"))
        result = result.left(result.length() - 16);
    return result;
}

} // namespace

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
    int totalSeconds = m_db->getTodayTotal();
    QDate today = QDate::currentDate();

    QJsonObject body;
    body["totalSeconds"] = totalSeconds;
    body["date"] = today.toString("yyyy-MM-dd");

    QUrl url(normalizeEndpoint(m_endpoint) + "/api/health/push");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("X-Screen-Time-Token", m_token.toUtf8());

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "[LineWeb] 推送成功";
            m_db->setSetting("lineweb_last_push",
                QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " 成功");
            emit pushSucceeded();
        } else {
            QString err = reply->errorString();
            qWarning() << "[LineWeb] 推送失败:" << err;
            m_db->setSetting("lineweb_last_push",
                QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " 失败");
            emit pushFailed(err);
        }
        reply->deleteLater();
    });
}

void LineWebPusher::pushNow()
{
    if (!m_enabled || m_token.isEmpty() || m_endpoint.isEmpty())
        return;

    int totalSeconds = m_db->getTodayTotal();
    QDate today = QDate::currentDate();

    QJsonObject body;
    body["totalSeconds"] = totalSeconds;
    body["date"] = today.toString("yyyy-MM-dd");

    QUrl url(normalizeEndpoint(m_endpoint) + "/api/health/push");
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
            m_db->setSetting("lineweb_last_push",
                QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " 成功");
            emit pushSucceeded();
        } else {
            QString err = reply->errorString();
            qWarning() << "[LineWeb] 最终推送失败:" << err;
            m_db->setSetting("lineweb_last_push",
                QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " 失败");
            emit pushFailed(err);
        }
    } else {
        reply->abort();
        qWarning() << "[LineWeb] 最终推送超时";
        m_db->setSetting("lineweb_last_push",
            QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " 超时");
        emit pushFailed("timeout");
    }
    reply->deleteLater();
}
