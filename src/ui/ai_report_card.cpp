#include "ui/ai_report_card.h"
#include "ai/ai_client.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"
#include "ui/ui_utils.h"

#include <QDate>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

// 极简 HTML 转义：AI 报告正文的富文本展示需要。
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

// 周几中文（1=周一 … 7=周日）。
QString weekdayCn(int dayOfWeek)
{
    static const QString kNames[] = {
        QStringLiteral("一"), QStringLiteral("二"), QStringLiteral("三"),
        QStringLiteral("四"), QStringLiteral("五"), QStringLiteral("六"),
        QStringLiteral("日")
    };
    return (dayOfWeek >= 1 && dayOfWeek <= 7) ? kNames[dayOfWeek - 1] : QString();
}

} // namespace

/// 总结区：日期、8 字总结、说明与文字按钮。材质交给外层 CardFrame。
class SummaryCard : public QWidget
{
    Q_OBJECT
public:
    explicit SummaryCard(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 4, 0, 0);
        layout->setSpacing(6);

        m_dateLabel = new QLabel(this);
        m_dateLabel->setAlignment(Qt::AlignLeft);
        m_dateLabel->setFont(DesignTokens::eyebrowFont(11));
        layout->addWidget(m_dateLabel);

        m_phraseLabel = new QLabel(this);
        m_phraseLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_phraseLabel->setWordWrap(true);
        m_phraseLabel->setFont(DesignTokens::appFont(22, QFont::DemiBold));
        layout->addWidget(m_phraseLabel, 1);

        m_captionLabel = new QLabel(this);
        m_captionLabel->setAlignment(Qt::AlignLeft);
        m_captionLabel->setFont(DesignTokens::appFont(11));
        layout->addWidget(m_captionLabel);

        auto *bottomRow = new QHBoxLayout();
        bottomRow->setContentsMargins(0, 4, 0, 0);
        bottomRow->setSpacing(8);

        m_refreshBtn = new QPushButton(QStringLiteral("生成分析"), this);
        m_refreshBtn->setCursor(Qt::PointingHandCursor);
        m_refreshBtn->setMinimumHeight(32);
        connect(m_refreshBtn, &QPushButton::clicked,
                this, &SummaryCard::refreshRequested);
        bottomRow->addWidget(m_refreshBtn);

        m_weeklyBtn = new QPushButton(QStringLiteral("上周周报"), this);
        m_weeklyBtn->setCursor(Qt::PointingHandCursor);
        m_weeklyBtn->setMinimumHeight(32);
        connect(m_weeklyBtn, &QPushButton::clicked,
                this, &SummaryCard::weeklyClicked);
        bottomRow->addWidget(m_weeklyBtn);

        m_detailBtn = new QPushButton(QStringLiteral("今日报告"), this);
        m_detailBtn->setCursor(Qt::PointingHandCursor);
        m_detailBtn->setMinimumHeight(32);
        connect(m_detailBtn, &QPushButton::clicked,
                this, &SummaryCard::detailClicked);
        bottomRow->addWidget(m_detailBtn);
        bottomRow->addStretch();
        layout->addLayout(bottomRow);
        applyTheme();
    }

    void applyTheme()
    {
        m_dateLabel->setStyleSheet(
            QStringLiteral("color: %1; background: transparent;")
                .arg(DesignTokens::kTextMute().name()));
        m_phraseLabel->setStyleSheet(
            QStringLiteral("color: %1; background: transparent;")
                .arg(DesignTokens::kTextStrong().name()));
        m_captionLabel->setStyleSheet(
            QStringLiteral("color: %1; background: transparent;")
                .arg(DesignTokens::kTextMute().name()));

        const QString secondary = QStringLiteral(
            "QPushButton { background: %1; color: %2; border: 1px solid %3;"
            " border-radius: 6px; padding: 0 12px; font-size: 12px; }"
            "QPushButton:hover { background: %4; }")
            .arg(DesignTokens::kSurface().name(),
                 DesignTokens::kText().name(),
                 DesignTokens::kBorder().name(),
                 DesignTokens::kButtonHoverBg().name());
        const QString primary = QStringLiteral(
            "QPushButton { background: %1; color: white; border: 1px solid %1;"
            " border-radius: 6px; padding: 0 14px; font-size: 12px; font-weight: 600; }"
            "QPushButton:hover { background: %2; border-color: %2; }")
            .arg(DesignTokens::kAccent().name(),
                 DesignTokens::kAccentHover().name());
        m_refreshBtn->setStyleSheet(secondary);
        m_weeklyBtn->setStyleSheet(secondary);
        m_detailBtn->setStyleSheet(primary);
    }

    void setDateText(const QString &text) { m_dateLabel->setText(text); }
    void setPhrase(const QString &phrase) { m_phraseLabel->setText(phrase); }
    void setCaption(const QString &caption) { m_captionLabel->setText(caption); }
    /// 各操作按钮按状态独立控制可见性：
    /// detail（↗ 查看网页）与 weekly（▤ 周报）在 AI 未配置时仍有价值，
    /// refresh（⟳ AI 生成）仅在 AI 可用时显示。
    void setButtonsVisible(bool refresh, bool detail, bool weekly)
    {
        m_refreshBtn->setVisible(refresh);
        m_detailBtn->setVisible(detail);
        m_weeklyBtn->setVisible(weekly);
    }

signals:
    void detailClicked();
    void refreshRequested();
    void weeklyClicked();

private:
    QLabel *m_dateLabel = nullptr;
    QLabel *m_phraseLabel = nullptr;
    QLabel *m_captionLabel = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QPushButton *m_weeklyBtn = nullptr;
    QPushButton *m_detailBtn = nullptr;
};

AiReportCard::AiReportCard(AiClient *ai, QWidget *parent)
    : CardFrame(QStringLiteral("AI 使用报告"), parent)
    , m_ai(ai)
{
    contentLayout()->setContentsMargins(20, 14, 20, 16);
    contentLayout()->setSpacing(8);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setFont(DesignTokens::appFont(11));
    m_hintLabel->setTextFormat(Qt::PlainText);
    contentLayout()->addWidget(m_hintLabel);

    m_summaryCard = new SummaryCard(this);
    // ↗：在浏览器打开今日报告网页（统计 + AI 分析）。
    connect(m_summaryCard, &SummaryCard::detailClicked,
            this, &AiReportCard::dailyReportOpenRequested);
    // 刷新：重新生成今日 AI 分析，完成后日报网页自动更新。
    connect(m_summaryCard, &SummaryCard::refreshRequested, this, [this]() {
        setLoading(true);
        emit generateRequested();
    });
    // 上周周报菜单：打开 / 立即生成 / 重新生成（复用周报信号链）。
    connect(m_summaryCard, &SummaryCard::weeklyClicked,
            this, &AiReportCard::showWeeklyMenu);
    contentLayout()->addWidget(m_summaryCard, 1);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) {
        applyThemeColors();
        refreshContent();
    });

    applyThemeColors();
    reloadState();
}

void AiReportCard::applyThemeColors()
{
    m_hintLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;")
            .arg(DesignTokens::kTextMute().name()));
    if (m_summaryCard)
        m_summaryCard->applyTheme();
}

void AiReportCard::setWeeklyReportPath(const QString &path)
{
    // 路径为空或文件已不存在时视为未生成（弹窗内按钮据此禁用）。
    m_weeklyReportPath =
        (!path.isEmpty() && QFile::exists(path)) ? path : QString();
}

void AiReportCard::reloadState()
{
    m_reportText = m_ai->cachedReport(AiPeriod::daily());
    refreshContent();
}

void AiReportCard::setReport(const QString &text)
{
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

void AiReportCard::showError(const QString &error)
{
    m_error = error;
    m_loading = false;
    refreshContent();
}

void AiReportCard::showWeeklyMenu()
{
    QMenu menu(this);
    UiUtils::applyMenuStyle(&menu);
    QAction *openAct = menu.addAction(QStringLiteral("打开上周周报"));
    menu.addSeparator();
    QAction *genAct = menu.addAction(QStringLiteral("立即生成上周周报"));
    QAction *reAct = menu.addAction(QStringLiteral("重新生成（覆盖当前周报）"));
    openAct->setEnabled(!m_weeklyReportPath.isEmpty());
    if (!m_weeklyReportPath.isEmpty())
        openAct->setToolTip(QStringLiteral("打开 %1").arg(m_weeklyReportPath));
    connect(openAct, &QAction::triggered, this, [this]() {
        emit weeklyReportOpenRequested(m_weeklyReportPath);
    });
    connect(genAct, &QAction::triggered, this, [this]() {
        emit weeklyReportGenerateRequested();
    });
    connect(reAct, &QAction::triggered, this, [this]() {
        emit weeklyReportRegenerateRequested();
    });
    menu.exec(m_summaryCard->mapToGlobal(m_summaryCard->rect().bottomRight()));
}

void AiReportCard::refreshContent()
{
    const bool hasReport = !m_reportText.isEmpty();

    // 整卡始终显示（渐变卡即组件本体），状态差异体现在大字与图标上。
    m_summaryCard->setDateText(todayText());

    if (m_loading) {
        m_hintLabel->setText(QStringLiteral("正在生成报告…"));
        m_summaryCard->setPhrase(QStringLiteral("正在生成报告…"));
        m_summaryCard->setCaption(QStringLiteral("AI 正在分析今日使用数据"));
        m_summaryCard->setButtonsVisible(false, true, true);
        return;
    }

    if (!m_error.isEmpty()) {
        m_hintLabel->setText(QStringLiteral("生成失败，可以重试"));
        m_hintLabel->setStyleSheet(
            QStringLiteral("color: %1; background: transparent; font-weight: 600;")
                .arg(DesignTokens::kError().name()));
        m_summaryCard->setPhrase(QStringLiteral("生成失败，可以重试"));
        m_summaryCard->setCaption(QStringLiteral("可重新生成分析，或先查看今日统计报告"));
        m_summaryCard->setButtonsVisible(true, true, true);
        return;
    }
    applyThemeColors();

    if (!m_ai->isConfigured()) {
        m_reportText.clear();
        m_hintLabel->setText(QStringLiteral("AI 智能尚未配置"));
        m_summaryCard->setPhrase(QStringLiteral("先看今日统计"));
        m_summaryCard->setCaption(QStringLiteral("可打开今日报告；设置中启用 AI 后可生成分析"));
        m_summaryCard->setButtonsVisible(false, true, true);
        return;
    }

    if (hasReport) {
        const QString cachedDate = m_ai->cachedReportDate(AiPeriod::daily());
        const bool fresh = (cachedDate == QDate::currentDate().toString(Qt::ISODate));
        m_hintLabel->setText(
            fresh ? QStringLiteral("报告已更新 · %1").arg(todayText())
                  : QStringLiteral("报告缓存于 %1，可重新生成").arg(cachedDate));
        m_summaryCard->setCaption(QStringLiteral("AI 今日总结"));
        m_summaryCard->setButtonsVisible(true, true, true);
        updateSummaryCard();
        return;
    }

    m_hintLabel->setText(QStringLiteral("还没有今日分析"));
    m_summaryCard->setPhrase(QStringLiteral("还没有今日报告"));
    m_summaryCard->setCaption(QStringLiteral("可先查看统计报告，或生成每日分析"));
    m_summaryCard->setButtonsVisible(true, true, true);
}

void AiReportCard::updateSummaryCard()
{
    QString phrase = summaryPhrase();
    if (phrase.isEmpty())
        phrase = QStringLiteral("今日使用情况");
    m_summaryCard->setPhrase(phrase);
}

QString AiReportCard::summaryPhrase() const
{
    if (m_reportText.isEmpty())
        return QString();
    const QStringList lines = m_reportText.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        int pos = -1;
        if (line.startsWith(QStringLiteral("【总结】")))
            pos = 4;
        else if (line.startsWith(QStringLiteral("总结：")))
            pos = 3;
        if (pos < 0)
            continue;
        QString phrase = line.mid(pos).trimmed();
        if (phrase.isEmpty())
            break;
        if (phrase.endsWith(QStringLiteral("。")) || phrase.endsWith(QLatin1Char('.')))
            phrase.chop(1);
        return capPhrase(phrase);
    }
    // 旧缓存报告可能没有【总结】行：回退到概览小节首句。
    return capPhrase(overviewFirstSentence());
}

QString AiReportCard::overviewFirstSentence() const
{
    const QStringList lines = m_reportText.split(QLatin1Char('\n'));
    bool inOverview = false;
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QStringLiteral("## "))) {
            inOverview = line.contains(QStringLiteral("概览"));
            continue;
        }
        if (inOverview && !line.isEmpty())
            return line;
    }
    // 无概览小节：取首个普通段落。
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (!line.isEmpty() && !line.startsWith(QStringLiteral("## "))
            && !line.startsWith(QStringLiteral("- ")))
            return line;
    }
    return QString();
}

QString AiReportCard::capPhrase(const QString &phrase) const
{
    if (phrase.size() <= 12)
        return phrase;
    return phrase.left(12) + QStringLiteral("…");
}

QString AiReportCard::todayText() const
{
    const QDate d = QDate::currentDate();
    return QStringLiteral("%1月%2日 · 星期%3")
        .arg(d.month()).arg(d.day()).arg(weekdayCn(d.dayOfWeek()));
}

// SummaryCard 定义在本文件内且含 Q_OBJECT，需显式包含 moc 输出。
#include "ai_report_card.moc"
