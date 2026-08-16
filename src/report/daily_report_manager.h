#ifndef DAILY_REPORT_MANAGER_H
#define DAILY_REPORT_MANAGER_H

#include <QDate>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "report/report_html_builder.h"

class DatabaseManager;
class AiClient;

/// 每日报告统计聚合结果：由当日会话明细（getAllSessions）一次计算得出。
struct DayStats {
    int hourTotals[24] = {};       // 0-23 每小时秒数（会话按时段切分，跨午夜部分归次日不计）
    int periodSeconds[4] = {0};    // 凌晨0-6 / 上午6-12 / 下午12-18 / 晚上18-24
    QVector<ReportHtml::AppUsage> apps; // 按时长降序（同名变体已归一化合并）
    int total = 0;                 // 当日总秒数
    int sessionCount = 0;
    int longestSeconds = 0;        // 最长单次连续使用秒数
    QString longestApp;
    int activeHours = 0;           // 有使用记录的小时数
    int yesterdayTotal = 0;
    int dailyGoal = 28800;         // 每日目标秒数（daily_goal 设置）
    QVector<int> weekTotals;       // 本周一至当日每日秒数
    QStringList weekLabels;        // 与 weekTotals 等长的中文周几标签
};

/// 每日报告管理器：把「今日使用报告」生成为与周报同款的液态玻璃 HTML 网页
/// （Documents/TimeMaster/日报-yyyy-MM-dd.html，同日覆盖）。统计板块始终完整；
/// AI 分析区使用缓存文本（applyReportText 回填最新结果），无缓存时显示引导空态。
/// 无定时调度：主页卡片「↗」查看时现场生成最新统计，「⟳」刷新 AI 后自动更新。
class DailyReportManager : public QObject
{
    Q_OBJECT
public:
    explicit DailyReportManager(DatabaseManager *db, AiClient *ai,
                                QObject *parent = nullptr);

    /// 输出目录（默认 Documents/TimeMaster）；测试注入临时目录。
    void setOutputDir(const QString &dir);

    /// 生成/覆盖今日报告 HTML 并 emit dailyReportReady；返回文件路径，
    /// 写入失败返回空串。无数据日也生成空态页（查看操作不应失败）。
    QString refreshToday();

    /// 回填最新 AI 分析文本（锚点=今天）并重新生成页面。
    void applyReportText(const QString &text);

signals:
    /// 日报已生成（path 为 HTML 绝对路径）。
    void dailyReportReady(const QString &path);

private:
    DayStats collectDayStats(const QDate &date) const;
    QString buildHtml(const DayStats &stats) const;
    QString filePathFor(const QDate &date) const;

    DatabaseManager *m_db;
    AiClient *m_ai;
    QString m_outputDir;
    QString m_aiText;   // AI 分析 Markdown（缓存或最新回填）
    QDate m_aiAnchor;   // AI 文本锚点日期（标注缓存新鲜度）
};

#endif // DAILY_REPORT_MANAGER_H
