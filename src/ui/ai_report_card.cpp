#include "ui/ai_report_card.h"
#include "ai/ai_client.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"

#include <QButtonGroup>
#include <QDate>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

// 极简 Markdown → HTML：支持 ## 标题、**加粗**、- 列表项与普通段落。
// AI 报告的正文结构由 prompt 约束（## 概览 / ## 应用分析 / ## 建议），
// 无需引入完整渲染器；颜色内联注入以便主题切换时重绘。
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

} // namespace

AiReportCard::AiReportCard(AiClient *ai, QWidget *parent)
    : CardFrame(QStringLiteral("AI 使用报告"), parent)
    , m_ai(ai)
{
    // ---- 顶部工具栏：周期切换 + 生成按钮 ----
    auto *toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(8);

    m_dailyBtn = new QPushButton(QStringLiteral("每天"), this);
    m_weeklyBtn = new QPushButton(QStringLiteral("每周"), this);
    for (QPushButton *btn : {m_dailyBtn, m_weeklyBtn}) {
        btn->setCursor(Qt::PointingHandCursor);
        btn->setCheckable(true);
        btn->setFixedHeight(26);
        btn->setStyleSheet(toggleStyle(btn));
    }

    m_periodGroup = new QButtonGroup(this);
    m_periodGroup->addButton(m_dailyBtn, 0);
    m_periodGroup->addButton(m_weeklyBtn, 1);
    m_periodGroup->setExclusive(true);

    connect(m_periodGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, [this](int id) {
        selectPeriod(id == 1 ? AiPeriod::weekly() : AiPeriod::daily());
    });

    toolbar->addWidget(m_dailyBtn);
    toolbar->addWidget(m_weeklyBtn);
    toolbar->addStretch();

    m_generateBtn = new QPushButton(
        QStringLiteral("生成报告"), this);
    m_generateBtn->setCursor(Qt::PointingHandCursor);
    m_generateBtn->setMinimumHeight(30);
    connect(m_generateBtn, &QPushButton::clicked,
            this, &AiReportCard::onGenerateClicked);
    toolbar->addWidget(m_generateBtn);

    // 上周周报：打开自动生成的 HTML 日报（未生成时提示）。
    m_weeklyReportBtn = new QPushButton(
        QStringLiteral("上周周报"), this);
    m_weeklyReportBtn->setCursor(Qt::PointingHandCursor);
    m_weeklyReportBtn->setMinimumHeight(30);
    m_weeklyReportBtn->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: 1px solid %3;"
                " border-radius: 6px; padding: 0 14px; font-size: 12px; }"
                "QPushButton:hover { background: %4; }"
                "QPushButton:disabled { color: %5; }")
            .arg(DesignTokens::kSurface().name(), DesignTokens::kText().name(),
                 DesignTokens::kBorder().name(), DesignTokens::kButtonHoverBg().name(),
                 DesignTokens::kTextFaint().name()));
    connect(m_weeklyReportBtn, &QPushButton::clicked,
            this, &AiReportCard::onWeeklyReportClicked);
    toolbar->addWidget(m_weeklyReportBtn);

    contentLayout()->addLayout(toolbar);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setFont(DesignTokens::appFont(11));
    m_hintLabel->setTextFormat(Qt::PlainText);
    contentLayout()->addWidget(m_hintLabel);

    // ---- 正文滚动区 ----
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet(
        QString("QScrollArea { background: transparent; border: none; }"
                "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
                "QScrollBar::handle:vertical { background: %1; min-height: 24px; border-radius: 3px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
            .arg(DesignTokens::kTextFaint().name()));
    m_scrollArea->viewport()->setStyleSheet("background: transparent;");

    auto *bodyContainer = new QWidget();
    bodyContainer->setStyleSheet("background: transparent;");
    auto *bodyLayout = new QVBoxLayout(bodyContainer);
    bodyLayout->setContentsMargins(0, 2, 4, 0);

    m_bodyLabel = new QLabel(bodyContainer);
    m_bodyLabel->setTextFormat(Qt::RichText);
    m_bodyLabel->setWordWrap(true);
    m_bodyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_bodyLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    bodyLayout->addWidget(m_bodyLabel);
    bodyLayout->addStretch();

    m_scrollArea->setWidget(bodyContainer);
    contentLayout()->addWidget(m_scrollArea, 1);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) {
        m_dailyBtn->setStyleSheet(toggleStyle(m_dailyBtn));
        m_weeklyBtn->setStyleSheet(toggleStyle(m_weeklyBtn));
        m_weeklyReportBtn->setStyleSheet(
            QString("QPushButton { background: %1; color: %2; border: 1px solid %3;"
                    " border-radius: 6px; padding: 0 14px; font-size: 12px; }"
                    "QPushButton:hover { background: %4; }"
                    "QPushButton:disabled { color: %5; }")
                .arg(DesignTokens::kSurface().name(), DesignTokens::kText().name(),
                     DesignTokens::kBorder().name(), DesignTokens::kButtonHoverBg().name(),
                     DesignTokens::kTextFaint().name()));
        refreshContent();
    });

    selectPeriod(AiPeriod::daily());
}

void AiReportCard::setWeeklyReportPath(const QString &path)
{
    m_weeklyReportPath = path;
    m_weeklyReportBtn->setEnabled(!m_weeklyReportPath.isEmpty());
}

void AiReportCard::onWeeklyReportClicked()
{
    if (m_weeklyReportPath.isEmpty()) {
        // 未生成：提示将在每周配置时刻自动生成。
        m_hintLabel->setText(
            QStringLiteral("⏳ 周报尚未生成，将在每周设置的时刻自动生成，届时可在此打开"));
        m_hintLabel->setStyleSheet(
            QString("color: %1; background: transparent;")
                .arg(DesignTokens::kTextMute().name()));
        return;
    }
    emit weeklyReportOpenRequested(m_weeklyReportPath);
}

void AiReportCard::reloadState()
{
    // 设置变更后：重新读取该周期缓存并刷新展示。
    m_reportText = m_ai->cachedReport(m_period);
    refreshContent();
}

void AiReportCard::selectPeriod(const QString &period)
{
    m_period = period;
    m_dailyBtn->setChecked(period == AiPeriod::daily());
    m_weeklyBtn->setChecked(period == AiPeriod::weekly());
    m_dailyBtn->setStyleSheet(toggleStyle(m_dailyBtn));
    m_weeklyBtn->setStyleSheet(toggleStyle(m_weeklyBtn));

    // 切换周期后展示该周期的缓存（可能为空），并清掉上一周期的错误。
    // 重置 loading：在途请求返回时按周期匹配回填，不会影响新周期的展示。
    m_error.clear();
    m_loading = false;
    m_reportText = m_ai->cachedReport(period);
    refreshContent();
}

void AiReportCard::onGenerateClicked()
{
    m_loading = true;
    refreshContent();
    emit generateRequested(m_period);
}

void AiReportCard::setReport(const QString &period, const QString &text)
{
    if (period != m_period)
        return;
    m_reportText = text;
    m_error.clear();
    m_loading = false;
    refreshContent();
}

void AiReportCard::setLoading(bool loading)
{
    m_loading = loading;
    refreshContent();
}

void AiReportCard::showError(const QString &period, const QString &error)
{
    if (period != m_period)
        return;
    m_error = error;
    m_loading = false;
    refreshContent();
}

void AiReportCard::refreshContent()
{
    if (m_loading) {
        m_hintLabel->setText(QString());
        m_bodyLabel->setText(
            QStringLiteral("<div style='color:%1; font-size:13px;'>正在生成报告…</div>")
                .arg(DesignTokens::kTextMute().name()));
        m_generateBtn->setEnabled(false);
        return;
    }

    m_generateBtn->setEnabled(true);

    if (!m_error.isEmpty()) {
        m_hintLabel->setText(QStringLiteral("生成失败，可以重试"));
        m_hintLabel->setStyleSheet(
            QString("color: %1; background: transparent;")
                .arg(DesignTokens::kError().name()));
        m_bodyLabel->setText(
            QStringLiteral("<div style='color:%1; font-size:13px; line-height:1.7;'>%2</div>")
                .arg(DesignTokens::kError().name(), escapeHtml(m_error)));
        return;
    }

    if (!m_ai->isConfigured()) {
        m_reportText.clear();
        m_generateBtn->setEnabled(false); // 未配置时禁止点击，避免进入无法恢复的生成中状态
        m_hintLabel->setText(QStringLiteral("⚙ AI 智能尚未配置"));
        m_hintLabel->setStyleSheet(
            QString("color: %1; background: transparent;")
                .arg(DesignTokens::kTextMute().name()));
        m_bodyLabel->setText(
            QStringLiteral("<div style='color:%1; font-size:13px; line-height:1.7;'>"
                           "在「设置 → AI 智能」中启用并填写 API 地址与 API Key 后，"
                           "即可生成每日/每周用例分析报告。</div>")
                .arg(DesignTokens::kTextMute().name()));
        return;
    }

    // 有缓存：展示报告并标注缓存周期是否已过期。
    if (!m_reportText.isEmpty()) {
        const QString cachedDate = m_ai->cachedReportDate(m_period);
        const bool fresh = (cachedDate == currentAnchorDate());
        if (fresh) {
            m_hintLabel->setText(
                QStringLiteral("✔ 报告已更新（%1）").arg(periodLabelText()));
        } else {
            m_hintLabel->setText(
                QStringLiteral("⏳ 报告缓存于 %1，点击「生成报告」可更新")
                    .arg(cachedDate));
        }
        m_hintLabel->setStyleSheet(
            QString("color: %1; background: transparent;")
                .arg(DesignTokens::kTextMute().name()));
        renderReport();
        return;
    }

    // 无缓存：提示用户生成。
    m_hintLabel->setText(QString());
    m_bodyLabel->setText(
        QStringLiteral("<div style='color:%1; font-size:13px; line-height:1.7;'>"
                       "还没有本周期的报告。点击「生成报告」，AI 会根据%2的使用数据生成一份分析报告。</div>")
            .arg(DesignTokens::kTextMute().name(),
                 m_period == AiPeriod::daily()
                     ? QStringLiteral("今天")
                     : QStringLiteral("本周")));
}

void AiReportCard::renderReport()
{
    m_bodyLabel->setText(markdownToHtml(m_reportText));
}

QString AiReportCard::currentAnchorDate() const
{
    const QDate today = QDate::currentDate();
    if (m_period == AiPeriod::weekly())
        return today.addDays(-today.dayOfWeek() + 1).toString(Qt::ISODate);
    return today.toString(Qt::ISODate);
}

QString AiReportCard::periodLabelText() const
{
    const QDate today = QDate::currentDate();
    if (m_period == AiPeriod::weekly()) {
        const QDate monday = today.addDays(-today.dayOfWeek() + 1);
        return monday.toString(QStringLiteral("M月d日"))
            + QStringLiteral(" – ")
            + today.toString(QStringLiteral("M月d日"));
    }
    return today.toString(QStringLiteral("yyyy年M月d日"));
}

QString AiReportCard::markdownToHtml(const QString &markdown) const
{
    const QString bodyColor = DesignTokens::kText().name();
    const QString headingColor = DesignTokens::kTextStrong().name();

    QString html;
    html.reserve(markdown.size() * 2 + 128);
    html += QStringLiteral("<div style='color:%1; font-size:13px; line-height:1.7;'>")
                .arg(bodyColor);

    bool inList = false;
    const QStringList lines = markdown.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            if (inList) {
                html += QStringLiteral("</div>");
                inList = false;
            }
            continue;
        }

        // 先转义 HTML，再替换 **加粗**（星号不受转义影响）。
        QString inlineText = escapeHtml(line);
        inlineText.replace(QRegularExpression(QStringLiteral("\\*\\*(.+?)\\*\\*")),
                           QStringLiteral("<b>\\1</b>"));

        if (line.startsWith(QStringLiteral("## "))) {
            if (inList) {
                html += QStringLiteral("</div>");
                inList = false;
            }
            html += QStringLiteral("<div style='color:%1; font-size:14px; font-weight:600; margin:10px 0 4px;'>%2</div>")
                        .arg(headingColor, inlineText.mid(3));
        } else if (line.startsWith(QStringLiteral("- "))) {
            if (!inList) {
                html += QStringLiteral("<div style='margin:2px 0 2px 14px;'>");
                inList = true;
            }
            html += QStringLiteral("• %1<br>").arg(inlineText.mid(2));
        } else {
            if (inList) {
                html += QStringLiteral("</div>");
                inList = false;
            }
            html += QStringLiteral("<div>%1</div>").arg(inlineText);
        }
    }
    if (inList)
        html += QStringLiteral("</div>");
    html += QStringLiteral("</div>");
    return html;
}

QString AiReportCard::toggleStyle(QPushButton *btn) const
{
    if (btn->isChecked()) {
        return QString(
            "QPushButton { border: none; border-radius: 6px; padding: 0 12px;"
            " font-size: 12px; color: %1; background: %2; }")
            .arg(DesignTokens::kAccentLight().name(),
                 DesignTokens::kAccent().name());
    }
    return QString(
        "QPushButton { border: none; border-radius: 6px; padding: 0 12px;"
        " font-size: 12px; color: %1; background: transparent; }"
        "QPushButton:hover { background: %2; }")
        .arg(DesignTokens::kTextMute().name(),
             DesignTokens::kButtonHoverBg().name(QColor::HexArgb));
}
