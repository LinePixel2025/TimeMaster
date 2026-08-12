#pragma once

#include "ui/card_frame.h"

#include <QString>

class AiClient;
class QButtonGroup;
class QLabel;
class QPushButton;
class QScrollArea;
class QWidget;

/// 主页 AI 使用报告卡片：每日/每周周期切换、手动生成、缓存展示。
/// 报告正文由 AiClient 生成并缓存，本卡片只负责展示与交互。
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
    /// 报告生成完成（period 与当前周期一致时更新展示）。
    void setReport(const QString &period, const QString &text);
    /// 切换生成中状态（生成中显示占位文案、禁用按钮）。
    void setLoading(bool loading);
    /// 报告生成失败（period 与当前周期一致时展示错误）。
    void showError(const QString &period, const QString &error);

signals:
    /// 用户点击「生成报告」，携带当前选中的周期。
    void generateRequested(const QString &period);
    /// 用户点击「上周周报」，携带 HTML 绝对路径（不存在时不发出）。
    void weeklyReportOpenRequested(const QString &path);

private:
    void selectPeriod(const QString &period);
    void onGenerateClicked();
    void onWeeklyReportClicked();
    void refreshContent();
    void renderReport();
    /// 当前周期对应的缓存锚点日期：daily=今天，weekly=本周周一（yyyy-MM-dd）。
    QString currentAnchorDate() const;
    /// 当前周期的中文展示文本（如「2026年8月12日」「8月10日 – 8月12日」）。
    QString periodLabelText() const;
    QString markdownToHtml(const QString &markdown) const;
    QString toggleStyle(QPushButton *btn) const;

    AiClient *m_ai = nullptr;
    QString m_period = QStringLiteral("daily");
    QString m_reportText; // 当前展示的原始 Markdown 报告
    QString m_error;      // 当前周期最近一次生成失败的错误描述
    bool m_loading = false;

    QButtonGroup *m_periodGroup = nullptr;
    QPushButton *m_dailyBtn = nullptr;
    QPushButton *m_weeklyBtn = nullptr;
    QPushButton *m_generateBtn = nullptr;
    QPushButton *m_weeklyReportBtn = nullptr; // 打开上周周报 HTML
    QString m_weeklyReportPath;               // 最近生成的周报 HTML 绝对路径
    QLabel *m_hintLabel = nullptr; // 报告期间/缓存/未配置/错误提示
    QScrollArea *m_scrollArea = nullptr;
    QLabel *m_bodyLabel = nullptr; // 报告正文（富文本）
};
