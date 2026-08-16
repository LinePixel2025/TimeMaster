#include "report/daily_report_manager.h"
#include "ai/ai_client.h"
#include "database/database_manager.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QStandardPaths>

#include <algorithm>

using ReportHtml::dayOfWeekCn;
using ReportHtml::escapeHtml;
using ReportHtml::formatDuration;

DailyReportManager::DailyReportManager(DatabaseManager *db, AiClient *ai,
                                       QObject *parent)
    : QObject(parent), m_db(db), m_ai(ai)
{
    m_outputDir =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + QStringLiteral("/TimeMaster");

    // 启动时载入 AI 分析缓存（可能锚定在更早的日期，页面上会标注）。
    m_aiText = m_ai->cachedReport(AiPeriod::daily());
    m_aiAnchor = QDate::fromString(m_ai->cachedReportDate(AiPeriod::daily()),
                                   Qt::ISODate);
}

void DailyReportManager::setOutputDir(const QString &dir)
{
    m_outputDir = dir;
}

QString DailyReportManager::refreshToday()
{
    const QDate today = QDate::currentDate();
    const QString html = buildHtml(collectDayStats(today));
    const QString path = filePathFor(today);

    if (!QDir().mkpath(m_outputDir)) {
        qWarning() << "[DailyReport] 无法创建输出目录:" << m_outputDir;
        return QString();
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "[DailyReport] 无法写入日报:" << path;
        return QString();
    }
    file.write(html.toUtf8());
    file.close();
    emit dailyReportReady(path);
    return path;
}

void DailyReportManager::applyReportText(const QString &text)
{
    m_aiText = text;
    m_aiAnchor = QDate::currentDate();
    refreshToday();
}

DayStats DailyReportManager::collectDayStats(const QDate &date) const
{
    DayStats stats;

    // 应用名变体合并：与周报一致按小写去空格键聚合，展示名取组内最大变体。
    struct AppEntry {
        QString display;
        int total = 0;
        int maxRow = 0;
    };
    QMap<QString, AppEntry> appMap;

    const auto rows = m_db->getAllSessions(date.toString(Qt::ISODate),
                                            date.toString(Qt::ISODate));
    for (const auto &row : rows) {
        const int secs = row.value(QStringLiteral("duration_seconds")).toInt();
        if (secs <= 0)
            continue;
        const QDateTime start = QDateTime::fromString(
            row.value(QStringLiteral("start_time")).toString(), Qt::ISODate);
        if (!start.isValid() || start.date() != date)
            continue;

        stats.total += secs;
        ++stats.sessionCount;
        if (secs > stats.longestSeconds) {
            stats.longestSeconds = secs;
            stats.longestApp = row.value(QStringLiteral("app_name")).toString();
        }

        const QString appName = row.value(QStringLiteral("app_name")).toString();
        AppEntry &entry = appMap[appName.toLower().remove(QLatin1Char(' '))];
        entry.total += secs;
        if (secs > entry.maxRow) {
            entry.maxRow = secs;
            entry.display = appName;
        }

        // 时段切分：跨小时的会话按实际占用分摊；跨午夜溢出的部分不计入当日。
        qint64 remaining = secs;
        QDateTime cursor = start;
        while (remaining > 0) {
            const int intoHour = cursor.time().msecsSinceStartOfDay() % 3600000 / 1000;
            const qint64 chunk = qMin<qint64>(remaining, 3600 - intoHour);
            if (cursor.date() == date) {
                stats.hourTotals[cursor.time().hour()] += int(chunk);
                stats.periodSeconds[cursor.time().hour() / 6] += int(chunk);
            }
            remaining -= chunk;
            cursor = cursor.addSecs(int(chunk));
        }
    }

    for (int h = 0; h < 24; ++h)
        if (stats.hourTotals[h] > 0)
            ++stats.activeHours;

    for (auto it = appMap.begin(); it != appMap.end(); ++it)
        if (it.value().total > 0)
            stats.apps.append({it.value().display, it.value().total});
    std::sort(stats.apps.begin(), stats.apps.end(),
              [](const ReportHtml::AppUsage &a, const ReportHtml::AppUsage &b) {
                  return a.seconds > b.seconds;
              });

    // 昨日总量：按 date 的前一天查询（今日场景等价于 getYesterdayTotal）。
    const auto yesterdayRows = m_db->getDailySummaries(
        date.addDays(-1).toString(Qt::ISODate), date.addDays(-1).toString(Qt::ISODate));
    for (const auto &row : yesterdayRows)
        stats.yesterdayTotal += row[QStringLiteral("total_seconds")].toInt();

    stats.dailyGoal = m_db->getSetting("daily_goal", "28800").toInt();

    // 本周节奏：本周一至 date 的每日时长（getWeekSummary 以真实当前周为准，
    // 因此仅当 date 为今天时两者对齐；报告场景恒为今天）。
    for (const auto &row : m_db->getWeekSummary()) {
        const QDate d = QDate::fromString(row[QStringLiteral("d")].toString(),
                                          Qt::ISODate);
        if (!d.isValid() || d > date)
            continue;
        stats.weekTotals.append(row[QStringLiteral("total_seconds")].toInt());
        stats.weekLabels << dayOfWeekCn(d.dayOfWeek());
    }

    return stats;
}

QString DailyReportManager::buildHtml(const DayStats &stats) const
{
    const QDate today = QDate::currentDate();
    const QString generatedAt =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));

    QString html = QStringLiteral(
        "<!DOCTYPE html>\n<html lang=\"zh-CN\">\n<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "<title>Time Master 今日使用报告</title>\n"
        "<style>%1</style>\n</head>\n<body>\n"
        "<div class=\"blob blob-a\"></div><div class=\"blob blob-b\"></div>"
        "<div class=\"blob blob-c\"></div>\n"
        "<main class=\"report\">\n").arg(ReportHtml::style());

    html += QStringLiteral(
        "<header class=\"glass hero\">\n"
        "<div class=\"hero-main\">\n"
        "<div class=\"brand\">Time Master</div>\n"
        "<h1>今日使用报告</h1>\n"
        "<p class=\"range\"><b>%1</b> · %2</p>\n"
        "</div>\n"
        "<div class=\"hero-chip\">生成于 %3</div>\n"
        "</header>\n")
        .arg(today.toString(QStringLiteral("yyyy年M月d日")),
             dayOfWeekCn(today.dayOfWeek()), generatedAt);

    // 总览：今日总时长（对比昨日）、目标完成度、活跃小时、最长连续使用。
    QString changeValue = QStringLiteral("—");
    QString changeClass = QStringLiteral("");
    if (stats.yesterdayTotal > 0) {
        const int pct = static_cast<int>(
            (stats.total - stats.yesterdayTotal) * 100.0 / stats.yesterdayTotal);
        changeValue = (pct > 0 ? QStringLiteral("+%1%") : QStringLiteral("%1%")).arg(pct);
        if (pct > 0)
            changeClass = QStringLiteral("up");
        else if (pct < 0)
            changeClass = QStringLiteral("down");
    }

    QString goalValue = QStringLiteral("—");
    QString goalSub = QStringLiteral("未设置每日目标");
    if (stats.dailyGoal > 0) {
        const int pct = qRound(stats.total * 100.0 / stats.dailyGoal);
        goalValue = QStringLiteral("%1%").arg(pct);
        goalSub = QStringLiteral("目标 %1 · 已用 %2")
                      .arg(formatDuration(stats.dailyGoal), formatDuration(stats.total));
    }

    html += QStringLiteral(
        "<section class=\"stat-grid\">\n"
        "<div class=\"glass stat\"><div class=\"label\">今日总时长</div>"
        "<div class=\"value\">%1</div><div class=\"sub\">较昨日 <span class=\"%2\">%3</span>"
        " · 昨日 %4</div></div>\n"
        "<div class=\"glass stat\"><div class=\"label\">目标完成度</div>"
        "<div class=\"value\">%5</div><div class=\"sub\">%6</div></div>\n"
        "<div class=\"glass stat\"><div class=\"label\">活跃时段</div>"
        "<div class=\"value\">%7 小时</div><div class=\"sub\">共 %8 次使用</div></div>\n"
        "<div class=\"glass stat\"><div class=\"label\">最长连续使用</div>"
        "<div class=\"value\">%9</div><div class=\"sub\">%10</div></div>\n"
        "</section>\n")
        .arg(formatDuration(stats.total))
        .arg(changeClass, changeValue,
             stats.yesterdayTotal > 0 ? formatDuration(stats.yesterdayTotal)
                                      : QStringLiteral("无数据"))
        .arg(goalValue, goalSub)
        .arg(stats.activeHours)
        .arg(stats.sessionCount)
        .arg(stats.longestSeconds > 0 ? formatDuration(stats.longestSeconds)
                                      : QStringLiteral("—"))
        .arg(stats.longestSeconds > 0 ? escapeHtml(stats.longestApp)
                                      : QStringLiteral("暂无记录"));

    // 每小时使用折线：24 点，数值标签过密只靠 y 轴刻度与悬停提示。
    ReportHtml::LineChartOptions hourly;
    ReportHtml::LineSeries hourSeries;
    for (int h = 0; h < 24; ++h) {
        hourly.xLabels << QString::number(h);
        hourSeries.values << stats.hourTotals[h];
        hourSeries.titles << QStringLiteral("%1 · %2")
                                 .arg(QString::number(h).rightJustified(2, QLatin1Char('0'))
                                          + QStringLiteral(":00 起"),
                                      formatDuration(stats.hourTotals[h]));
    }
    hourly.series.append(hourSeries);
    hourly.valueLabels = false;
    hourly.ariaLabel = QStringLiteral("每小时使用时长折线图");
    html += ReportHtml::wrapCard(
        QStringLiteral("每小时使用"),
        ReportHtml::buildLineChart(hourly)
            + QStringLiteral("<p class=\"chart-note\">横轴为 0-23 时；悬停数据点可查看该小时时长。</p>"));

    // 本周节奏：本周一至今日，今日高亮。
    if (stats.weekTotals.size() >= 2) {
        ReportHtml::LineChartOptions week;
        ReportHtml::LineSeries weekSeries;
        const QDate monday = today.addDays(-today.dayOfWeek() + 1);
        for (int i = 0; i < stats.weekTotals.size(); ++i) {
            week.xLabels << stats.weekLabels.value(i);
            week.xSubLabels << monday.addDays(i).toString(QStringLiteral("M/d"));
            weekSeries.values << stats.weekTotals[i];
            weekSeries.titles << QStringLiteral("%1 · %2")
                                     .arg(monday.addDays(i).toString(QStringLiteral("M月d日")),
                                          formatDuration(stats.weekTotals[i]));
        }
        week.series.append(weekSeries);
        week.valueLabels = true;
        week.highlightIndex = stats.weekTotals.size() - 1; // 今日
        week.ariaLabel = QStringLiteral("本周每日使用时长折线图");
        html += ReportHtml::wrapCard(
            QStringLiteral("本周节奏"),
            ReportHtml::buildLineChart(week)
                + QStringLiteral("<p class=\"chart-note\">本周一至今日的每日使用时长，今日为高亮点。</p>"));
    }

    html += ReportHtml::wrapCard(QStringLiteral("时段分布"),
                                 ReportHtml::buildPeriodBars(stats.periodSeconds));
    html += ReportHtml::wrapCard(
        QStringLiteral("应用排行 Top %1").arg(qMin(5, qMax(1, stats.apps.size()))),
        ReportHtml::buildAppRank(stats.apps, stats.total));

    QVector<ReportHtml::InsightItem> insights;
    const int avg = stats.sessionCount > 0 ? stats.total / stats.sessionCount : 0;
    insights.append({QString::number(stats.sessionCount), QStringLiteral("会话总数"), QString()});
    insights.append({formatDuration(avg), QStringLiteral("平均会话时长"), QString()});
    insights.append({stats.longestSeconds > 0 ? formatDuration(stats.longestSeconds)
                                              : QStringLiteral("—"),
                     QStringLiteral("最长单次连续使用"),
                     stats.longestSeconds > 0 ? escapeHtml(stats.longestApp)
                                              : QStringLiteral("暂无记录")});
    html += ReportHtml::wrapCard(QStringLiteral("专注洞察"),
                                 ReportHtml::buildInsights(insights));

    // AI 分析区：缓存文本 + 锚点标注；无缓存时显示引导空态（统计板块仍然完整）。
    QString aiBody;
    if (!m_aiText.trimmed().isEmpty()) {
        aiBody = ReportHtml::markdownToHtml(m_aiText);
        if (m_aiAnchor.isValid())
            aiBody += QStringLiteral("<p class=\"chart-note\">AI 分析生成于 %1%2</p>")
                          .arg(m_aiAnchor.toString(QStringLiteral("M月d日")),
                               m_aiAnchor == today ? QString()
                                                   : QStringLiteral("，点击应用卡片 ⟳ 更新"));
    } else {
        aiBody = QStringLiteral(
            "<p class=\"chart-note\">今日 AI 分析尚未生成：点击应用卡片上的 ⟳ 生成；"
            "未配置 AI 时，本页统计板块仍然完整。</p>");
    }
    html += ReportHtml::wrapCard(QStringLiteral("AI 智能分析"),
                                 QStringLiteral("<div class=\"ai\">%1</div>").arg(aiBody));

    html += QStringLiteral(
        "<footer class=\"footer\">由 Time Master 自动生成 · 生成时间 %1</footer>\n"
        "</main>\n</body>\n</html>\n").arg(generatedAt);
    return html;
}

QString DailyReportManager::filePathFor(const QDate &date) const
{
    return m_outputDir + QStringLiteral("/日报-")
           + date.toString(QStringLiteral("yyyy-MM-dd"))
           + QStringLiteral(".html");
}
