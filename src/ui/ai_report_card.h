#pragma once

#include "ui/card_frame.h"

#include <QString>

class AiClient;
class QLabel;
class QPaintEvent;
class QEnterEvent;
class SummaryCard;

/// 主页 AI 使用报告卡片：整个组件即为一张 Apple 风格圆角渐变卡
///（自绘渐变底 + 顶部光晕 + 圆角边框，无内外嵌套卡片），内含标题、
/// 状态提示与 SummaryCard（日期 + 8 字总结大字 + 操作按钮）。
/// 「↗」在浏览器打开今日报告网页（与周报一致），「⟳」生成/刷新 AI 分析，
/// 「▤」弹出上周周报菜单；AI 未配置时统计网页仍然完整可用。
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
    /// 用户点击「生成报告」（⟳ 刷新按钮）。
    void generateRequested();
    /// 用户点击「↗」：请求在浏览器打开今日报告网页。
    void dailyReportOpenRequested();
    /// 用户点击「上周周报」，携带 HTML 绝对路径（不存在时不发出）。
    void weeklyReportOpenRequested(const QString &path);
    /// 用户点击「立即生成上周周报」。
    void weeklyReportGenerateRequested();
    /// 用户点击「重新生成」，请求强制重新生成上周周报。
    void weeklyReportRegenerateRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void showWeeklyMenu();
    void refreshContent();
    void updateSummaryCard();
    /// 提取「【总结】」/「总结：」后的 8 字短语；超长按字符截断。
    QString summaryPhrase() const;
    /// 短语统一截断：超过 12 字截为前 12 字 + 省略号（大字卡片宽度恒可容纳）。
    QString capPhrase(const QString &phrase) const;
    /// 当前周期（今日）的展示文本，如「8月12日 · 星期三」。
    QString todayText() const;
    /// 概览小节的首句（旧缓存无【总结】行时的回退总结源）。
    QString overviewFirstSentence() const;

    AiClient *m_ai = nullptr;
    QString m_reportText; // 当前展示的原始 Markdown 报告
    QString m_error;      // 最近一次生成失败的错误描述
    bool m_loading = false;

    QLabel *m_hintLabel = nullptr;   // 报告状态提示（渐变卡内白字）
    SummaryCard *m_summaryCard = nullptr; // 渐变卡内容区（透明容器）
    int m_glossAlpha = 70;           // 顶部光晕透明度（hover 增亮）
    QString m_weeklyReportPath; // 最近生成的周报 HTML 绝对路径
};
