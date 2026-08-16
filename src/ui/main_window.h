#pragma once

#include <QMainWindow>
#include <QTimer>

class DatabaseManager;
class QPushButton;
class QLabel;

class HeroCard;
class TrendCard;
class RankCard;
class CompareCard;
class AiReportCard;
class AiClient;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(DatabaseManager *db, AiClient *ai, QWidget *parent = nullptr);

public slots:
    void refreshData();
    /// AI 报告生成完成/失败后的刷新入口（由 AiClient 信号经 main 转发）。
    void onAiReportReady(const QString &period, const QString &text);
    void onAiReportFailed(const QString &period, const QString &error);
    /// 每周周报生成成功后回填主页按钮（由 main 转发）。
    void onWeeklyReportReady(const QString &path);

signals:
    void settingsChanged();
    /// 用户点击主界面「云端同步」按钮，请求立即执行云端同步。
    void cloudSyncRequested();
    /// 用户在 AI 报告卡片点击「生成报告」，携带当前周期（daily/weekly）。
    void aiReportRequested(const QString &period);
    /// 用户在 AI 报告卡片点击「上周周报」，携带 HTML 绝对路径。
    void weeklyReportOpenRequested(const QString &path);
    /// 用户在 AI 报告卡片点击「立即生成上周周报」，请求手动生成。
    void weeklyReportGenerateRequested();
    /// 用户在 AI 报告卡片点击「重新生成」，请求强制重新生成上周周报。
    void weeklyReportRegenerateRequested();
    /// 用户在 AI 报告卡片点击「↗」，请求生成并在浏览器打开今日报告网页。
    void dailyReportOpenRequested();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onExport();
    void onSettings();

private:
    void applyTheme();

    DatabaseManager *m_db = nullptr;
    QTimer *m_refreshTimer = nullptr;
    QPushButton *m_themeBtn = nullptr;
    QPushButton *m_settingsBtn = nullptr;
    QPushButton *m_exportBtn = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QPushButton *m_cloudSyncBtn = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_dateLabel = nullptr;
    HeroCard *m_heroCard = nullptr;
    TrendCard *m_trendCard = nullptr;
    RankCard *m_rankCard = nullptr;
    CompareCard *m_compareCard = nullptr;
    AiReportCard *m_aiCard = nullptr;
};
