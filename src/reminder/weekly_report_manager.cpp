#include "reminder/weekly_report_manager.h"
#include "ai/ai_client.h"
#include "database/database_manager.h"
#include "report/report_html_builder.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include <algorithm>

using ReportHtml::dayOfWeekCn;
using ReportHtml::escapeHtml;
using ReportHtml::formatDuration;

WeeklyReportManager::WeeklyReportManager(DatabaseManager *db, AiClient *ai,
                                         QObject *parent)
    : QObject(parent), m_db(db), m_ai(ai)
{
    m_outputDir =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + QStringLiteral("/TimeMaster");

    m_timer = new QTimer(this);
    m_timer->setInterval(30000);
    connect(m_timer, &QTimer::timeout, this, [this]() { checkNow(); });

    // AI 分析文案回填：tag 匹配在途请求才写文件（并发/过期请求丢弃）。
    connect(m_ai, &AiClient::weekReportReady, this,
            [this](const QString &tag, const QString &text, int promptTokens,
                   int completionTokens, int totalTokens) {
        if (!m_pendingMonday.isValid() ||
            tag != m_pendingMonday.toString(Qt::ISODate))
            return;
        const QDate monday = m_pendingMonday;
        m_pendingMonday = QDate();
        m_pendingAiUsage = AiClient::formatUsageText(promptTokens,
                                                     completionTokens,
                                                     totalTokens);
        finishReport(monday, ReportHtml::markdownToHtml(text));
    });
    connect(m_ai, &AiClient::weekReportFailed, this,
            [this](const QString &tag, const QString &) {
        if (!m_pendingMonday.isValid() ||
            tag != m_pendingMonday.toString(Qt::ISODate))
            return;
        const QDate monday = m_pendingMonday;
        m_pendingMonday = QDate();
        m_pendingAiUsage.clear(); // AI 失败回退本地小结，无 token 消耗可展示。
        // AI 失败 → 回退本地小结，周报不落空。
        finishReport(monday, ReportHtml::markdownToHtml(
                                  buildLocalSummary(monday, monday.addDays(6))));
    });
}

void WeeklyReportManager::start()
{
    reloadSettings();
    m_timer->start(); // 启动 30 秒轮询定时器；漏掉将导致只在启动瞬间检查一次。
    checkNow();
}

void WeeklyReportManager::stop()
{
    m_timer->stop();
}

bool WeeklyReportManager::isRunning() const
{
    return m_timer->isActive();
}

bool WeeklyReportManager::isAiPending() const
{
    return m_pendingMonday.isValid();
}

void WeeklyReportManager::reloadSettings()
{
    m_enabled = (m_db->getSetting("weekly_report_enabled", "false") == "true");
    m_day = qBound(1, m_db->getSetting("weekly_report_day", "1").toInt(), 7);
    const QString time = m_db->getSetting("weekly_report_time", "09:00");
    if (QTime::fromString(time, QStringLiteral("HH:mm")).isValid())
        m_time = time;
    m_lastGenerated = m_db->getSetting("weekly_report_last_generated", "");
}

void WeeklyReportManager::setOutputDir(const QString &dir)
{
    m_outputDir = dir;
}

void WeeklyReportManager::checkNow(const QTime &now)
{
    if (!m_enabled)
        return;

    const QTime current = now.isValid() ? now : QTime::currentTime();
    const QDate today = QDate::currentDate();
    if (today.dayOfWeek() != m_day)
        return;
    // 未到配置时刻不触发；已到或已过（含程序启动晚于时刻）则补生成，
    // 幂等由 generateWeekReport 的去重键保证，同周只生成一次。
    const QTime configured = QTime::fromString(m_time, QStringLiteral("HH:mm"));
    if (configured.isValid() && current < configured)
        return;
    generateWeekReport();
}

bool WeeklyReportManager::generateNow()
{
    if (generateWeekReport())
        return true;

    // 同周已生成过（去重键命中）→ 打开已有文件（未发起新的 AI 请求，无用量）。
    const QDate today = QDate::currentDate();
    const QDate monday = today.addDays(-((today.dayOfWeek() - 1) % 7) - 7);
    if (m_lastGenerated == monday.toString(Qt::ISODate)) {
        const QString path = filePathFor(monday);
        if (QFile::exists(path)) {
            emit weeklyReportReady(path, QString());
            return true;
        }
        // 去重键命中但文件已不在（被删除/移动）：清除去重键重新生成。
        // 数据仍在数据库中，手动请求应总能拿到报告，而不是误报「无数据」。
        m_lastGenerated.clear();
        m_db->setSetting("weekly_report_last_generated", QString());
        return generateWeekReport();
    }
    return false;
}

bool WeeklyReportManager::regenerateNow()
{
    const QDate today = QDate::currentDate();
    const QDate monday = today.addDays(-((today.dayOfWeek() - 1) % 7) - 7);
    if (weekTotalFor(monday, monday.addDays(6)) <= 0)
        return false; // 上周无使用记录。

    // 强制重新生成：清去除重键（内存 + 数据库），覆盖已有文件。
    m_lastGenerated.clear();
    m_db->setSetting("weekly_report_last_generated", QString());
    return generateWeekReport();
}

bool WeeklyReportManager::generateWeekReport()
{
    const QDate today = QDate::currentDate();
    // 上一完整周：上周一 = 今天倒退到本周周一后继续减 7 天。
    const QDate monday = today.addDays(-((today.dayOfWeek() - 1) % 7) - 7);
    const QDate sunday = monday.addDays(6);
    const QString key = monday.toString(Qt::ISODate);

    if (m_lastGenerated == key)
        return false; // 同周已生成过，不重复。
    if (weekTotalFor(monday, sunday) <= 0)
        return false; // 上周无使用记录，不生成空报告。

    // 同步去重键（含在途期间），防止同周后续命中重复生成。
    m_lastGenerated = key;

    if (m_ai->isConfigured() && m_ai->generateWeekReport(monday, key)) {
        m_pendingMonday = monday; // 结果经 weekReportReady/weekReportFailed 回填。
        return true;
    }

    finishReport(monday, ReportHtml::markdownToHtml(buildLocalSummary(monday, sunday)));
    return true;
}

int WeeklyReportManager::weekTotalFor(const QDate &start, const QDate &end) const
{
    int total = 0;
    const auto rows = m_db->getDailySummaries(start.toString(Qt::ISODate),
                                              end.toString(Qt::ISODate));
    for (const auto &row : rows)
        total += row[QStringLiteral("total_seconds")].toInt();
    return total;
}

WeekStats WeeklyReportManager::collectWeekStats(const QDate &monday) const
{
    WeekStats stats;
    const QDate prevMonday = monday.addDays(-7);

    // 应用名变体合并：与小写去空格键聚合，展示名取组内单行时长最大的变体
    // （与 DatabaseManager::mergeAppNameVariants 策略一致，getAllSessions 不做合并）。
    struct AppEntry {
        QString display;
        int total = 0;
        int maxRow = 0;
    };
    QMap<QString, AppEntry> appMap;

    const auto rows = m_db->getAllSessions(monday.toString(Qt::ISODate),
                                            monday.addDays(6).toString(Qt::ISODate));
    for (const auto &row : rows) {
        const int secs = row.value(QStringLiteral("duration_seconds")).toInt();
        if (secs <= 0)
            continue;
        const QDateTime start = QDateTime::fromString(
            row.value(QStringLiteral("start_time")).toString(), Qt::ISODate);
        if (!start.isValid())
            continue;
        const int dayIndex = monday.daysTo(start.date());
        if (dayIndex < 0 || dayIndex > 6)
            continue; // SQL 已按日期过滤，这里双保险。

        stats.dailyTotals[dayIndex] += secs;
        stats.weekTotal += secs;
        ++stats.sessionCount;
        if (secs > stats.longestSeconds) {
            stats.longestSeconds = secs;
            stats.longestApp = row.value(QStringLiteral("app_name")).toString();
            stats.longestDay = dayIndex;
        }

        const QString appName = row.value(QStringLiteral("app_name")).toString();
        AppEntry &entry = appMap[appName.toLower().remove(QLatin1Char(' '))];
        entry.total += secs;
        if (secs > entry.maxRow) {
            entry.maxRow = secs;
            entry.display = appName;
        }

        // 时段切分：跨小时/跨午夜的会话按实际占用分摊进热力矩阵，
        // 以 duration_seconds 为权威（chunk 恒 >= 1，循环必然终止）。
        qint64 remaining = secs;
        QDateTime cursor = start;
        while (remaining > 0) {
            const int intoHour = cursor.time().msecsSinceStartOfDay() % 3600000 / 1000;
            const qint64 chunk = qMin<qint64>(remaining, 3600 - intoHour);
            const int curDay = monday.daysTo(cursor.date());
            if (curDay >= 0 && curDay <= 6) {
                stats.hourMatrix[curDay][cursor.time().hour()] += int(chunk);
                stats.periodSeconds[cursor.time().hour() / 6] += int(chunk);
            }
            remaining -= chunk;
            cursor = cursor.addSecs(int(chunk));
        }
    }

    const auto prevRows = m_db->getAllSessions(prevMonday.toString(Qt::ISODate),
                                               monday.addDays(-1).toString(Qt::ISODate));
    for (const auto &row : prevRows) {
        const int secs = row.value(QStringLiteral("duration_seconds")).toInt();
        if (secs <= 0)
            continue;
        const QDateTime start = QDateTime::fromString(
            row.value(QStringLiteral("start_time")).toString(), Qt::ISODate);
        if (!start.isValid())
            continue;
        const int dayIndex = prevMonday.daysTo(start.date());
        if (dayIndex < 0 || dayIndex > 6)
            continue;
        stats.prevDailyTotals[dayIndex] += secs;
        stats.prevWeekTotal += secs;
    }

    for (auto it = appMap.begin(); it != appMap.end(); ++it)
        if (it.value().total > 0)
            stats.apps.append({it.value().display, it.value().total});
    std::sort(stats.apps.begin(), stats.apps.end(),
              [](const WeekStats::AppUsage &a, const WeekStats::AppUsage &b) {
                  return a.seconds > b.seconds;
              });

    for (int i = 0; i < 7; ++i)
        if (stats.dailyTotals[i] > 0)
            ++stats.activeDays;
    return stats;
}

void WeeklyReportManager::finishReport(const QDate &monday,
                                       const QString &aiHtml)
{
    const QDate sunday = monday.addDays(6);
    const QString html = buildHtml(aiHtml, monday, sunday);
    const QString path = filePathFor(monday);

    if (!QDir().mkpath(m_outputDir)) {
        qWarning() << "[WeeklyReport] 无法创建输出目录:" << m_outputDir;
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "[WeeklyReport] 无法写入周报:" << path;
        return;
    }
    file.write(html.toUtf8());
    file.close();

    m_db->setSetting("weekly_report_last_generated",
                     monday.toString(Qt::ISODate));
    m_db->setSetting("weekly_report_path", path);
    // 同步内存去重键，避免同周定时器后续命中时重复生成。
    m_lastGenerated = monday.toString(Qt::ISODate);
    emit weeklyReportReady(path, m_pendingAiUsage);
    m_pendingAiUsage.clear();
}

QString WeeklyReportManager::buildHtml(const QString &aiHtml,
                                       const QDate &monday,
                                       const QDate &sunday) const
{
    const WeekStats stats = collectWeekStats(monday);

    // 环比：屏幕时间下降是好事，用绿色；上升用琥珀色，不做负面评判。
    QString changeValue = QStringLiteral("—");
    QString changeClass = QStringLiteral("");
    if (stats.prevWeekTotal > 0) {
        const int pct = static_cast<int>(
            (stats.weekTotal - stats.prevWeekTotal) * 100.0 / stats.prevWeekTotal);
        changeValue = (pct > 0 ? QStringLiteral("+%1%") : QStringLiteral("%1%")).arg(pct);
        if (pct > 0)
            changeClass = QStringLiteral("up");
        else if (pct < 0)
            changeClass = QStringLiteral("down");
    }

    int peakDay = 0;
    for (int i = 1; i < 7; ++i)
        if (stats.dailyTotals[i] > stats.dailyTotals[peakDay])
            peakDay = i;

    const QString generatedAt =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));

    QString html = QStringLiteral(
        "<!DOCTYPE html>\n<html lang=\"zh-CN\">\n<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "<title>Time Master 周使用报告</title>\n"
        "<style>%1</style>\n</head>\n<body>\n"
        "<div class=\"blob blob-a\"></div><div class=\"blob blob-b\"></div>"
        "<div class=\"blob blob-c\"></div>\n"
        "<main class=\"report\">\n").arg(ReportHtml::style());

    html += QStringLiteral(
        "<header class=\"glass hero\">\n"
        "<div class=\"hero-main\">\n"
        "<div class=\"brand\">Time Master</div>\n"
        "<h1>周使用报告</h1>\n"
        "<p class=\"range\">统计区间 <b>%1 – %2</b> · 完整 7 天</p>\n"
        "</div>\n"
        "<div class=\"hero-chip\">生成于 %3</div>\n"
        "</header>\n")
        .arg(monday.toString(QStringLiteral("yyyy年M月d日")),
             sunday.toString(QStringLiteral("yyyy年M月d日")),
             generatedAt);

    html += QStringLiteral(
        "<section class=\"stat-grid\">\n"
        "<div class=\"glass stat\"><div class=\"label\">上周总时长</div>"
        "<div class=\"value\">%1</div><div class=\"sub\">共 %2 次使用记录</div></div>\n"
        "<div class=\"glass stat\"><div class=\"label\">日均使用</div>"
        "<div class=\"value\">%3</div><div class=\"sub\">按 7 天平均</div></div>\n"
        "<div class=\"glass stat\"><div class=\"label\">活跃天数</div>"
        "<div class=\"value\">%4 天</div><div class=\"sub\">峰值 %5 · %6</div></div>\n"
        "<div class=\"glass stat\"><div class=\"label\">环比变化</div>"
        "<div class=\"value %7\">%8</div><div class=\"sub\">前一周 %9</div></div>\n"
        "</section>\n")
        .arg(formatDuration(stats.weekTotal))
        .arg(stats.sessionCount)
        .arg(formatDuration(stats.weekTotal / 7))
        .arg(stats.activeDays)
        .arg(dayOfWeekCn(peakDay + 1),
             formatDuration(stats.dailyTotals[peakDay]))
        .arg(changeClass, changeValue,
             stats.prevWeekTotal > 0 ? formatDuration(stats.prevWeekTotal)
                                     : QStringLiteral("无数据"));

    // 每日趋势折线图：上周实线 + 前一周对比虚线。
    ReportHtml::LineChartOptions chart;
    for (int i = 0; i < 7; ++i) {
        const QDate d = monday.addDays(i);
        chart.xLabels << dayOfWeekCn(d.dayOfWeek());
        chart.xSubLabels << d.toString(QStringLiteral("M/d"));
    }
    ReportHtml::LineSeries main;
    for (int i = 0; i < 7; ++i) {
        const QDate d = monday.addDays(i);
        main.values << stats.dailyTotals[i];
        main.titles << QStringLiteral("%1 %2 · %3")
                          .arg(d.toString(QStringLiteral("M月d日")),
                               dayOfWeekCn(d.dayOfWeek()),
                               formatDuration(stats.dailyTotals[i]));
    }
    chart.series.append(main);
    chart.legendNames << QStringLiteral("上周");
    if (stats.prevWeekTotal > 0) {
        ReportHtml::LineSeries prev;
        for (int i = 0; i < 7; ++i)
            prev.values << stats.prevDailyTotals[i];
        chart.series.append(prev);
        chart.legendNames << QStringLiteral("前一周");
    }
    chart.valueLabels = true; // 7 点间距足够放数值标签
    chart.ariaLabel = QStringLiteral("每日使用时长折线图");
    html += ReportHtml::wrapCard(
        QStringLiteral("每日使用趋势"),
        ReportHtml::buildLineChart(chart)
            + QStringLiteral(
                "<p class=\"chart-note\">实线为上周，虚线为前一周"
                "（无对比数据时不显示）；悬停数据点可查看当日详情。</p>"));

    QVector<QVector<int>> matrix(7);
    QStringList rowLabels;
    for (int d = 0; d < 7; ++d) {
        matrix[d] = QVector<int>(stats.hourMatrix[d], stats.hourMatrix[d] + 24);
        rowLabels << dayOfWeekCn(d + 1);
    }
    html += ReportHtml::wrapCard(QStringLiteral("使用时段热力图"),
                                 ReportHtml::buildHeatmap(matrix, rowLabels));
    html += ReportHtml::wrapCard(QStringLiteral("时段分布"),
                                 ReportHtml::buildPeriodBars(stats.periodSeconds));

    QVector<ReportHtml::AppUsage> apps;
    apps.reserve(stats.apps.size());
    for (const auto &app : stats.apps)
        apps.append({app.name, app.seconds});
    html += ReportHtml::wrapCard(
        QStringLiteral("应用排行 Top %1").arg(qMin(5, qMax(1, apps.size()))),
        ReportHtml::buildAppRank(apps, stats.weekTotal));

    QVector<ReportHtml::InsightItem> insights;
    const int avg = stats.sessionCount > 0 ? stats.weekTotal / stats.sessionCount : 0;
    insights.append({QString::number(stats.sessionCount), QStringLiteral("会话总数"), QString()});
    insights.append({formatDuration(avg), QStringLiteral("平均会话时长"), QString()});
    insights.append({stats.longestSeconds > 0 ? formatDuration(stats.longestSeconds)
                                              : QStringLiteral("—"),
                     QStringLiteral("最长单次连续使用"),
                     stats.longestDay >= 0 && stats.longestSeconds > 0
                         ? QStringLiteral("%1 · %2")
                               .arg(dayOfWeekCn(stats.longestDay + 1),
                                    escapeHtml(stats.longestApp))
                         : QStringLiteral("—")});
    html += ReportHtml::wrapCard(QStringLiteral("专注洞察"),
                                 ReportHtml::buildInsights(insights));

    html += ReportHtml::wrapCard(
        QStringLiteral("AI 智能分析"),
        QStringLiteral("<div class=\"ai\">%1</div>").arg(aiHtml));

    html += QStringLiteral(
        "<footer class=\"footer\">由 Time Master 自动生成 · 生成时间 %1</footer>\n"
        "</main>\n</body>\n</html>\n").arg(generatedAt);
    return html;
}

QString WeeklyReportManager::buildLocalSummary(const QDate &monday,
                                               const QDate &sunday) const
{
    const WeekStats stats = collectWeekStats(monday);
    static const QString kPeriodNames[4] = {
        QStringLiteral("凌晨（0-6 时）"), QStringLiteral("上午（6-12 时）"),
        QStringLiteral("下午（12-18 时）"), QStringLiteral("晚间（18-24 时）")
    };

    QString summary = QStringLiteral("## 概览\n");
    summary += QStringLiteral("上周（%1 至 %2）总使用 %3，日均约 %4，7 天中 %5 天有使用记录。")
                   .arg(monday.toString(QStringLiteral("M月d日")),
                        sunday.toString(QStringLiteral("M月d日")),
                        formatDuration(stats.weekTotal),
                        formatDuration(stats.weekTotal / 7))
                   .arg(stats.activeDays);
    int peakDay = -1;
    for (int i = 0; i < 7; ++i)
        if (peakDay < 0 || stats.dailyTotals[i] > stats.dailyTotals[peakDay])
            peakDay = i;
    if (peakDay >= 0 && stats.dailyTotals[peakDay] > 0)
        summary += QStringLiteral("使用峰值出现在%1（%2）。")
                       .arg(dayOfWeekCn(peakDay + 1),
                            formatDuration(stats.dailyTotals[peakDay]));

    summary += QStringLiteral("\n## 应用与时段\n");
    if (!stats.apps.isEmpty()) {
        const int share = stats.apps.first().seconds * 100 / qMax(1, stats.weekTotal);
        summary += QStringLiteral("主力应用为 %1（%2，约占 %3%）。")
                       .arg(stats.apps.first().name,
                            formatDuration(stats.apps.first().seconds))
                       .arg(share);
    }
    if (stats.weekTotal > 0) {
        int peakPeriod = 0;
        for (int i = 1; i < 4; ++i)
            if (stats.periodSeconds[i] > stats.periodSeconds[peakPeriod])
                peakPeriod = i;
        const int pct = qRound(stats.periodSeconds[peakPeriod] * 100.0 / stats.weekTotal);
        summary += QStringLiteral("%1使用最多，约占 %2%。")
                       .arg(kPeriodNames[peakPeriod])
                       .arg(pct);
        if (stats.periodSeconds[0] > 0)
            summary += QStringLiteral("另有凌晨使用 %1，如非必要建议提前休息。")
                           .arg(formatDuration(stats.periodSeconds[0]));
    }
    if (stats.sessionCount > 0)
        summary += QStringLiteral("共记录 %1 次使用，平均每次 %2，最长连续使用 %3。")
                       .arg(stats.sessionCount)
                       .arg(formatDuration(stats.weekTotal / stats.sessionCount))
                       .arg(formatDuration(stats.longestSeconds));

    summary += QStringLiteral("\n## 环比与建议\n");
    if (stats.prevWeekTotal > 0) {
        const int pct = static_cast<int>(
            (stats.weekTotal - stats.prevWeekTotal) * 100.0 / stats.prevWeekTotal);
        if (pct > 0)
            summary += QStringLiteral("相较前一周增加 %1%（前一周 %2）。")
                           .arg(pct)
                           .arg(formatDuration(stats.prevWeekTotal));
        else if (pct < 0)
            summary += QStringLiteral("相较前一周减少 %1%（前一周 %2），节奏有所收敛。")
                           .arg(-pct)
                           .arg(formatDuration(stats.prevWeekTotal));
        else
            summary += QStringLiteral("与前一周（%1）基本持平。")
                           .arg(formatDuration(stats.prevWeekTotal));
    } else {
        summary += QStringLiteral("前一周无对比数据。");
    }
    summary += QStringLiteral("建议保持规律作息，连续使用约 1 小时后起身活动片刻。");
    return summary;
}

QString WeeklyReportManager::filePathFor(const QDate &monday) const
{
    return m_outputDir + QStringLiteral("/周报-")
           + monday.toString(QStringLiteral("yyyy-MM-dd"))
           + QStringLiteral(".html");
}
