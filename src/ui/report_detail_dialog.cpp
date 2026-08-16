#include "ui/report_detail_dialog.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"

#include <QApplication>
#include <QClipboard>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QTextDocument>
#include <QVBoxLayout>

namespace {

QString escapeHtml(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar ch : text) {
        switch (ch.unicode()) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        default:  out += ch;      break;
        }
    }
    return out;
}

/// 转义后还原 **加粗**（星号不受转义影响）。
QString inlineMarkup(const QString &text)
{
    QString out = escapeHtml(text);
    out.replace(QRegularExpression(QStringLiteral("\\*\\*(.+?)\\*\\*")),
                QStringLiteral("<b>\\1</b>"));
    return out;
}

} // namespace

ReportDetailDialog::ReportDetailDialog(const QString &title, const QString &markdown,
                                       const QString &weeklyReportPath,
                                       const QString &errorText,
                                       QWidget *parent)
    : QDialog(parent), m_markdown(markdown), m_errorText(errorText)
{
    setWindowTitle(QStringLiteral("完整报告"));
    setModal(true);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    resize(620, 660);
    setMinimumSize(520, 420);

    // 按可用工作区约束尺寸，避免小屏超界。
    const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
    resize(qMin(width(), avail.width() - 80), qMin(height(), avail.height() - 120));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(24, 22, 24, 22);
    outer->setSpacing(14);

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setFont(DesignTokens::appFont(17, QFont::DemiBold));
    outer->addWidget(m_titleLabel);

    // 报告操作行：生成报告 / 打开或生成上周周报（主界面卡片只留展开图标）。
    auto *actionRow = new QHBoxLayout();
    actionRow->setSpacing(10);
    actionRow->addStretch();

    m_generateBtn = new QPushButton(QStringLiteral("生成报告"), this);
    m_generateBtn->setCursor(Qt::PointingHandCursor);
    m_generateBtn->setMinimumHeight(30);
    connect(m_generateBtn, &QPushButton::clicked, this, [this]() {
        emit generateRequested();
        accept();
    });
    actionRow->addWidget(m_generateBtn);

    m_weeklyReportBtn = new QPushButton(QStringLiteral("上周周报"), this);
    m_weeklyReportBtn->setCursor(Qt::PointingHandCursor);
    m_weeklyReportBtn->setMinimumHeight(30);
    m_weeklyReportBtn->setEnabled(!weeklyReportPath.isEmpty());
    m_weeklyReportBtn->setToolTip(
        weeklyReportPath.isEmpty()
            ? QStringLiteral("周报尚未生成，点击「立即生成上周周报」")
            : QStringLiteral("打开上周周报 HTML"));
    connect(m_weeklyReportBtn, &QPushButton::clicked, this, [this, weeklyReportPath]() {
        emit weeklyReportOpenRequested(weeklyReportPath);
        accept();
    });
    actionRow->addWidget(m_weeklyReportBtn);

    m_weeklyReportGenerateBtn = new QPushButton(
        QStringLiteral("立即生成上周周报"), this);
    m_weeklyReportGenerateBtn->setCursor(Qt::PointingHandCursor);
    m_weeklyReportGenerateBtn->setMinimumHeight(30);
    connect(m_weeklyReportGenerateBtn, &QPushButton::clicked, this, [this]() {
        emit weeklyReportGenerateRequested();
        accept();
    });
    actionRow->addWidget(m_weeklyReportGenerateBtn);

    // 重新生成：已有周报时可强制重新统计并覆盖（如想要新版样式或重跑 AI 分析）。
    m_weeklyReportRegenerateBtn = new QPushButton(
        QStringLiteral("重新生成"), this);
    m_weeklyReportRegenerateBtn->setCursor(Qt::PointingHandCursor);
    m_weeklyReportRegenerateBtn->setMinimumHeight(30);
    m_weeklyReportRegenerateBtn->setEnabled(!weeklyReportPath.isEmpty());
    m_weeklyReportRegenerateBtn->setToolTip(
        weeklyReportPath.isEmpty()
            ? QStringLiteral("周报尚未生成，点击「立即生成上周周报」")
            : QStringLiteral("重新统计上周数据并覆盖当前周报（AI 分析将重新请求）"));
    connect(m_weeklyReportRegenerateBtn, &QPushButton::clicked, this, [this]() {
        emit weeklyReportRegenerateRequested();
        accept();
    });
    actionRow->addWidget(m_weeklyReportRegenerateBtn);

    outer->addLayout(actionRow);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    outer->addWidget(m_scrollArea, 1);

    auto *container = new QWidget();
    auto *bodyLayout = new QVBoxLayout(container);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    m_bodyLabel = new QLabel(container);
    m_bodyLabel->setTextFormat(Qt::RichText);
    m_bodyLabel->setWordWrap(true);
    m_bodyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bodyLayout->addWidget(m_bodyLabel);
    bodyLayout->addStretch();

    m_scrollArea->setWidget(container);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    btnRow->addStretch();

    m_copyBtn = new QPushButton(QStringLiteral("复制全文"), this);
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    m_copyBtn->setMinimumHeight(32);
    connect(m_copyBtn, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_markdown);
    });
    btnRow->addWidget(m_copyBtn);

    m_closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setMinimumHeight(32);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(m_closeBtn);

    outer->addLayout(btnRow);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { applyTheme(); });

    applyTheme();
}

void ReportDetailDialog::applyTheme()
{
    const QColor bg = DesignTokens::kSurface();
    QPalette pal = palette();
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::Base, bg);
    pal.setColor(QPalette::Text, DesignTokens::kText());
    pal.setColor(QPalette::WindowText, DesignTokens::kTextStrong());
    setPalette(pal);

    m_titleLabel->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(DesignTokens::kTextStrong().name()));
    m_bodyLabel->setStyleSheet(QString("background: transparent;"));

    m_scrollArea->setStyleSheet(
        QString("QScrollArea { background: transparent; border: none; }"
                "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
                "QScrollBar::handle:vertical { background: %1; min-height: 24px; border-radius: 4px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
            .arg(DesignTokens::kTextFaint().name()));

    const QString secondaryStyle = QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 6px; padding: 0 18px; font-size: 12px; }"
        "QPushButton:hover { background: %4; }"
        "QPushButton:disabled { color: %5; border-color: %5; }")
        .arg(DesignTokens::kSurface().name(), DesignTokens::kText().name(),
             DesignTokens::kBorder().name(), DesignTokens::kButtonHoverBg().name(),
             DesignTokens::kTextFaint().name());
    m_generateBtn->setStyleSheet(secondaryStyle);
    m_weeklyReportBtn->setStyleSheet(secondaryStyle);
    m_weeklyReportGenerateBtn->setStyleSheet(secondaryStyle);
    m_weeklyReportRegenerateBtn->setStyleSheet(secondaryStyle);
    m_copyBtn->setStyleSheet(secondaryStyle);

    const QString primaryStyle = QString(
        "QPushButton { background: %1; color: white; border: 1px solid %1;"
        " border-radius: 6px; padding: 0 18px; font-size: 12px; font-weight: 600; }"
        "QPushButton:hover { background: %2; }")
        .arg(DesignTokens::kAccent().name(), DesignTokens::kAccentHover().name());
    m_closeBtn->setStyleSheet(primaryStyle);

    rebuildContent();
}

void ReportDetailDialog::rebuildContent()
{
    m_bodyLabel->setText(markdownToHtml(m_markdown));
}

QString ReportDetailDialog::markdownToHtml(const QString &markdown) const
{
    // 无报告内容（今日尚未生成）：显示引导占位，操作按钮仍可用。
    if (markdown.trimmed().isEmpty()) {
        if (!m_errorText.isEmpty()) {
            // 生成失败：优先展示具体错误原因，便于排查「一直失败」。
            return QStringLiteral(
                "<div style='color:%1; font-size:13px; line-height:1.8;'>"
                "<b>生成失败：</b><br>%2<br><br>"
                "可点击上方「生成报告」重试，或检查网络与 AI 配置。</div>")
                .arg(DesignTokens::kError().name(), escapeHtml(m_errorText));
        }
        return QStringLiteral(
            "<div style='color:%1; font-size:13px; line-height:1.8;'>"
            "暂无报告内容，点击上方「生成报告」生成今日报告。</div>")
            .arg(DesignTokens::kTextMute().name());
    }

    const QString bodyColor = DesignTokens::kText().name();
    const QString cardBg = DesignTokens::kButtonHoverBg().name();
    const QString border = DesignTokens::kBorder().name();
    const QString accent = DesignTokens::kAccent().name();

    QString html;
    html.reserve(markdown.size() * 2 + 256);
    html += QStringLiteral("<div style='color:%1; font-size:13px; line-height:1.8;'>")
                .arg(bodyColor);

    // 按「## 小节」切分；每节渲染成圆角卡块（左侧 accent 色条 + 标题 + 内容）。
    const QStringList lines = markdown.split(QLatin1Char('\n'));
    QString curTitle;
    QStringList curBody;
    bool firstSection = true;

    auto flushSection = [&]() {
        if (curTitle.isEmpty())
            return;
        QString body;
        for (const QString &raw : curBody) {
            const QString line = raw.trimmed();
            if (line.isEmpty())
                continue;
            if (line.startsWith(QStringLiteral("- "))) {
                body += QStringLiteral("<div style='margin:2px 0 2px 14px;'>• %1</div>")
                            .arg(inlineMarkup(line.mid(2)));
            } else {
                body += QStringLiteral("<div>%1</div>").arg(inlineMarkup(line));
            }
        }
        if (body.isEmpty())
            return;

        html += QStringLiteral(
            "<div style='border:1px solid %1; border-left:3px solid %2;"
            " background:%3; border-radius:8px; padding:12px 14px; margin:8px 0;'>"
            "<div style='color:%2; font-size:14px; font-weight:600;"
            " margin-bottom:6px;'>%4</div>%5</div>")
                    .arg(border, accent, cardBg, escapeHtml(curTitle), body);
        firstSection = false;
    };

    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QStringLiteral("## "))) {
            flushSection();
            curTitle = line.mid(3).trimmed();
            curBody.clear();
        } else if (!line.isEmpty()) {
            curBody.append(rawLine);
        }
    }
    flushSection();

    // 没有识别到「## 小节」（AI 输出不合规）：整篇按段落渲染，保证可读。
    if (firstSection) {
        html += QStringLiteral(
            "<div style='border:1px solid %1; border-left:3px solid %2;"
            " background:%3; border-radius:8px; padding:12px 14px;'>")
                    .arg(border, accent, cardBg);
        for (const QString &rawLine : lines) {
            const QString line = rawLine.trimmed();
            if (line.isEmpty())
                continue;
            if (line.startsWith(QStringLiteral("- "))) {
                html += QStringLiteral("<div style='margin:2px 0 2px 14px;'>• %1</div>")
                            .arg(inlineMarkup(line.mid(2)));
            } else {
                html += QStringLiteral("<div>%1</div>").arg(inlineMarkup(line));
            }
        }
        html += QStringLiteral("</div>");
    }

    html += QStringLiteral("</div>");
    return html;
}
