#include "reminder/weekly_report_manager.h"
#include "ai/ai_client.h"
#include "database/database_manager.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include <algorithm>

namespace {

QString dayOfWeekCn(int dayOfWeek)
{
    static const QString kNames[] = {
        QStringLiteral("周一"), QStringLiteral("周二"), QStringLiteral("周三"),
        QStringLiteral("周四"), QStringLiteral("周五"), QStringLiteral("周六"),
        QStringLiteral("周日")
    };
    return (dayOfWeek >= 1 && dayOfWeek <= 7) ? kNames[dayOfWeek - 1] : QString();
}

// "5小时32分" / "48分" / "30秒"，用于 HTML 统计展示。
QString formatDuration(int seconds)
{
    if (seconds < 60)
        return QStringLiteral("%1秒").arg(seconds);
    const int totalMinutes = seconds / 60;
    const int hours = totalMinutes / 60;
    const int minutes = totalMinutes % 60;
    if (hours <= 0)
        return QStringLiteral("%1分").arg(minutes);
    if (minutes == 0)
        return QStringLiteral("%1小时").arg(hours);
    return QStringLiteral("%1小时%2分").arg(hours).arg(minutes);
}

QString escapeHtml(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar ch : text) {
        switch (ch.unicode()) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default:  out += ch;       break;
        }
    }
    return out;
}

} // namespace

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
            [this](const QString &tag, const QString &text) {
        if (!m_pendingMonday.isValid() ||
            tag != m_pendingMonday.toString(Qt::ISODate))
            return;
        const QDate monday = m_pendingMonday;
        m_pendingMonday = QDate();
        finishReport(monday, markdownToHtml(text));
    });
    connect(m_ai, &AiClient::weekReportFailed, this,
            [this](const QString &tag, const QString &) {
        if (!m_pendingMonday.isValid() ||
            tag != m_pendingMonday.toString(Qt::ISODate))
            return;
        const QDate monday = m_pendingMonday;
        m_pendingMonday = QDate();
        // AI 失败 → 回退本地小结，周报不落空。
        finishReport(monday, markdownToHtml(buildLocalSummary(monday, monday.addDays(6))));
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
    if (current.toString(QStringLiteral("HH:mm")) != m_time)
        return;
    generateWeekReport();
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

    finishReport(monday, markdownToHtml(buildLocalSummary(monday, sunday)));
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
    emit weeklyReportReady(path);
}

QString WeeklyReportManager::buildLocalSummary(const QDate &monday,
                                               const QDate &sunday) const
{
    const int total = weekTotalFor(monday, sunday);
    const int days = monday.daysTo(sunday) + 1;

    QMap<QString, int> appTotals;
    const auto rows = m_db->getDailySummaries(monday.toString(Qt::ISODate),
                                              sunday.toString(Qt::ISODate));
    for (const auto &row : rows)
        appTotals[row[QStringLiteral("app_name")].toString()] +=
            row[QStringLiteral("total_seconds")].toInt();

    QString topApp;
    QString topDuration;
    for (auto it = appTotals.begin(); it != appTotals.end(); ++it) {
        if (topApp.isEmpty() || it.value() > appTotals[topApp]) {
            topApp = it.key();
            topDuration = formatDuration(it.value());
        }
    }

    QString summary = QStringLiteral(
        "上周总使用时长 %1，日均约 %2。").arg(formatDuration(total),
                                                 formatDuration(total / days));
    if (!topApp.isEmpty())
        summary += QStringLiteral("主力应用为 %1（%2）。").arg(topApp, topDuration);

    const int prevTotal = weekTotalFor(monday.addDays(-7), monday.addDays(-1));
    if (prevTotal > 0) {
        const int pct = static_cast<int>(
            (total - prevTotal) * 100.0 / prevTotal);
        if (pct > 0)
            summary += QStringLiteral("相较前一周增加 %1%。").arg(pct);
        else if (pct < 0)
            summary += QStringLiteral("相较前一周减少 %1%。").arg(-pct);
        else
            summary += QStringLiteral("与前一周基本持平。");
    }
    summary += QStringLiteral("建议留意长时间连续使用，适当起身休息。");
    return summary;
}

QString WeeklyReportManager::buildHtml(const QString &aiHtml,
                                       const QDate &monday,
                                       const QDate &sunday) const
{
    // 按日与应用聚合统计。
    QMap<QString, int> dailyTotals;
    QMap<QString, int> appTotals;
    int weekTotal = 0;
    const auto rows = m_db->getDailySummaries(monday.toString(Qt::ISODate),
                                              sunday.toString(Qt::ISODate));
    for (const auto &row : rows) {
        const int secs = row[QStringLiteral("total_seconds")].toInt();
        dailyTotals[row[QStringLiteral("d")].toString()] += secs;
        appTotals[row[QStringLiteral("app_name")].toString()] += secs;
        weekTotal += secs;
    }

    QVector<QPair<QString, int>> appList;
    appList.reserve(appTotals.size());
    for (auto it = appTotals.begin(); it != appTotals.end(); ++it)
        appList.append({it.key(), it.value()});
    std::sort(appList.begin(), appList.end(),
              [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
                  return a.second > b.second;
              });

    // 每日时长表。
    QString dailyRows;
    for (int i = 0; i < 7; ++i) {
        const QDate d = monday.addDays(i);
        const int secs = dailyTotals.value(d.toString(Qt::ISODate), 0);
        dailyRows += QStringLiteral(
            "<tr><td>%1</td><td>%2</td><td class=\"num\">%3</td></tr>")
                         .arg(d.toString(QStringLiteral("M月d日")),
                              dayOfWeekCn(d.dayOfWeek()), formatDuration(secs));
    }

    // 应用排行表（Top 5）。
    QString appRows;
    const int topCount = qMin(5, appList.size());
    for (int i = 0; i < topCount; ++i)
        appRows += QStringLiteral(
            "<tr><td>%1</td><td>%2</td><td class=\"num\">%3</td></tr>")
                       .arg(i + 1)
                       .arg(escapeHtml(appList[i].first),
                            formatDuration(appList[i].second));

    // 环比与前一周。
    const int prevTotal = weekTotalFor(monday.addDays(-7), monday.addDays(-1));
    QString changeText = QStringLiteral("—");
    if (prevTotal > 0) {
        const int pct = static_cast<int>((weekTotal - prevTotal) * 100.0 / prevTotal);
        changeText = (pct > 0 ? QStringLiteral("+%1%") : QStringLiteral("%1%"))
                         .arg(pct);
    }

    const QString generatedAt =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));

    return QStringLiteral(
        "<!DOCTYPE html>\n<html lang=\"zh-CN\">\n<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "<title>Time Master 周使用日报</title>\n"
        "<style>\n"
        "body { font-family: \"Microsoft YaHei\", \"Segoe UI\", sans-serif;"
        " background: #F3F6F5; color: #111827; margin: 0; padding: 32px 16px; }\n"
        ".card { max-width: 860px; margin: 0 auto 20px; background: #FFFFFF;"
        " border: 1px solid #DCE4E1; border-radius: 12px; padding: 28px 32px; }\n"
        "h1 { font-size: 22px; margin: 0 0 4px; }\n"
        ".meta { color: #6B7280; font-size: 13px; margin-bottom: 20px; }\n"
        "h2 { font-size: 16px; color: #087F6B; border-bottom: 1px solid #E2EAE7;"
        " padding-bottom: 6px; margin: 24px 0 12px; }\n"
        "table { width: 100%; border-collapse: collapse; font-size: 14px; }\n"
        "th, td { text-align: left; padding: 8px 10px; border-bottom: 1px solid #EEF2F1; }\n"
        "th { color: #6B7280; font-weight: 600; }\n"
        ".num { text-align: right; font-variant-numeric: tabular-nums; }\n"
        ".stat { display: flex; gap: 28px; flex-wrap: wrap; }\n"
        ".stat div { flex: 1; min-width: 140px; }\n"
        ".stat .value { font-size: 26px; font-weight: 700; color: #087F6B; }\n"
        ".stat .label { color: #6B7280; font-size: 12px; margin-top: 2px; }\n"
        ".ai { line-height: 1.8; font-size: 14px; }\n"
        ".ai h3 { font-size: 14px; color: #111827; margin: 14px 0 6px; }\n"
        ".footer { text-align: center; color: #9CA3AF; font-size: 12px; margin-top: 8px; }\n"
        "</style>\n</head>\n<body>\n"
        "<div class=\"card\">\n"
        "<h1>Time Master 周使用日报</h1>\n"
        "<p class=\"meta\">统计区间：%1 ~ %2</p>\n"
        "<h2>总览</h2>\n"
        "<div class=\"stat\">\n"
        "<div><div class=\"value\">%3</div><div class=\"label\">上周总时长</div></div>\n"
        "<div><div class=\"value\">%4</div><div class=\"label\">日均使用</div></div>\n"
        "<div><div class=\"value\">%5</div><div class=\"label\">前一周总时长</div></div>\n"
        "<div><div class=\"value\">%6</div><div class=\"label\">环比变化</div></div>\n"
        "</div>\n"
        "<h2>每日时长</h2>\n"
        "<table>\n<tr><th>日期</th><th>星期</th><th class=\"num\">时长</th></tr>\n%7\n</table>\n"
        "<h2>应用排行（Top 5）</h2>\n"
        "<table>\n<tr><th>排名</th><th>应用</th><th class=\"num\">时长</th></tr>\n%8\n</table>\n"
        "<h2>AI 分析</h2>\n"
        "<div class=\"ai\">%9</div>\n"
        "<p class=\"footer\">由 Time Master 自动生成 · 生成时间 %10</p>\n"
        "</div>\n</body>\n</html>\n")
        .arg(monday.toString(QStringLiteral("yyyy年M月d日")),
             sunday.toString(QStringLiteral("yyyy年M月d日")),
             formatDuration(weekTotal),
             formatDuration(weekTotal / 7),
             prevTotal > 0 ? formatDuration(prevTotal)
                           : QStringLiteral("—"),
             changeText,
             dailyRows,
             appRows,
             aiHtml,
             generatedAt);
}

QString WeeklyReportManager::markdownToHtml(const QString &markdown) const
{
    // 极简转换：## 标题、**加粗**、- 列表、换行；其余转义后按段落 <br> 拼接。
    QString html;
    const QStringList lines = markdown.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QStringLiteral("## "))) {
            html += QStringLiteral("<h3>%1</h3>")
                        .arg(escapeHtml(line.mid(3).trimmed()));
            continue;
        }
        QString content = escapeHtml(line);
        if (content.startsWith(QStringLiteral("- ")))
            content = QStringLiteral("• ") + content.mid(2);
        content.replace(QRegularExpression(QStringLiteral("\\*\\*(.+?)\\*\\*")),
                        QStringLiteral("<b>\\1</b>"));
        html += content + QStringLiteral("<br>");
    }
    return html;
}

QString WeeklyReportManager::filePathFor(const QDate &monday) const
{
    return m_outputDir + QStringLiteral("/周报-")
           + monday.toString(QStringLiteral("yyyy-MM-dd"))
           + QStringLiteral(".html");
}
