#include "update/update_checker.h"
#include "database/database_manager.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDateTime>
#include <QUrl>
#include <QCoreApplication>

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif

namespace {

/// 默认的 GitHub API 端点：仓库最新 Release。
const char *kDefaultApiUrl =
    "https://api.github.com/repos/LinePixel2025/TimeMaster/releases/latest";

/// 启动后延迟多久做首次检查（60 秒，避免启动瞬间打扰用户）。
const int kInitialDelayMs = 60 * 1000;

/// 自动检查周期（24 小时）。
const int kCheckIntervalMs = 24 * 60 * 60 * 1000;

/// GitHub API 请求超时（15 秒）。
const int kRequestTimeoutMs = 15000;

/// 去掉版本号开头的 v/V 前缀（tag 形如 v5.6.4）。
QString stripTagPrefix(QString version)
{
    version = version.trimmed();
    if (version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        version.remove(0, 1);
    return version;
}

} // namespace

UpdateChecker::UpdateChecker(DatabaseManager *db, QObject *parent)
    : QObject(parent), m_db(db), m_apiUrl(QLatin1String(kDefaultApiUrl))
{
    qRegisterMetaType<UpdateInfo>("UpdateInfo");

    m_nam = new QNetworkAccessManager(this);
    m_timer = new QTimer(this);
    m_timer->setInterval(kCheckIntervalMs);
}

void UpdateChecker::start()
{
    // 首次检查延迟触发；repeat timer 负责后续周期检查。
    QTimer::singleShot(kInitialDelayMs, this, [this]() { checkNow(false); });
    m_timer->start();
}

void UpdateChecker::stop()
{
    m_timer->stop();
}

bool UpdateChecker::checkNow(bool manual)
{
    if (m_reply) // 已有请求在途，防重入
        return false;

    QNetworkRequest req{QUrl(m_apiUrl)};
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("TimeMaster/") + QLatin1String(APP_VERSION));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setTransferTimeout(kRequestTimeoutMs);
    m_manual = manual;
    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::finished,
            this, &UpdateChecker::onReplyFinished);
    return true;
}

void UpdateChecker::onReplyFinished()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (!reply)
        return;
    reply->deleteLater();

    const bool manual = m_manual;
    m_manual = false;

    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 400) {
        finishCheck(UpdateInfo(), false,
                    QStringLiteral("GitHub 返回 HTTP 状态码 %1").arg(status));
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        finishCheck(UpdateInfo(), false, reply->errorString());
        return;
    }

    bool ok = false;
    const UpdateInfo info = parseRelease(reply->readAll(), &ok);
    if (!ok) {
        finishCheck(UpdateInfo(), false,
                    QString::fromUtf8("响应解析失败，请稍后重试"));
        return;
    }

    const bool hasUpdate = isNewerVersion(info.version, QLatin1String(APP_VERSION));
    persistResult(info);
    finishCheck(info, hasUpdate, QString());
    if (hasUpdate && !manual) {
        // 同一版本只自动提醒一次（跨重启去重）。
        const QString notified =
            m_db->getSetting("update_notified_version", QString());
        if (info.version != notified) {
            m_db->setSetting("update_notified_version", info.version);
            emit updateAvailable(info);
        }
    }
}

void UpdateChecker::finishCheck(const UpdateInfo &info, bool hasUpdate,
                                const QString &error)
{
    emit checkFinished(hasUpdate, info, error);
}

void UpdateChecker::persistResult(const UpdateInfo &info)
{
    m_db->setSetting("update_latest_version", info.version);
    m_db->setSetting("update_latest_published", info.publishedAt);
    m_db->setSetting("update_latest_notes", info.notes);
    m_db->setSetting("update_latest_url", info.releaseUrl);
    m_db->setSetting("update_latest_download", info.downloadUrl);
    m_db->setSetting("update_last_check",
                     QDateTime::currentDateTime().toString(
                         QStringLiteral("yyyy-MM-dd hh:mm:ss")));
}

UpdateInfo UpdateChecker::cachedInfo() const
{
    UpdateInfo info;
    info.version = m_db->getSetting("update_latest_version", QString());
    info.publishedAt = m_db->getSetting("update_latest_published", QString());
    info.notes = m_db->getSetting("update_latest_notes", QString());
    info.releaseUrl = m_db->getSetting("update_latest_url", QString());
    info.downloadUrl = m_db->getSetting("update_latest_download", QString());
    return info;
}

bool UpdateChecker::isNewerVersion(const QString &remote, const QString &local)
{
    const QString r = stripTagPrefix(remote);
    const QString l = stripTagPrefix(local);
    if (r.isEmpty() || l.isEmpty())
        return false;

    const QStringList rParts = r.split(QLatin1Char('.'));
    const QStringList lParts = l.split(QLatin1Char('.'));

    bool ok = false;
    QVector<int> rNums;
    for (const QString &part : rParts) {
        const int num = part.toInt(&ok);
        if (!ok)
            return false; // 非纯数字段（如 rc/alpha）无法比较，视为无更新
        rNums.append(num);
    }
    QVector<int> lNums;
    for (const QString &part : lParts) {
        const int num = part.toInt(&ok);
        if (!ok)
            return false;
        lNums.append(num);
    }

    // 段数不等时较短者补 0（5.6 == 5.6.0）。
    const int count = qMax(rNums.size(), lNums.size());
    for (int i = 0; i < count; ++i) {
        const int rv = i < rNums.size() ? rNums[i] : 0;
        const int lv = i < lNums.size() ? lNums[i] : 0;
        if (rv != lv)
            return rv > lv;
    }
    return false;
}

UpdateInfo UpdateChecker::parseRelease(const QByteArray &json, bool *ok)
{
    if (ok)
        *ok = false;
    UpdateInfo info;

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject())
        return info;

    const QJsonObject obj = doc.object();
    const QString tag = obj.value(QStringLiteral("tag_name")).toString();
    if (tag.isEmpty())
        return info;

    info.tagName = tag;
    info.version = stripTagPrefix(tag);
    info.notes = obj.value(QStringLiteral("body")).toString();
    info.releaseUrl = obj.value(QStringLiteral("html_url")).toString();
    info.publishedAt = obj.value(QStringLiteral("published_at"))
                           .toString().left(10);

    // 安装包直链：取第一个 .exe 资产。
    const QJsonArray assets =
        obj.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &assetVal : assets) {
        const QJsonObject asset = assetVal.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        if (name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
            info.downloadUrl = asset.value(QStringLiteral("browser_download_url"))
                                   .toString();
            break;
        }
    }

    if (ok)
        *ok = true;
    return info;
}

void UpdateChecker::setApiUrlForTest(const QString &url)
{
    m_apiUrl = url;
}
