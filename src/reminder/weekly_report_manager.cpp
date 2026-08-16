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
#include <cmath>

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

// 折线图 y 轴刻度用的短格式："8小时" / "45分"（刻度值为整点计算结果）。
QString formatShortDuration(int seconds)
{
    if (seconds >= 3600)
        return QStringLiteral("%1小时").arg(seconds / 3600);
    return QStringLiteral("%1分").arg(qMax(1, seconds / 60));
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

QString wrapCard(const QString &title, const QString &body)
{
    return QStringLiteral("<section class=\"glass card\">\n<h2>%1</h2>\n%2\n</section>\n")
        .arg(title, body);
}

// 报告页样式：液态玻璃卡片 + 渐变模糊色斑背景 + 深浅色自适应（纯 CSS，离线可用）。
const char kReportStyle[] = R"CSS(:root{--text-strong:#122019;--text:#374151;--text-mute:#6B7280;
--accent:#087F6B;--accent-light:#35B99A;--chip:rgba(8,127,107,.10);--track:rgba(10,70,58,.08);
--grid-line:rgba(10,70,58,.10);--h0:#E9F0EC;--h1:#CDE9E0;--h2:#7FD4BE;--h3:#35B99A;--h4:#087F6B;
--p0:#7C8BFE;--p1:#66CDB4;--p2:#1F9E7F;--p3:#0B8A72;
--glass-bg:rgba(255,255,255,.60);--glass-border:rgba(255,255,255,.70);--glass-inner:rgba(255,255,255,.75)}
@media (prefers-color-scheme:dark){:root{--text-strong:#F1F6F3;--text:#CBD5D1;--text-mute:#94A3B8;
--accent:#4DD6B0;--accent-light:#72E2C2;--chip:rgba(77,214,176,.14);--track:rgba(255,255,255,.08);
--grid-line:rgba(255,255,255,.10);--h0:#2A3532;--h1:#1B4A3E;--h2:#0F6B56;--h3:#15977B;--h4:#4DD6B0;
--p0:#8B93F8;--p1:#7FD4BE;--p2:#35B99A;--p3:#15977B;
--glass-bg:rgba(24,32,30,.55);--glass-border:rgba(255,255,255,.12);--glass-inner:rgba(255,255,255,.08)}}
*{box-sizing:border-box}
body{margin:0;padding:44px 16px 36px;font-family:"Microsoft YaHei","Segoe UI",system-ui,sans-serif;
color:var(--text);background:linear-gradient(160deg,#E7F2EC 0%,#E6EEF3 48%,#EDEFF6 100%);
background-attachment:fixed;-webkit-font-smoothing:antialiased}
@media (prefers-color-scheme:dark){body{background:linear-gradient(160deg,#0F1815 0%,#0F1518 48%,#131522 100%);
background-attachment:fixed}}
.blob{position:fixed;border-radius:50%;filter:blur(90px);z-index:-1;pointer-events:none;
animation:drift 28s ease-in-out infinite alternate}
.blob-a{width:560px;height:560px;left:-170px;top:-130px;background:radial-gradient(circle,rgba(8,127,107,.34),transparent 66%)}
.blob-b{width:520px;height:520px;right:-150px;top:30%;background:radial-gradient(circle,rgba(53,185,154,.28),transparent 66%);
animation-delay:-9s;animation-duration:34s}
.blob-c{width:480px;height:480px;left:22%;bottom:-190px;background:radial-gradient(circle,rgba(124,139,254,.20),transparent 66%);
animation-delay:-18s;animation-duration:40s}
@keyframes drift{from{transform:translate3d(0,0,0) scale(1)}to{transform:translate3d(64px,42px,0) scale(1.12)}}
@media (prefers-reduced-motion:reduce){.blob{animation:none}}
.report{max-width:960px;margin:0 auto;display:flex;flex-direction:column;gap:20px}
.glass{background:var(--glass-bg);backdrop-filter:blur(24px) saturate(160%);
-webkit-backdrop-filter:blur(24px) saturate(160%);border:1px solid var(--glass-border);
border-radius:24px;box-shadow:0 10px 34px rgba(6,50,40,.10),inset 0 1px 0 var(--glass-inner)}
@media (prefers-color-scheme:dark){.glass{box-shadow:0 12px 38px rgba(0,0,0,.42),inset 0 1px 0 var(--glass-inner)}}
@media (prefers-reduced-motion:no-preference){.report>*{animation:rise .55s cubic-bezier(.2,.7,.3,1) both}
.report>*:nth-child(2){animation-delay:.06s}.report>*:nth-child(3){animation-delay:.12s}
.report>*:nth-child(4){animation-delay:.18s}.report>*:nth-child(5){animation-delay:.24s}
.report>*:nth-child(6){animation-delay:.30s}.report>*:nth-child(7){animation-delay:.36s}
.report>*:nth-child(8){animation-delay:.42s}}
@keyframes rise{from{opacity:0;transform:translateY(16px)}to{opacity:1;transform:none}}
.hero{display:flex;justify-content:space-between;align-items:flex-start;gap:16px;padding:30px 34px;flex-wrap:wrap}
.brand{font-size:12px;font-weight:700;letter-spacing:.22em;text-transform:uppercase;color:var(--accent);margin-bottom:10px}
.hero h1{margin:0;font-size:30px;letter-spacing:-.5px;color:var(--text-strong)}
.range{margin:10px 0 0;font-size:13.5px;color:var(--text-mute)}
.range b{color:var(--text);font-weight:600}
.hero-chip{font-size:12px;color:var(--text-mute);background:var(--chip);border-radius:999px;padding:7px 14px;white-space:nowrap}
.stat-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:16px}
.stat{padding:22px 24px}
.stat .label{font-size:12.5px;color:var(--text-mute)}
.stat .value{margin-top:8px;font-size:27px;font-weight:800;letter-spacing:-.5px;font-variant-numeric:tabular-nums;
background:linear-gradient(135deg,var(--accent-light),var(--accent));-webkit-background-clip:text;background-clip:text;
-webkit-text-fill-color:transparent;color:var(--accent)}
.stat .value.up{background:linear-gradient(135deg,#FBBF24,#F59E0B);-webkit-background-clip:text;background-clip:text;
-webkit-text-fill-color:transparent;color:#F59E0B}
.stat .value.down{background:linear-gradient(135deg,#34D399,#10B981);-webkit-background-clip:text;background-clip:text;
-webkit-text-fill-color:transparent;color:#10B981}
.stat .sub{margin-top:6px;font-size:12px;color:var(--text-mute)}
.card{padding:26px 30px}
.card h2{display:flex;align-items:center;gap:10px;margin:0 0 18px;font-size:16px;color:var(--text-strong)}
.card h2::before{content:"";width:4px;height:16px;border-radius:2px;background:linear-gradient(180deg,var(--accent-light),var(--accent))}
.chart-note{margin:14px 0 0;font-size:12px;color:var(--text-mute)}
.line-chart{width:100%;height:auto;display:block}
.heat-scroll{overflow-x:auto}
.heatmap{display:grid;grid-template-columns:52px repeat(24,minmax(15px,1fr));gap:3px;align-items:center;min-width:640px}
.hcell{aspect-ratio:1;border-radius:4px;background:var(--h0);min-width:15px}
.hcol{aspect-ratio:auto;height:16px;display:flex;align-items:center;font-size:10.5px;color:var(--text-mute)}
.hrow{aspect-ratio:auto;font-size:12px;color:var(--text-mute)}
.l1{background:var(--h1)}.l2{background:var(--h2)}.l3{background:var(--h3)}.l4{background:var(--h4)}
.heat-legend{display:flex;align-items:center;gap:6px;margin-top:14px;font-size:12px;color:var(--text-mute)}
.heat-legend i{width:12px;height:12px;border-radius:3px;display:inline-block}
.period-bar{display:flex;height:16px;border-radius:8px;overflow:hidden;background:var(--track)}
.period-legend{display:flex;gap:26px;flex-wrap:wrap;margin-top:16px}
.pl-item{display:flex;gap:9px;align-items:center}
.pl-dot{width:11px;height:11px;border-radius:4px}
.pl-item b{font-size:13px;color:var(--text-strong);font-weight:600}
.pl-item .d{display:block;font-size:12px;color:var(--text-mute)}
.app-row{margin-bottom:16px}
.app-row:last-child{margin-bottom:0}
.app-head{display:flex;align-items:center;gap:10px;margin-bottom:8px}
.rank{flex:none;width:22px;height:22px;border-radius:7px;display:inline-flex;align-items:center;justify-content:center;
font-size:12px;font-weight:700;color:var(--accent);background:var(--chip)}
.app-name{font-size:13.5px;color:var(--text-strong);font-weight:600;max-width:52%;overflow:hidden;
text-overflow:ellipsis;white-space:nowrap}
.app-meta{margin-left:auto;font-size:12.5px;color:var(--text-mute);font-variant-numeric:tabular-nums}
.app-track{height:10px;border-radius:5px;background:var(--track);overflow:hidden}
.app-fill{height:100%;border-radius:5px;background:linear-gradient(90deg,var(--accent-light),var(--accent))}
.insights{display:grid;grid-template-columns:repeat(3,1fr);gap:18px}
.ins .ivalue{font-size:23px;font-weight:800;color:var(--text-strong);font-variant-numeric:tabular-nums;letter-spacing:-.3px}
.ins .ilabel{margin-top:6px;font-size:12.5px;color:var(--text-mute)}
.ins .isub{margin-top:4px;font-size:12px;color:var(--text-mute);overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.ai{line-height:1.95;font-size:14.5px;color:var(--text)}
.ai h3{font-size:14px;color:var(--text-strong);margin:16px 0 6px}
.ai h3:first-child{margin-top:0}
.footer{text-align:center;color:var(--text-mute);font-size:12px;margin-top:4px}
@media (max-width:720px){.stat-grid{grid-template-columns:repeat(2,1fr)}.insights{grid-template-columns:1fr}
.hero h1{font-size:25px}})CSS";

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

    // 同周已生成过（去重键命中）→ 打开已有文件。
    const QDate today = QDate::currentDate();
    const QDate monday = today.addDays(-((today.dayOfWeek() - 1) % 7) - 7);
    if (m_lastGenerated == monday.toString(Qt::ISODate)) {
        const QString path = filePathFor(monday);
        if (QFile::exists(path)) {
            emit weeklyReportReady(path);
            return true;
        }
    }
    return false;
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
    emit weeklyReportReady(path);
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
        "<main class=\"report\">\n").arg(QString::fromLatin1(kReportStyle));

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

    html += wrapCard(QStringLiteral("每日使用趋势"),
                     buildLineChartSvg(stats, monday)
                         + QStringLiteral(
                             "<p class=\"chart-note\">实线为上周，虚线为前一周"
                             "（无对比数据时不显示）；悬停数据点可查看当日详情。</p>"));
    html += wrapCard(QStringLiteral("使用时段热力图"), buildHeatmapHtml(stats));
    html += wrapCard(QStringLiteral("时段分布"), buildPeriodBarsHtml(stats));
    html += wrapCard(QStringLiteral("应用排行 Top %1")
                         .arg(qMin(5, qMax(1, stats.apps.size()))),
                     buildAppRankHtml(stats));
    html += wrapCard(QStringLiteral("专注洞察"), buildInsightsHtml(stats));
    html += wrapCard(QStringLiteral("AI 智能分析"),
                     QStringLiteral("<div class=\"ai\">%1</div>").arg(aiHtml));

    html += QStringLiteral(
        "<footer class=\"footer\">由 Time Master 自动生成 · 生成时间 %1</footer>\n"
        "</main>\n</body>\n</html>\n").arg(generatedAt);
    return html;
}

QString WeeklyReportManager::buildLineChartSvg(const WeekStats &stats,
                                               const QDate &monday) const
{
    const double plotLeft = 64, plotRight = 704, plotTop = 40, plotBottom = 252;

    int maxSecs = 0;
    for (int i = 0; i < 7; ++i)
        maxSecs = qMax(maxSecs, qMax(stats.dailyTotals[i], stats.prevDailyTotals[i]));
    // y 轴刻度取整小时并留 12% 顶部余量，给数值标签留空间。
    const int niceMax = qMax<int>(3600,
                                  int(std::ceil(maxSecs * 1.12 / 3600.0)) * 3600);

    const auto xs = [&](int i) {
        return plotLeft + (plotRight - plotLeft) * i / 6.0;
    };
    const auto ys = [&](int secs) {
        return plotBottom - (plotBottom - plotTop) * qMin(secs, niceMax) / double(niceMax);
    };
    const auto f1 = [](double v) { return QString::number(v, 'f', 1); };

    QString gridAndLabels;
    for (int k = 0; k <= 4; ++k) {
        const double y = plotBottom - (plotBottom - plotTop) * k / 4.0;
        const int secs = qRound(niceMax / 4.0 * k);
        gridAndLabels += QStringLiteral(
            "<line x1=\"%1\" y1=\"%2\" x2=\"%3\" y2=\"%2\" stroke=\"var(--grid-line)\" "
            "stroke-width=\"1\"%4/>")
            .arg(f1(plotLeft), f1(y), f1(plotRight),
                 k == 0 ? QString() : QStringLiteral(" stroke-dasharray=\"3 4\""));
        if (k > 0)
            gridAndLabels += QStringLiteral(
                "<text x=\"%1\" y=\"%2\" text-anchor=\"end\" font-size=\"11\" "
                "fill=\"var(--text-mute)\">%3</text>")
                .arg(f1(plotLeft - 10), f1(y + 4), formatShortDuration(secs));
    }
    for (int i = 0; i < 7; ++i) {
        const QDate d = monday.addDays(i);
        gridAndLabels += QStringLiteral(
            "<text x=\"%1\" y=\"%2\" text-anchor=\"middle\" font-size=\"11\" "
            "fill=\"var(--text-strong)\">%3</text>"
            "<text x=\"%1\" y=\"%4\" text-anchor=\"middle\" font-size=\"10\" "
            "fill=\"var(--text-mute)\">%5</text>")
            .arg(f1(xs(i)), f1(plotBottom + 22), dayOfWeekCn(d.dayOfWeek()),
                 f1(plotBottom + 38), d.toString(QStringLiteral("M/d")));
    }

    // 前一周对比（灰虚线，仅在有数据时绘制）。
    QString prevLine;
    if (stats.prevWeekTotal > 0) {
        QStringList pts;
        for (int i = 0; i < 7; ++i)
            pts << QStringLiteral("%1,%2").arg(f1(xs(i)), f1(ys(stats.prevDailyTotals[i])));
        prevLine = QStringLiteral(
            "<polyline points=\"%1\" fill=\"none\" stroke=\"var(--text-mute)\" "
            "stroke-width=\"1.8\" stroke-dasharray=\"5 5\" stroke-linecap=\"round\"/>")
                       .arg(pts.join(QLatin1Char(' ')));
    }

    int peakDay = 0;
    for (int i = 1; i < 7; ++i)
        if (stats.dailyTotals[i] > stats.dailyTotals[peakDay])
            peakDay = i;

    // 渐变面积 + 主折线 + 数据点（含原生 title 悬停提示与数值标签）。
    QString areaPath = QStringLiteral("M%1,%2").arg(f1(xs(0)), f1(ys(stats.dailyTotals[0])));
    QString linePath;
    QString dots;
    for (int i = 0; i < 7; ++i) {
        const double x = xs(i);
        const double y = ys(stats.dailyTotals[i]);
        if (i > 0)
            areaPath += QStringLiteral(" L%1,%2").arg(f1(x), f1(y));
        linePath += (i == 0 ? QString() : QStringLiteral(" "))
            + QStringLiteral("%1,%2").arg(f1(x), f1(y));
        const double labelY = (y - 12 < 14) ? y + 20 : y - 12;
        dots += QStringLiteral(
            "<circle cx=\"%1\" cy=\"%2\" r=\"%3\" fill=\"var(--accent-light)\" "
            "stroke=\"var(--accent)\" stroke-width=\"2\"><title>%4</title></circle>"
            "<text x=\"%1\" y=\"%5\" text-anchor=\"middle\" font-size=\"11\" "
            "font-weight=\"600\" fill=\"var(--text-strong)\">%6</text>")
            .arg(f1(x), f1(y))
            .arg(i == peakDay ? 5 : 4)
            .arg(QStringLiteral("%1 %2 · %3")
                     .arg(monday.addDays(i).toString(QStringLiteral("M月d日")),
                          dayOfWeekCn(monday.addDays(i).dayOfWeek()),
                          formatDuration(stats.dailyTotals[i])))
            .arg(f1(labelY), formatDuration(stats.dailyTotals[i]));
    }
    areaPath += QStringLiteral(" L%1,%2 L%3,%2 Z")
                    .arg(f1(xs(6)), f1(plotBottom), f1(xs(0)));

    QString legend = QStringLiteral(
        "<line x1=\"536\" y1=\"16\" x2=\"562\" y2=\"16\" stroke=\"var(--accent)\" "
        "stroke-width=\"2.5\" stroke-linecap=\"round\"/>"
        "<text x=\"568\" y=\"20\" font-size=\"11\" fill=\"var(--text-mute)\">上周</text>");
    if (stats.prevWeekTotal > 0)
        legend += QStringLiteral(
            "<line x1=\"616\" y1=\"16\" x2=\"642\" y2=\"16\" stroke=\"var(--text-mute)\" "
            "stroke-width=\"1.8\" stroke-dasharray=\"5 5\"/>"
            "<text x=\"648\" y=\"20\" font-size=\"11\" fill=\"var(--text-mute)\">前一周</text>");

    return QStringLiteral(
        "<svg class=\"line-chart\" viewBox=\"0 0 720 300\" role=\"img\" "
        "aria-label=\"每日使用时长折线图\" xmlns=\"http://www.w3.org/2000/svg\">\n"
        "<defs><linearGradient id=\"tmArea\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\">"
        "<stop offset=\"0\" stop-color=\"#35B99A\" stop-opacity=\"0.34\"/>"
        "<stop offset=\"1\" stop-color=\"#35B99A\" stop-opacity=\"0\"/>"
        "</linearGradient></defs>\n"
        "%1\n%2\n%3\n"
        "<path d=\"%4\" fill=\"url(#tmArea)\"/>\n"
        "<polyline points=\"%5\" fill=\"none\" stroke=\"var(--accent)\" stroke-width=\"2.5\" "
        "stroke-linejoin=\"round\" stroke-linecap=\"round\"/>\n%6\n</svg>\n")
        .arg(gridAndLabels, prevLine, legend, areaPath, linePath, dots);
}

QString WeeklyReportManager::buildHeatmapHtml(const WeekStats &stats) const
{
    // 色阶分界取非零格子的 25/50/75 分位（与 TrendCard 热力图策略一致）。
    QVector<int> nonZero;
    for (int d = 0; d < 7; ++d)
        for (int h = 0; h < 24; ++h)
            if (stats.hourMatrix[d][h] > 0)
                nonZero.append(stats.hourMatrix[d][h]);
    std::sort(nonZero.begin(), nonZero.end());
    const auto quantile = [&nonZero](double p) {
        if (nonZero.isEmpty())
            return 0;
        return nonZero.value(qBound(0, int(nonZero.size() * p), nonZero.size() - 1));
    };
    const int q1 = quantile(0.25), q2 = quantile(0.50), q3 = quantile(0.75);

    QString cells = QStringLiteral("<div class=\"hcell hrow\"></div>");
    for (int h = 0; h < 24; ++h)
        cells += QStringLiteral("<div class=\"hcell hcol\">%1</div>")
                     .arg(QString::number(h).rightJustified(2, QLatin1Char('0')));

    for (int d = 0; d < 7; ++d) {
        cells += QStringLiteral("<div class=\"hcell hrow\">%1</div>")
                     .arg(dayOfWeekCn(d + 1));
        for (int h = 0; h < 24; ++h) {
            const int v = stats.hourMatrix[d][h];
            int level = 0;
            if (v > 0)
                level = v <= q1 ? 1 : (v <= q2 ? 2 : (v <= q3 ? 3 : 4));
            QString attr;
            if (v > 0)
                attr = QStringLiteral(" title=\"%1 %2:00 起 · %3\"")
                           .arg(dayOfWeekCn(d + 1),
                                QString::number(h).rightJustified(2, QLatin1Char('0')),
                                formatDuration(v));
            cells += QStringLiteral("<div class=\"hcell l%1\"%2></div>")
                         .arg(level)
                         .arg(attr);
        }
    }

    QString legend = QStringLiteral("<div class=\"heat-legend\"><span>少</span>");
    for (int l = 0; l <= 4; ++l)
        legend += QStringLiteral("<i style=\"background:var(--h%1)\"></i>").arg(l);
    legend += QStringLiteral("<span>多 · 悬停格子查看时长</span></div>");

    return QStringLiteral("<div class=\"heat-scroll\"><div class=\"heatmap\">%1</div></div>\n%2")
        .arg(cells, legend);
}

QString WeeklyReportManager::buildPeriodBarsHtml(const WeekStats &stats) const
{
    // 注意：中文必须经 QStringLiteral 走 UTF-16，不能用 QLatin1String/const char*
    // 直接转换（会按 Latin-1 解释字节导致乱码）。
    static const struct {
        const QString name;
        const char *var;
    } kPeriods[4] = {
        {QStringLiteral("凌晨 0-6 时"), "--p0"}, {QStringLiteral("上午 6-12 时"), "--p1"},
        {QStringLiteral("下午 12-18 时"), "--p2"}, {QStringLiteral("晚间 18-24 时"), "--p3"},
    };
    const int total = qMax(1, stats.weekTotal);

    QString bar;
    for (int i = 0; i < 4; ++i) {
        if (stats.periodSeconds[i] <= 0)
            continue;
        const double pct = stats.periodSeconds[i] * 100.0 / total;
        bar += QStringLiteral(
            "<div class=\"pseg\" style=\"width:%1%;background:var(%2)\" title=\"%3：%4\"></div>")
                   .arg(QString::number(pct, 'f', 1), QLatin1String(kPeriods[i].var),
                        kPeriods[i].name,
                        formatDuration(stats.periodSeconds[i]));
    }

    QString legend;
    for (int i = 0; i < 4; ++i) {
        const int pct = qRound(stats.periodSeconds[i] * 100.0 / total);
        legend += QStringLiteral(
            "<div class=\"pl-item\"><span class=\"pl-dot\" style=\"background:var(%1)\"></span>"
            "<div><b>%2</b><span class=\"d\">%3 · %4%</span></div></div>")
            .arg(QLatin1String(kPeriods[i].var), kPeriods[i].name,
                 formatDuration(stats.periodSeconds[i]))
            .arg(pct);
    }
    return QStringLiteral("<div class=\"period-bar\">%1</div>\n<div class=\"period-legend\">%2</div>")
        .arg(bar, legend);
}

QString WeeklyReportManager::buildAppRankHtml(const WeekStats &stats) const
{
    const int count = qMin(5, stats.apps.size());
    if (count <= 0)
        return QStringLiteral("<p class=\"chart-note\">上周无应用使用记录。</p>");

    const int total = qMax(1, stats.weekTotal);
    QString rows;
    for (int i = 0; i < count; ++i) {
        const auto &app = stats.apps[i];
        const int pct = qRound(app.seconds * 100.0 / total);
        rows += QStringLiteral(
            "<div class=\"app-row\">"
            "<div class=\"app-head\"><span class=\"rank\">%1</span>"
            "<span class=\"app-name\" title=\"%2\">%2</span>"
            "<span class=\"app-meta\">%3 · %4%</span></div>"
            "<div class=\"app-track\"><div class=\"app-fill\" style=\"width:%5%\"></div></div>"
            "</div>\n")
            .arg(i + 1)
            .arg(escapeHtml(app.name)) // 单参替换两处 %2（title 与显示名）
            .arg(formatDuration(app.seconds))
            .arg(pct)
            .arg(qMax(pct, 2)); // 条形最小可见宽度，避免小占比不可见
    }
    return rows;
}

QString WeeklyReportManager::buildInsightsHtml(const WeekStats &stats) const
{
    const int avg = stats.sessionCount > 0 ? stats.weekTotal / stats.sessionCount : 0;
    QString longestSub = QStringLiteral("—");
    if (stats.longestDay >= 0 && stats.longestSeconds > 0)
        longestSub = QStringLiteral("%1 · %2")
                         .arg(dayOfWeekCn(stats.longestDay + 1),
                              escapeHtml(stats.longestApp));
    return QStringLiteral(
        "<div class=\"insights\">"
        "<div class=\"ins\"><div class=\"ivalue\">%1</div>"
        "<div class=\"ilabel\">会话总数</div></div>"
        "<div class=\"ins\"><div class=\"ivalue\">%2</div>"
        "<div class=\"ilabel\">平均会话时长</div></div>"
        "<div class=\"ins\"><div class=\"ivalue\">%3</div>"
        "<div class=\"ilabel\">最长单次连续使用</div><div class=\"isub\">%4</div></div>"
        "</div>")
        .arg(stats.sessionCount)
        .arg(formatDuration(avg))
        .arg(stats.longestSeconds > 0 ? formatDuration(stats.longestSeconds)
                                      : QStringLiteral("—"))
        .arg(longestSub);
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
