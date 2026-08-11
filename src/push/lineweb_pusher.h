#ifndef LINEWEB_PUSHER_H
#define LINEWEB_PUSHER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QDate>

class DatabaseManager;

/// 屏幕时间推送 API 单日秒数上限（服务端约定 0-86400）。
inline constexpr int kMaxTotalSeconds = 86400;

/// 归一化 LineWeb API 地址：去空白、尾斜杠，并去掉可能粘贴进来的 `/api/health/push` 后缀。
inline QString normalizeLineWebEndpoint(const QString &endpoint)
{
    QString result = endpoint.trimmed();
    while (result.endsWith(QLatin1Char('/')))
        result.chop(1);
    if (result.endsWith(QLatin1String("/api/health/push")))
        result = result.left(result.length() - 16);
    return result;
}

class LineWebPusher : public QObject
{
    Q_OBJECT
public:
    explicit LineWebPusher(DatabaseManager *db, QObject *parent = nullptr);

    void start();
    void stop();
    void pushNow();
    void reloadSettings();

    /// 周期推送入口：补推待补日期与昨日、推送今日并拉取云端状态。
    /// 公开以便测试直接触发（定时器间隔最短 5 分钟，测试不便等待）。
    void doPush();

signals:
    void pushSucceeded();
    void pushFailed(const QString &error);
    /// 云端每日目标已写回本地 daily_goal（goalSeconds > 0 时发出）。
    void goalUpdated(int goalSeconds);
    /// 当日首次超目标（overMinutes 为超出分钟数）。
    void goalExceeded(int overMinutes);
    /// 完成一次云端状态拉取（目标 + 今日时长）。
    void cloudStateUpdated();

private:
    void sendPush(const QDate &date, int totalSeconds);
    void pushDate(const QDate &date);
    void fetchCloudState();

    DatabaseManager *m_db;
    QNetworkAccessManager *m_nam;
    QTimer *m_timer;
    QString m_token;
    QString m_endpoint;
    int m_intervalMinutes = 10;
    bool m_enabled = false;
    /// 上次成功推送的日期（yyyy-MM-dd），用于跨天补推昨日。
    QString m_lastPushedDate;
    /// 待补推的日期（yyyy-MM-dd），为空表示无待补；持久化于 lineweb_pending_push。
    QString m_pendingPushDate;
    /// 当日已发出过超目标提醒的日期，保证每日只提醒一次。
    QString m_goalExceededDate;
};

#endif // LINEWEB_PUSHER_H
