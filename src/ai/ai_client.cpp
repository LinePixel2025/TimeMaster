#include "ai/ai_client.h"
#include "database/database_manager.h"

#include <QDate>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QVector>
#include <QtGlobal>

#include <algorithm>

namespace {

// 周一到周日的简体中文名，QDate::dayOfWeek() 返回 1=周一 … 7=周日。
QString dayOfWeekCn(int dayOfWeek)
{
    static const QString kNames[] = {
        QStringLiteral("周一"), QStringLiteral("周二"), QStringLiteral("周三"),
        QStringLiteral("周四"), QStringLiteral("周五"), QStringLiteral("周六"),
        QStringLiteral("周日")
    };
    return (dayOfWeek >= 1 && dayOfWeek <= 7) ? kNames[dayOfWeek - 1] : QString();
}

} // namespace

AiClient::AiClient(DatabaseManager *db, QObject *parent)
    : QObject(parent), m_db(db)
{
    m_nam = new QNetworkAccessManager(this);
}

void AiClient::reloadSettings()
{
    m_enabled = (m_db->getSetting("ai_enabled", "false") == "true");
    m_endpoint = m_db->getSetting("ai_api_endpoint",
                                  QStringLiteral("https://api.deepseek.com"));
    m_apiKey = m_db->getSetting("ai_api_key", "");
    m_model = m_db->getSetting("ai_model", "deepseek-chat");
}

bool AiClient::isConfigured() const
{
    return m_enabled && !m_apiKey.isEmpty() && !m_endpoint.isEmpty();
}

bool AiClient::generateReport(const QString &period)
{
    if (!m_enabled || m_apiKey.isEmpty() || m_endpoint.isEmpty())
        return false;
    if (period != AiPeriod::daily() && period != AiPeriod::weekly())
        return false;
    if (!hasDataForPeriod(period))
        return false;
    sendChat(period, buildPrompt(period));
    return true;
}

QString AiClient::cachedReport(const QString &period) const
{
    if (period == AiPeriod::daily())
        return m_db->getSetting("ai_report_daily_text", "");
    if (period == AiPeriod::weekly())
        return m_db->getSetting("ai_report_weekly_text", "");
    return QString();
}

QString AiClient::cachedReportDate(const QString &period) const
{
    if (period == AiPeriod::daily())
        return m_db->getSetting("ai_report_daily_date", "");
    if (period == AiPeriod::weekly())
        return m_db->getSetting("ai_report_weekly_date", "");
    return QString();
}

QString AiClient::buildPrompt(const QString &period) const
{
    const QDate today = QDate::currentDate();
    QString rangeLabel;
    QString stats;

    if (period == AiPeriod::daily()) {
        rangeLabel = QStringLiteral("每日");
        const int total = m_db->getTodayTotal();
        const int yesterday = m_db->getYesterdayTotal();
        const int goal = m_db->getSetting("daily_goal", "28800").toInt();

        stats += QStringLiteral("统计日期：%1\n").arg(today.toString("yyyy年M月d日"));
        stats += QStringLiteral("今日屏幕总时长：%1\n").arg(formatDuration(total));
        stats += QStringLiteral("昨日屏幕总时长：%1\n").arg(formatDuration(yesterday));
        stats += QStringLiteral("每日目标：%1\n").arg(formatDuration(goal));

        const auto rank = m_db->getAppRank(today);
        stats += QStringLiteral("今日应用使用排行（Top 5）：\n");
        if (rank.isEmpty())
            stats += QStringLiteral("（今日无应用记录）\n");
        else
            for (int i = 0; i < qMin(5, rank.size()); ++i)
                stats += QStringLiteral("%1. %2（%3）\n")
                             .arg(i + 1)
                             .arg(rank[i][QStringLiteral("app_name")].toString())
                             .arg(formatDuration(
                                 rank[i][QStringLiteral("total_seconds")].toInt()));

        QMap<QString, int> dayMap;
        const auto week = m_db->getWeekSummary();
        for (const auto &row : week)
            dayMap[row[QStringLiteral("d")].toString()] =
                row[QStringLiteral("total_seconds")].toInt();
        stats += QStringLiteral("本周每日时长：\n");
        const QDate monday = today.addDays(-today.dayOfWeek() + 1);
        for (int i = 0; i < 7; ++i) {
            const QDate d = monday.addDays(i);
            if (d > today)
                break;
            stats += QStringLiteral("%1（%2）：%3\n")
                         .arg(d.toString("M月d日"))
                         .arg(dayOfWeekCn(d.dayOfWeek()))
                         .arg(formatDuration(
                             dayMap.value(d.toString(Qt::ISODate), 0)));
        }
    } else {
        // 主页「每周」报告：覆盖本周周一至今；聚合逻辑由 buildPromptForRange 提供。
        const QDate monday = today.addDays(-today.dayOfWeek() + 1);
        return buildPromptForRange(QStringLiteral("每周"), monday, today);
    }

    // 每日报告：daily 分支在上方组装 stats，统一在此拼装 prompt。
    return QStringLiteral(
               "你是 Time Master 屏幕时间管理助手。请基于以下统计数据，用简体中文"
               "生成一份%1屏幕使用报告。\n\n"
               "统计数据：\n%2\n"
               "输出要求：\n"
               "1. 使用 Markdown 格式，包含「## 概览」「## 应用分析」「## 建议」三个小节；\n"
               "2. 报告总字数控制在 300-500 字；\n"
               "3. 基于数据给出 2-3 条具体、可执行的建议（如时间分配、休息提醒、"
               "减少某应用的使用时间等）；\n"
               "4. 语气客观友好，直接输出报告正文，不要额外解释。")
        .arg(rangeLabel, stats);
}

QString AiClient::buildPromptForRange(const QString &rangeLabel,
                                      const QDate &start, const QDate &end) const
{
    QString stats;
    stats += QStringLiteral("统计区间：%1 至 %2\n")
                 .arg(start.toString("M月d日"), end.toString("M月d日"));

    const auto rows = m_db->getDailySummaries(start.toString(Qt::ISODate),
                                              end.toString(Qt::ISODate));
    QMap<QString, int> dailyTotals; // 日(yyyy-MM-dd) -> 总秒数
    QMap<QString, int> appTotals;   // 应用名 -> 总秒数
    int weekTotal = 0;
    for (const auto &row : rows) {
        const int secs = row[QStringLiteral("total_seconds")].toInt();
        dailyTotals[row[QStringLiteral("d")].toString()] += secs;
        appTotals[row[QStringLiteral("app_name")].toString()] += secs;
        weekTotal += secs;
    }
    stats += QStringLiteral("%1屏幕总时长：%2\n")
                 .arg(rangeLabel, formatDuration(weekTotal));

    QVector<QPair<QString, int>> appList;
    appList.reserve(appTotals.size());
    for (auto it = appTotals.begin(); it != appTotals.end(); ++it)
        appList.append({it.key(), it.value()});
    std::sort(appList.begin(), appList.end(),
              [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
                  return a.second > b.second;
              });
    stats += QStringLiteral("%1应用使用排行（Top 5）：\n").arg(rangeLabel);
    if (appList.isEmpty())
        stats += QStringLiteral("（该区间无应用记录）\n");
    else
        for (int i = 0; i < qMin(5, appList.size()); ++i)
            stats += QStringLiteral("%1. %2（%3）\n")
                         .arg(i + 1)
                         .arg(appList[i].first)
                         .arg(formatDuration(appList[i].second));

    stats += QStringLiteral("%1每日时长：\n").arg(rangeLabel);
    const int days = qMin(7, start.daysTo(end) + 1);
    for (int i = 0; i < days; ++i) {
        const QDate d = start.addDays(i);
        stats += QStringLiteral("%1（%2）：%3\n")
                     .arg(d.toString("M月d日"))
                     .arg(dayOfWeekCn(d.dayOfWeek()))
                     .arg(formatDuration(
                         dailyTotals.value(d.toString(Qt::ISODate), 0)));
    }

    return QStringLiteral(
               "你是 Time Master 屏幕时间管理助手。请基于以下统计数据，用简体中文"
               "生成一份%1屏幕使用报告。\n\n"
               "统计数据：\n%2\n"
               "输出要求：\n"
               "1. 使用 Markdown 格式，包含「## 概览」「## 应用分析」「## 建议」三个小节；\n"
               "2. 报告总字数控制在 300-500 字；\n"
               "3. 基于数据给出 2-3 条具体、可执行的建议（如时间分配、休息提醒、"
               "减少某应用的使用时间等）；\n"
               "4. 语气客观友好，直接输出报告正文，不要额外解释。")
        .arg(rangeLabel, stats);
}

void AiClient::sendChat(const QString &period, const QString &prompt)
{
    QJsonObject sysMsg;
    sysMsg[QStringLiteral("role")] = QStringLiteral("system");
    sysMsg[QStringLiteral("content")] =
        QStringLiteral("你是屏幕时间管理助手，擅长根据使用时长数据生成简洁的中文分析报告。");

    QJsonObject userMsg;
    userMsg[QStringLiteral("role")] = QStringLiteral("user");
    userMsg[QStringLiteral("content")] = prompt;

    QJsonArray messages;
    messages.append(sysMsg);
    messages.append(userMsg);

    QJsonObject body;
    body[QStringLiteral("model")] = m_model;
    body[QStringLiteral("messages")] = messages;
    body[QStringLiteral("max_tokens")] = 1024;

    QString endpoint = m_endpoint.trimmed();
    while (endpoint.endsWith(QLatin1Char('/')))
        endpoint.chop(1);

    QNetworkRequest req(QUrl(endpoint + QStringLiteral("/chat/completions")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());
    req.setTransferTimeout(30000);

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, period]() {
        QString text;
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonObject obj =
                QJsonDocument::fromJson(reply->readAll()).object();
            const QJsonArray choices =
                obj[QStringLiteral("choices")].toArray();
            if (!choices.isEmpty()) {
                const QJsonObject msg =
                    choices.first().toObject()[QStringLiteral("message")].toObject();
                text = msg[QStringLiteral("content")].toString();
            }
        }

        if (!text.isEmpty()) {
            saveCache(period, text);
            emit reportReady(period, text);
        } else {
            QString err = reply->errorString();
            const QJsonObject obj =
                QJsonDocument::fromJson(reply->readAll()).object();
            const QJsonObject errObj =
                obj[QStringLiteral("error")].toObject();
            if (!errObj.isEmpty() &&
                errObj.contains(QStringLiteral("message")))
                err = errObj[QStringLiteral("message")].toString();
            qWarning() << "[AI] 报告生成失败:" << period << err;
            emit reportFailed(period, err);
        }
        reply->deleteLater();
    });
}

bool AiClient::generateReminderMessage(const QString &tag)
{
    if (!isConfigured())
        return false;
    if (m_db->getTodayTotal() <= 0)
        return false;

    const int total = m_db->getTodayTotal();
    const int goal = m_db->getSetting("daily_goal", "28800").toInt();
    const auto rank = m_db->getAppRank();
    QString topApp;
    if (!rank.isEmpty())
        topApp = rank.first()[QStringLiteral("app_name")].toString();

    QString data = QStringLiteral("今日屏幕使用总时长：%1。").arg(formatDuration(total));
    data += QStringLiteral("每日目标：%1。").arg(formatDuration(goal));
    if (!topApp.isEmpty())
        data += QStringLiteral("今日使用时间最多的应用：%1。").arg(topApp);

    const QString prompt = QStringLiteral(
        "你是屏幕时间管理助手。请根据以下今日屏幕使用数据，生成一句 30-60 字、"
        "简短友好的中文提醒，语气温和不评判。可提示起身休息、留意用眼、"
        "合理分配时间等，不要使用 Markdown 符号，不要转义。\n\n数据：%1")
                               .arg(data);
    sendReminderChat(tag, prompt);
    return true;
}

void AiClient::sendReminderChat(const QString &tag, const QString &prompt)
{
    QJsonObject sysMsg;
    sysMsg[QStringLiteral("role")] = QStringLiteral("system");
    sysMsg[QStringLiteral("content")] =
        QStringLiteral("你是屏幕时间管理助手，擅长用一句话给出简短友好的中文提醒。");

    QJsonObject userMsg;
    userMsg[QStringLiteral("role")] = QStringLiteral("user");
    userMsg[QStringLiteral("content")] = prompt;

    QJsonArray messages;
    messages.append(sysMsg);
    messages.append(userMsg);

    QJsonObject body;
    body[QStringLiteral("model")] = m_model;
    body[QStringLiteral("messages")] = messages;
    body[QStringLiteral("max_tokens")] = 200;

    QString endpoint = m_endpoint.trimmed();
    while (endpoint.endsWith(QLatin1Char('/')))
        endpoint.chop(1);

    QNetworkRequest req(QUrl(endpoint + QStringLiteral("/chat/completions")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());
    req.setTransferTimeout(30000);

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, tag]() {
        QString text;
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonObject obj =
                QJsonDocument::fromJson(reply->readAll()).object();
            const QJsonArray choices =
                obj[QStringLiteral("choices")].toArray();
            if (!choices.isEmpty()) {
                const QJsonObject msg =
                    choices.first().toObject()[QStringLiteral("message")].toObject();
                text = msg[QStringLiteral("content")].toString();
            }
        }

        if (!text.isEmpty()) {
            emit reminderReady(tag, text);
        } else {
            QString err = reply->errorString();
            const QJsonObject obj =
                QJsonDocument::fromJson(reply->readAll()).object();
            const QJsonObject errObj =
                obj[QStringLiteral("error")].toObject();
            if (!errObj.isEmpty() &&
                errObj.contains(QStringLiteral("message")))
                err = errObj[QStringLiteral("message")].toString();
            qWarning() << "[AI] 提醒文案生成失败:" << tag << err;
            emit reminderFailed(tag, err);
        }
        reply->deleteLater();
    });
}

bool AiClient::generateWeekReport(const QDate &monday, const QString &tag)
{
    if (!isConfigured())
        return false;
    const QDate sunday = monday.addDays(6);
    int total = 0;
    const auto rows = m_db->getDailySummaries(monday.toString(Qt::ISODate),
                                              sunday.toString(Qt::ISODate));
    for (const auto &row : rows)
        total += row[QStringLiteral("total_seconds")].toInt();
    if (total <= 0)
        return false;
    sendWeekChat(tag, buildPromptForRange(QStringLiteral("上周"), monday, sunday));
    return true;
}

void AiClient::sendWeekChat(const QString &tag, const QString &prompt)
{
    QJsonObject sysMsg;
    sysMsg[QStringLiteral("role")] = QStringLiteral("system");
    sysMsg[QStringLiteral("content")] =
        QStringLiteral("你是屏幕时间管理助手，擅长根据一周屏幕使用数据生成简洁的中文分析报告。");

    QJsonObject userMsg;
    userMsg[QStringLiteral("role")] = QStringLiteral("user");
    userMsg[QStringLiteral("content")] = prompt;

    QJsonArray messages;
    messages.append(sysMsg);
    messages.append(userMsg);

    QJsonObject body;
    body[QStringLiteral("model")] = m_model;
    body[QStringLiteral("messages")] = messages;
    body[QStringLiteral("max_tokens")] = 1024;

    QString endpoint = m_endpoint.trimmed();
    while (endpoint.endsWith(QLatin1Char('/')))
        endpoint.chop(1);

    QNetworkRequest req(QUrl(endpoint + QStringLiteral("/chat/completions")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());
    req.setTransferTimeout(30000);

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, tag]() {
        QString text;
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonObject obj =
                QJsonDocument::fromJson(reply->readAll()).object();
            const QJsonArray choices =
                obj[QStringLiteral("choices")].toArray();
            if (!choices.isEmpty()) {
                const QJsonObject msg =
                    choices.first().toObject()[QStringLiteral("message")].toObject();
                text = msg[QStringLiteral("content")].toString();
            }
        }

        if (!text.isEmpty()) {
            emit weekReportReady(tag, text);
        } else {
            QString err = reply->errorString();
            const QJsonObject obj =
                QJsonDocument::fromJson(reply->readAll()).object();
            const QJsonObject errObj =
                obj[QStringLiteral("error")].toObject();
            if (!errObj.isEmpty() &&
                errObj.contains(QStringLiteral("message")))
                err = errObj[QStringLiteral("message")].toString();
            qWarning() << "[AI] 周报分析失败:" << tag << err;
            emit weekReportFailed(tag, err);
        }
        reply->deleteLater();
    });
}

void AiClient::saveCache(const QString &period, const QString &text)
{
    if (period == AiPeriod::daily()) {
        m_db->setSetting("ai_report_daily_text", text);
        m_db->setSetting("ai_report_daily_date",
                         QDate::currentDate().toString(Qt::ISODate));
    } else if (period == AiPeriod::weekly()) {
        // 每周缓存锚定本周周一，跨周后由调用方判定失效。
        const QDate today = QDate::currentDate();
        const QDate monday = today.addDays(-today.dayOfWeek() + 1);
        m_db->setSetting("ai_report_weekly_text", text);
        m_db->setSetting("ai_report_weekly_date", monday.toString(Qt::ISODate));
    }
}

bool AiClient::hasDataForPeriod(const QString &period) const
{
    if (period == AiPeriod::daily())
        return m_db->getTodayTotal() > 0;
    if (period == AiPeriod::weekly()) {
        int total = 0;
        const auto week = m_db->getWeekSummary();
        for (const auto &row : week)
            total += row[QStringLiteral("total_seconds")].toInt();
        return total > 0;
    }
    return false;
}

QString AiClient::formatDuration(int seconds)
{
    if (seconds < 60)
        return QStringLiteral("%1秒").arg(seconds);
    const int totalMinutes = seconds / 60;
    const int hours = totalMinutes / 60;
    const int minutes = totalMinutes % 60;
    if (hours <= 0)
        return QStringLiteral("%1分钟").arg(minutes);
    if (minutes == 0)
        return QStringLiteral("%1小时").arg(hours);
    return QStringLiteral("%1小时%2分钟").arg(hours).arg(minutes);
}
