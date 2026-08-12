#ifndef WEEKLY_REPORT_MANAGER_H
#define WEEKLY_REPORT_MANAGER_H

#include <QDate>
#include <QObject>
#include <QString>
#include <QTime>
#include <QTimer>

class DatabaseManager;
class AiClient;

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

    /// 检查 now 时刻是否命中配置的「周几 + 时刻」并触发周报生成。
    /// now 为空时使用当前时间；公开以便测试注入时间（定时器间隔 30 秒，测试不便等待）。
    void checkNow(const QTime &now = QTime());

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
    QString buildHtml(const QString &aiAnalysis, const QDate &monday,
                      const QDate &sunday) const;
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
