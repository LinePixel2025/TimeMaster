#include "report/report_html_builder.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace ReportHtml {
namespace {

// 报告页样式：液态玻璃卡片 + 渐变模糊色斑背景 + 深浅色自适应（纯 CSS，离线可用）。
const char kReportStyle[] = R"CSS(:root{--text-strong:#111827;--text:#374151;--text-mute:#6B7280;
--accent:#0B7A66;--accent-light:#35B99A;--chip:rgba(11,122,102,.10);--track:rgba(10,70,58,.08);
--grid-line:rgba(10,70,58,.10);--h0:#E4ECE9;--h1:#CDE9E0;--h2:#7FD4BE;--h3:#35B99A;--h4:#0B7A66;
--p0:#66CDB4;--p1:#35B99A;--p2:#15977B;--p3:#0B7A66;
--glass-bg:rgba(255,255,255,.60);--glass-border:rgba(255,255,255,.70);--glass-inner:rgba(255,255,255,.75)}
@media (prefers-color-scheme:dark){:root{--text-strong:#F1F5F3;--text:#C5D0CB;--text-mute:#8A9993;
--accent:#3DCFB0;--accent-light:#5FDBBF;--chip:rgba(61,207,176,.14);--track:rgba(255,255,255,.08);
--grid-line:rgba(255,255,255,.10);--h0:#24302C;--h1:#16352E;--h2:#0B7A66;--h3:#15977B;--h4:#3DCFB0;
--p0:#5FDBBF;--p1:#3DCFB0;--p2:#15977B;--p3:#0B7A66;
--glass-bg:rgba(16,22,20,.62);--glass-border:rgba(255,255,255,.12);--glass-inner:rgba(255,255,255,.08)}}
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

QString style()
{
    return QString::fromLatin1(kReportStyle);
}

QString wrapCard(const QString &title, const QString &body)
{
    return QStringLiteral("<section class=\"glass card\">\n<h2>%1</h2>\n%2\n</section>\n")
        .arg(title, body);
}

QString buildLineChart(const LineChartOptions &opt)
{
    const int n = opt.xLabels.size();
    if (n <= 0 || opt.series.isEmpty())
        return QString();

    const double plotLeft = 64, plotRight = 704, plotTop = 40, plotBottom = 252;

    int maxSecs = 0;
    for (const auto &s : opt.series)
        for (int v : s.values)
            maxSecs = qMax(maxSecs, v);
    if (maxSecs <= 0)
        maxSecs = 3600;
    // y 轴刻度取整小时并留 12% 顶部余量，给数值标签留空间。
    const int niceMax = qMax<int>(3600,
                                  int(std::ceil(maxSecs * 1.12 / 3600.0)) * 3600);

    const auto xs = [plotLeft, plotRight, n](int i) {
        return plotLeft + (plotRight - plotLeft) * i / double(n - 1);
    };
    const auto ys = [plotTop, plotBottom, niceMax](int secs) {
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
    for (int i = 0; i < n; ++i) {
        if (!opt.xLabels[i].isEmpty())
            gridAndLabels += QStringLiteral(
                "<text x=\"%1\" y=\"%2\" text-anchor=\"middle\" font-size=\"11\" "
                "fill=\"var(--text-strong)\">%3</text>")
                .arg(f1(xs(i)), f1(plotBottom + 22), escapeHtml(opt.xLabels[i]));
        const QString sub = i < opt.xSubLabels.size() ? opt.xSubLabels.at(i) : QString();
        if (!sub.isEmpty())
            gridAndLabels += QStringLiteral(
                "<text x=\"%1\" y=\"%2\" text-anchor=\"middle\" font-size=\"10\" "
                "fill=\"var(--text-mute)\">%3</text>")
                .arg(f1(xs(i)), f1(plotBottom + 38), escapeHtml(sub));
    }

    // 对比系列（灰虚线，无数据点）。
    QString compareLines;
    for (int s = 1; s < opt.series.size(); ++s) {
        const auto &series = opt.series[s];
        if (series.values.size() != n)
            continue;
        QStringList pts;
        for (int i = 0; i < n; ++i)
            pts << QStringLiteral("%1,%2").arg(f1(xs(i)), f1(ys(series.values[i])));
        compareLines += QStringLiteral(
            "<polyline points=\"%1\" fill=\"none\" stroke=\"var(--text-mute)\" "
            "stroke-width=\"1.8\" stroke-dasharray=\"5 5\" stroke-linecap=\"round\"/>")
                           .arg(pts.join(QLatin1Char(' ')));
    }

    const auto &main = opt.series.first();
    int highlight = opt.highlightIndex;
    if (highlight < 0 || highlight >= n) {
        highlight = 0;
        for (int i = 1; i < qMin(n, main.values.size()); ++i)
            if (main.values[i] > main.values[highlight])
                highlight = i;
    }

    QString areaPath = QStringLiteral("M%1,%2").arg(f1(xs(0)), f1(ys(main.values.at(0))));
    QString linePath;
    QString dots;
    for (int i = 0; i < n; ++i) {
        const double x = xs(i);
        const double y = ys(main.values.at(i));
        if (i > 0)
            areaPath += QStringLiteral(" L%1,%2").arg(f1(x), f1(y));
        linePath += (i == 0 ? QString() : QStringLiteral(" "))
            + QStringLiteral("%1,%2").arg(f1(x), f1(y));
        QString title = i < main.titles.size() ? main.titles.at(i) : QString();
        if (title.isEmpty())
            title = opt.xLabels.isEmpty() ? formatDuration(main.values.at(i))
                                          : QStringLiteral("%1 · %2")
                                                .arg(opt.xLabels[i],
                                                     formatDuration(main.values.at(i)));
        dots += QStringLiteral(
            "<circle cx=\"%1\" cy=\"%2\" r=\"%3\" fill=\"var(--accent-light)\" "
            "stroke=\"var(--accent)\" stroke-width=\"2\"><title>%4</title></circle>")
            .arg(f1(x), f1(y))
            .arg(i == highlight ? 5 : 4)
            .arg(escapeHtml(title));
        if (opt.valueLabels) {
            const double labelY = (y - 12 < 14) ? y + 20 : y - 12;
            dots += QStringLiteral(
                "<text x=\"%1\" y=\"%2\" text-anchor=\"middle\" font-size=\"11\" "
                "font-weight=\"600\" fill=\"var(--text-strong)\">%3</text>")
                .arg(f1(x), f1(labelY), formatDuration(main.values.at(i)));
        }
    }
    areaPath += QStringLiteral(" L%1,%2 L%3,%2 Z")
                    .arg(f1(xs(n - 1)), f1(plotBottom), f1(xs(0)));

    QString legend;
    if (!opt.legendNames.isEmpty()) {
        double lx = plotRight - 170;
        if (!opt.legendNames.value(0).isEmpty()) {
            legend += QStringLiteral(
                "<line x1=\"%1\" y1=\"16\" x2=\"%2\" y2=\"16\" stroke=\"var(--accent)\" "
                "stroke-width=\"2.5\" stroke-linecap=\"round\"/>"
                "<text x=\"%3\" y=\"20\" font-size=\"11\" fill=\"var(--text-mute)\">%4</text>")
                .arg(f1(lx), f1(lx + 26), f1(lx + 32), escapeHtml(opt.legendNames[0]));
            lx += 80;
        }
        if (opt.legendNames.size() > 1 && !opt.legendNames.value(1).isEmpty()) {
            legend += QStringLiteral(
                "<line x1=\"%1\" y1=\"16\" x2=\"%2\" y2=\"16\" stroke=\"var(--text-mute)\" "
                "stroke-width=\"1.8\" stroke-dasharray=\"5 5\"/>"
                "<text x=\"%3\" y=\"20\" font-size=\"11\" fill=\"var(--text-mute)\">%4</text>")
                .arg(f1(lx), f1(lx + 26), f1(lx + 32), escapeHtml(opt.legendNames[1]));
        }
    }

    return QStringLiteral(
        "<svg class=\"line-chart\" viewBox=\"0 0 720 300\" role=\"img\" "
        "aria-label=\"%1\" xmlns=\"http://www.w3.org/2000/svg\">\n"
        "<defs><linearGradient id=\"tmArea\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\">"
        "<stop offset=\"0\" stop-color=\"#35B99A\" stop-opacity=\"0.34\"/>"
        "<stop offset=\"1\" stop-color=\"#35B99A\" stop-opacity=\"0\"/>"
        "</linearGradient></defs>\n"
        "%2\n%3\n%4\n"
        "<path d=\"%5\" fill=\"url(#tmArea)\"/>\n"
        "<polyline points=\"%6\" fill=\"none\" stroke=\"var(--accent)\" stroke-width=\"2.5\" "
        "stroke-linejoin=\"round\" stroke-linecap=\"round\"/>\n%7\n</svg>\n")
        .arg(escapeHtml(opt.ariaLabel), gridAndLabels, compareLines, legend,
             areaPath, linePath, dots);
}

QString buildHeatmap(const QVector<QVector<int>> &matrix,
                     const QStringList &rowLabels)
{
    // 色阶分界取非零格子的 25/50/75 分位（与 TrendCard 热力图策略一致）。
    QVector<int> nonZero;
    for (const auto &row : matrix)
        for (int v : row)
            if (v > 0)
                nonZero.append(v);
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

    for (int d = 0; d < matrix.size(); ++d) {
        const QString rowLabel = d < rowLabels.size() ? rowLabels.at(d) : QString();
        cells += QStringLiteral("<div class=\"hcell hrow\">%1</div>")
                     .arg(escapeHtml(rowLabel));
        const auto &row = matrix[d];
        for (int h = 0; h < 24; ++h) {
            const int v = h < row.size() ? row.at(h) : 0;
            int level = 0;
            if (v > 0)
                level = v <= q1 ? 1 : (v <= q2 ? 2 : (v <= q3 ? 3 : 4));
            QString attr;
            if (v > 0)
                attr = QStringLiteral(" title=\"%1 %2:00 起 · %3\"")
                           .arg(escapeHtml(rowLabel),
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

QString buildPeriodBars(const int periodSeconds[4])
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
    int total = 0;
    for (int i = 0; i < 4; ++i)
        total += periodSeconds[i];
    total = qMax(1, total);

    QString bar;
    for (int i = 0; i < 4; ++i) {
        if (periodSeconds[i] <= 0)
            continue;
        const double pct = periodSeconds[i] * 100.0 / total;
        bar += QStringLiteral(
            "<div class=\"pseg\" style=\"width:%1%;background:var(%2)\" title=\"%3：%4\"></div>")
                   .arg(QString::number(pct, 'f', 1), QLatin1String(kPeriods[i].var),
                        kPeriods[i].name,
                        formatDuration(periodSeconds[i]));
    }

    QString legend;
    for (int i = 0; i < 4; ++i) {
        const int pct = qRound(periodSeconds[i] * 100.0 / total);
        legend += QStringLiteral(
            "<div class=\"pl-item\"><span class=\"pl-dot\" style=\"background:var(%1)\"></span>"
            "<div><b>%2</b><span class=\"d\">%3 · %4%</span></div></div>")
            .arg(QLatin1String(kPeriods[i].var), kPeriods[i].name,
                 formatDuration(periodSeconds[i]))
            .arg(pct);
    }
    return QStringLiteral("<div class=\"period-bar\">%1</div>\n<div class=\"period-legend\">%2</div>")
        .arg(bar, legend);
}

QString buildAppRank(const QVector<AppUsage> &apps, int totalSeconds)
{
    const int count = qMin(5, apps.size());
    if (count <= 0)
        return QStringLiteral("<p class=\"chart-note\">暂无应用使用记录。</p>");

    const int total = qMax(1, totalSeconds);
    QString rows;
    for (int i = 0; i < count; ++i) {
        const auto &app = apps[i];
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

QString buildInsights(const QVector<InsightItem> &items)
{
    if (items.isEmpty())
        return QString();
    const int columns = qBound(1, items.size(), 3);
    QString cells;
    for (const auto &item : items) {
        cells += QStringLiteral(
            "<div class=\"ins\"><div class=\"ivalue\">%1</div>"
            "<div class=\"ilabel\">%2</div>%3</div>")
            .arg(escapeHtml(item.value), escapeHtml(item.label),
                 item.sub.isEmpty()
                     ? QString()
                     : QStringLiteral("<div class=\"isub\">%1</div>").arg(escapeHtml(item.sub)));
    }
    return QStringLiteral("<div class=\"insights\" style=\"grid-template-columns:repeat(%1,1fr)\">%2</div>")
        .arg(columns)
        .arg(cells);
}

QString markdownToHtml(const QString &markdown)
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

QString formatShortDuration(int seconds)
{
    if (seconds >= 3600)
        return QStringLiteral("%1小时").arg(seconds / 3600);
    return QStringLiteral("%1分").arg(qMax(1, seconds / 60));
}

QString dayOfWeekCn(int dayOfWeek)
{
    static const QString kNames[] = {
        QStringLiteral("周一"), QStringLiteral("周二"), QStringLiteral("周三"),
        QStringLiteral("周四"), QStringLiteral("周五"), QStringLiteral("周六"),
        QStringLiteral("周日")
    };
    return (dayOfWeek >= 1 && dayOfWeek <= 7) ? kNames[dayOfWeek - 1] : QString();
}

} // namespace ReportHtml
