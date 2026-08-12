#ifndef AI_CLIENT_H
#define AI_CLIENT_H

#include <QDate>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class DatabaseManager;

/// AI 报告周期标识：daily = 每日报告，weekly = 每周报告。
namespace AiPeriod {
inline QString daily() { return QStringLiteral("daily"); }
inline QString weekly() { return QStringLiteral("weekly"); }
} // namespace AiPeriod

/// AI 智能模块网络层：调用 OpenAI 兼容 Chat Completions 接口生成使用报告，
/// 报告文本缓存到 settings 表，避免重复消耗 token。
class AiClient : public QObject
{
    Q_OBJECT
public:
    explicit AiClient(DatabaseManager *db, QObject *parent = nullptr);

    void reloadSettings();

    /// 已启用且端点、API Key 均已配置。
    bool isConfigured() const;

    /// 生成指定周期（AiPeriod::daily / AiPeriod::weekly）的 AI 使用报告。
    /// 未启用、未配置或该周期无数据时返回 false（不发起任何请求）。
    bool generateReport(const QString &period);

    /// 读取指定周期已缓存的报告文本；无缓存返回空串。
    QString cachedReport(const QString &period) const;

    /// 读取指定周期缓存对应的日期（yyyy-MM-dd）；无缓存返回空串。
    /// 调用方据此判断缓存是否已跨周期失效。
    QString cachedReportDate(const QString &period) const;

    /// 生成一条提醒短文案。tag 为调用方提供的标识（如 "2026-08-12|23:59"），
    /// 结果经 reminderReady/reminderFailed 按 tag 原样回传，用于区分并发请求归属。
    /// 未启用、未配置或今日无数据时返回 false（不发起任何请求）。
    bool generateReminderMessage(const QString &tag);

    /// 生成指定整周（monday 为该周周一）的周报分析文案，供周报 HTML 使用。
    /// 结果经 weekReportReady/weekReportFailed 按 tag 原样回传；不写缓存
    ///（周报 HTML 由调用方落盘，避免与主页 weekly 的 ai_report_weekly_* 缓存键冲突）。
    /// 未启用、未配置或该周无数据时返回 false（不发起任何请求）。
    bool generateWeekReport(const QDate &monday, const QString &tag);

signals:
    /// 报告生成成功（文本已写入缓存）。
    void reportReady(const QString &period, const QString &text);
    /// 报告生成失败（error 为可展示的错误描述）。
    void reportFailed(const QString &period, const QString &error);
    /// 提醒短文案生成成功（不写缓存）。
    void reminderReady(const QString &tag, const QString &text);
    /// 提醒短文案生成失败（调用方应回退本地模板）。
    void reminderFailed(const QString &tag, const QString &error);
    /// 指定周周报分析文案生成成功（不写缓存）。
    void weekReportReady(const QString &tag, const QString &text);
    /// 指定周周报分析文案生成失败（调用方回退本地小结）。
    void weekReportFailed(const QString &tag, const QString &error);

private:
    QString buildPrompt(const QString &period) const;
    /// 任意日期范围（如上一完整周）的统计 prompt，供主页 weekly 与周报复用。
    QString buildPromptForRange(const QString &rangeLabel,
                                const QDate &start, const QDate &end) const;
    void sendChat(const QString &period, const QString &prompt);
    void sendReminderChat(const QString &tag, const QString &prompt);
    void sendWeekChat(const QString &tag, const QString &prompt);
    void saveCache(const QString &period, const QString &text);
    bool hasDataForPeriod(const QString &period) const;
    static QString formatDuration(int seconds);
    /// 追加 AI 请求失败日志到 AppData 目录的 ai_error.log，便于排查反复失败。
    void appendErrorLog(const QString &period, const QString &error);

    DatabaseManager *m_db;
    QNetworkAccessManager *m_nam = nullptr;
    QString m_endpoint;
    QString m_apiKey;
    QString m_model;
    bool m_enabled = false;
};

#endif // AI_CLIENT_H
