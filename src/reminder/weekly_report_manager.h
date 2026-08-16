#ifndef WEEKLY_REPORT_MANAGER_H
#define WEEKLY_REPORT_MANAGER_H

#include <QDate>
#include <QObject>
#include <QString>
#include <QTime>
#include <QTimer>
#include <QVector>

class DatabaseManager;
class AiClient;

/// 周报统计聚合结果：由一周会话明细（getAllSessions）一次计算得出，
/// 供 HTML 报告各板块与本地小结复用，避免重复查询。
struct WeekStats {
    int dailyTotals[7] = {0};      // 每日总秒数，索引 0=周一…6=周日（按会话开始日归属，与 SQL date(start_time) 一致）
    int prevDailyTotals[7] = {0};  // 前一周每日总秒数（对比折线用）
    int weekTotal = 0;
    int prevWeekTotal = 0;
    int activeDays = 0;            // 有使用记录的天数
    struct AppUsage {
        QString name;
        int seconds;
    };
    QVector<AppUsage> apps;        // 按时长降序（同名变体已按小写去空格键归一化合并）
    int hourMatrix[7][24] = {};    // 星期×小时热力矩阵（会话按时段切分，跨小时/跨午夜分摊到实际占用时段）
    int periodSeconds[4] = {0};    // 凌晨0-6 / 上午6-12 / 下午12-18 / 晚上18-24
    int sessionCount = 0;
    int longestSeconds = 0;        // 最长单次连续使用秒数
    QString longestApp;
    int longestDay = -1;           // 最长会话发生在周几（0=周一…6=周日）
};

/// 每周周报管理器：每周配置时刻自动生成上一完整周的 HTML 使用日报。
/// 统计部分由本地数据组装，AI 已配置且该周有数据时异步请求分析文案回填
/// 「AI 分析」区，AI 未配置或失败时回退本地小结，保证周报始终可生成。
/// 去重键为「上周一日期」，同周只生成一次并持久化，重启后不重复。
class WeeklyReportManager : public QObject
{
    Q_OBJECT
public:
    explicit WeeklyReportManager(DatabaseManager *db, AiClient *ai,
                                 QObject *parent = nullptr);

    void start();
    void stop();
    void reloadSettings();

    /// 30 秒轮询定时器是否已启动（start 后 true，stop 后 false）。
    bool isRunning() const;

    /// AI 周报分析是否在途（手动生成时用于「生成中」反馈）。
    bool isAiPending() const;

    /// 检查 now 时刻是否命中配置的「周几 + 时刻」并触发周报生成。
    /// now 为空时使用当前时间；公开以便测试注入时间（定时器间隔 30 秒，测试不便等待）。
    /// 配置日当天错过配置时刻后（如程序启动较晚）仍会补生成，幂等由去重键保证。
    void checkNow(const QTime &now = QTime());

    /// 手动生成上周周报：不受 weekly_report_enabled 开关限制（主动行为）。
    /// 同周已生成过且文件存在时直接打开已有文件（发 weeklyReportReady）；
    /// 文件已不在时清除去重键重新生成；无数据或写入失败时返回 false。
    bool generateNow();

    /// 强制重新生成上周周报：清除去重键后重新统计并覆盖已有文件
    /// （AI 已配置时重新请求分析文案）。上周无数据时返回 false。
    bool regenerateNow();

    /// 输出目录（默认 Documents/TimeMaster）；测试注入临时目录。
    void setOutputDir(const QString &dir);

signals:
    /// 周报已生成（path 为 HTML 绝对路径）。
    void weeklyReportReady(const QString &path);

private:
    bool generateWeekReport();
    /// 将分析区 HTML（AI 文案或本地小结）写入文件并更新去重/路径设置。
    void finishReport(const QDate &monday, const QString &aiHtml);
    int weekTotalFor(const QDate &start, const QDate &end) const;
    /// 聚合上周与前一周的会话明细，产出报告所需的全部统计。
    WeekStats collectWeekStats(const QDate &monday) const;
    QString buildHtml(const QString &aiAnalysis, const QDate &monday,
                      const QDate &sunday) const;
    QString buildLineChartSvg(const WeekStats &stats, const QDate &monday) const;
    QString buildHeatmapHtml(const WeekStats &stats) const;
    QString buildPeriodBarsHtml(const WeekStats &stats) const;
    QString buildAppRankHtml(const WeekStats &stats) const;
    QString buildInsightsHtml(const WeekStats &stats) const;
    QString buildLocalSummary(const QDate &monday, const QDate &sunday) const;
    QString markdownToHtml(const QString &markdown) const;
    QString filePathFor(const QDate &monday) const;

    DatabaseManager *m_db;
    AiClient *m_ai;
    QTimer *m_timer = nullptr;
    QString m_outputDir;
    bool m_enabled = false;
    int m_day = 1; // 1=周一 … 7=周日
    QString m_time = QStringLiteral("09:00");
    QString m_lastGenerated; // 已生成周报对应的「上周一」yyyy-MM-dd
    QDate m_pendingMonday;   // 在途 AI 请求对应的周一（结果回填时用于写文件）
};

#endif // WEEKLY_REPORT_MANAGER_H
