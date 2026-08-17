#pragma once

#include "ui/card_frame.h"

#include <QString>

class AiClient;
class QLabel;
class SummaryCard;

/// 主页 AI 使用报告卡片：横向洞察条，提供生成、日报与周报入口。
class AiReportCard : public CardFrame
{
    Q_OBJECT
public:
    explicit AiReportCard(AiClient *ai, QWidget *parent = nullptr);

    /// 按 AI 配置与报告缓存刷新界面状态（构造后与设置变更后调用）。
    void reloadState();
    /// 更新「上周周报」HTML 路径并刷新按钮可用状态（周报生成后由 main 转发）。
    void setWeeklyReportPath(const QString &path);

public slots:
    /// 报告生成完成，刷新大字总结。
    void setReport(const QString &text);
    /// 切换生成中状态（生成中显示占位文案、禁用按钮）。
    void setLoading(bool loading);
    /// 报告生成失败，展示错误。
    void showError(const QString &error);

signals:
    /// 用户点击「生成分析」。
    void generateRequested();
    /// 用户点击「今日报告」：请求在浏览器打开今日报告网页。
    void dailyReportOpenRequested();
    /// 用户点击「上周周报」，携带 HTML 绝对路径（不存在时不发出）。
    void weeklyReportOpenRequested(const QString &path);
    /// 用户点击「立即生成上周周报」。
    void weeklyReportGenerateRequested();
    /// 用户点击「重新生成」，请求强制重新生成上周周报。
    void weeklyReportRegenerateRequested();

private:
    void showWeeklyMenu();
    void refreshContent();
    void updateSummaryCard();
    /// 提取「【总结】」/「总结：」后的短语；超长按字符截断。
    QString summaryPhrase() const;
    /// 短语统一截断：超过 24 字截为前 24 字 + 省略号。
    QString capPhrase(const QString &phrase) const;
    /// 当前周期（今日）的展示文本，如「8月12日 · 星期三」。
    QString todayText() const;
    /// 概览小节的首句（旧缓存无【总结】行时的回退总结源）。
    QString overviewFirstSentence() const;

    AiClient *m_ai = nullptr;
    QString m_reportText;
    QString m_error;
    bool m_loading = false;

    SummaryCard *m_summaryCard = nullptr;
    QString m_weeklyReportPath;
};
