#pragma once

#include <QDialog>

class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

/// AI 完整报告弹窗：按「## 概览」「## 应用分析」「## 建议」结构分节渲染，
/// 正文可滚动、可选择复制。报告相关操作（生成、周报打开/生成）也集中在此，
/// 主界面卡片只保留展开图标入口。
class ReportDetailDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ReportDetailDialog(const QString &title, const QString &markdown,
                                const QString &weeklyReportPath = QString(),
                                const QString &errorText = QString(),
                                QWidget *parent = nullptr);

signals:
    /// 点击「生成报告」（弹窗随即关闭，主界面进入生成中状态）。
    void generateRequested();
    /// 点击「上周周报」，携带 HTML 绝对路径（未生成时按钮禁用）。
    void weeklyReportOpenRequested(const QString &path);
    /// 点击「立即生成上周周报」。
    void weeklyReportGenerateRequested();

private:
    void applyTheme();
    void rebuildContent();
    QString markdownToHtml(const QString &markdown) const;

    QLabel *m_titleLabel = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QLabel *m_bodyLabel = nullptr;
    QPushButton *m_generateBtn = nullptr;
    QPushButton *m_weeklyReportBtn = nullptr;
    QPushButton *m_weeklyReportGenerateBtn = nullptr;
    QPushButton *m_copyBtn = nullptr;
    QPushButton *m_closeBtn = nullptr;
    QString m_markdown;
    QString m_errorText; // 生成失败时的错误描述（正文区优先展示）
};
