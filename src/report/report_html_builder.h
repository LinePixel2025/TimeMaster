#pragma once

#include <QDate>
#include <QString>
#include <QStringList>
#include <QVector>

/// 报告网页共享渲染层：周报与日报共用的液态玻璃 HTML 片段构建器。
/// 纯静态函数集合，无 QObject；所有输出为纯 SVG/CSS（无 JS，离线可用）。
namespace ReportHtml {

struct AppUsage {
    QString name;
    int seconds;
};

struct InsightItem {
    QString value;
    QString label;
    QString sub; // 可空，显示在 label 下方
};

/// 折线图数据系列：首条为主系列（渐变面积 + 数据点 + 可选数值标签），
/// 其余为对比系列（灰虚线，无数据点）。
struct LineSeries {
    QVector<int> values;
    QStringList titles; // 每点悬停提示；为空时自动生成「xLabel · 时长」
};

struct LineChartOptions {
    QStringList xLabels;      // 主标签（如 周一~周日、0~23）；空串跳过绘制（用于抽稀）
    QStringList xSubLabels;   // 次标签（如 M/d）；可空
    QVector<LineSeries> series;
    QStringList legendNames;  // 图例：主系列名 + 可选对比系列名
    bool valueLabels = true;  // 每点上方显示数值（点距宽时用；24 点图应关闭）
    int highlightIndex = -1;  // 高亮点（-1 表示自动取主系列峰值）
    QString ariaLabel;
};

/// 报告页共享样式（液态玻璃卡片 + 渐变模糊色斑背景 + 深浅色自适应）。
QString style();

QString wrapCard(const QString &title, const QString &body);
QString buildLineChart(const LineChartOptions &opt);
/// 热力图：matrix 为 行×24 秒数矩阵，rowLabels 为行标签（如 周一~周日）。
QString buildHeatmap(const QVector<QVector<int>> &matrix,
                     const QStringList &rowLabels);
/// 时段分布分段条：periodSeconds 依次为 凌晨0-6 / 上午6-12 / 下午12-18 / 晚上18-24。
QString buildPeriodBars(const int periodSeconds[4]);
QString buildAppRank(const QVector<AppUsage> &apps, int totalSeconds);
QString buildInsights(const QVector<InsightItem> &items);

/// AI Markdown 文案的极简 HTML 转换（## 标题、**加粗**、- 列表、换行）。
QString markdownToHtml(const QString &markdown);

QString escapeHtml(const QString &text);
QString formatDuration(int seconds);   // "5小时32分" / "48分" / "30秒"
QString formatShortDuration(int seconds); // 折线图 y 轴刻度："8小时" / "45分"
QString dayOfWeekCn(int dayOfWeek);    // 1=周一 … 7=周日

} // namespace ReportHtml
