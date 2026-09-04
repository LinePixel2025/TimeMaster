#include "ui/ai_report_card.h"
#include "ai/ai_client.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"
#include "ui/ui_utils.h"

#include <QBoxLayout>
#include <QDate>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace {

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

/// AI 洞察条：左侧只保留大字结论（状态信息已上移至卡片标题行），
/// 右侧收纳融合后的主操作与昨日/周报次入口。
class SummaryCard : public QWidget
{
    Q_OBJECT
public:
    explicit SummaryCard(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("aiInsightBar"));
        setMinimumHeight(78);

        m_mainLayout = new QBoxLayout(QBoxLayout::LeftToRight, this);
        m_mainLayout->setContentsMargins(0, DesignTokens::kSpacingXs, 0, 0);
        m_mainLayout->setSpacing(DesignTokens::kSpacingLg);

        m_insightArea = new QWidget(this);
        auto *insightLayout = new QVBoxLayout(m_insightArea);
        insightLayout->setContentsMargins(0, 0, 0, 0);
        insightLayout->setSpacing(DesignTokens::kCompactGap);

        m_phraseLabel = new QLabel(m_insightArea);
        m_phraseLabel->setObjectName(QStringLiteral("aiInsightPhrase"));
        m_phraseLabel->setWordWrap(true);
        m_phraseLabel->setFont(DesignTokens::appFont(18, QFont::DemiBold));
        insightLayout->addWidget(m_phraseLabel);

        m_actionArea = new QWidget(this);
        m_actionsLayout = new QBoxLayout(QBoxLayout::LeftToRight, m_actionArea);
        m_actionsLayout->setContentsMargins(0, 0, 0, 0);
        m_actionsLayout->setSpacing(DesignTokens::kControlGap);

        // 融合后的主按钮：由外层按状态设定「生成分析」/「今日报告」等文案。
        m_primaryBtn = new QPushButton(QStringLiteral("生成分析"), m_actionArea);
        m_primaryBtn->setObjectName(QStringLiteral("aiPrimaryReportButton"));
        m_primaryBtn->setCursor(Qt::PointingHandCursor);
        m_primaryBtn->setMinimumHeight(DesignTokens::kActionButtonMinHeight);
        connect(m_primaryBtn, &QPushButton::clicked,
                this, &SummaryCard::primaryClicked);
        m_actionsLayout->addWidget(m_primaryBtn);

        m_yesterdayBtn = new QPushButton(QStringLiteral("昨日报告"), m_actionArea);
        m_yesterdayBtn->setObjectName(QStringLiteral("aiYesterdayReportButton"));
        m_yesterdayBtn->setCursor(Qt::PointingHandCursor);
        m_yesterdayBtn->setMinimumHeight(DesignTokens::kActionButtonMinHeight);
        connect(m_yesterdayBtn, &QPushButton::clicked,
                this, &SummaryCard::yesterdayClicked);
        m_actionsLayout->addWidget(m_yesterdayBtn);

        m_weeklyBtn = new QPushButton(QStringLiteral("周报 ▾"), m_actionArea);
        m_weeklyBtn->setObjectName(QStringLiteral("aiWeeklyReportButton"));
        m_weeklyBtn->setCursor(Qt::PointingHandCursor);
        m_weeklyBtn->setMinimumHeight(DesignTokens::kActionButtonMinHeight);
        m_weeklyBtn->setToolTip(QStringLiteral("上周周报"));
        connect(m_weeklyBtn, &QPushButton::clicked,
                this, &SummaryCard::weeklyClicked);
        m_actionsLayout->addWidget(m_weeklyBtn);

        m_mainLayout->addWidget(m_insightArea, 1);
        m_mainLayout->addWidget(m_actionArea, 0, Qt::AlignRight | Qt::AlignVCenter);
        applyTheme();
    }

    void applyTheme()
    {
        m_phraseLabel->setStyleSheet(
            QStringLiteral("color: %1; background: transparent;")
                .arg(DesignTokens::kTextStrong().name()));

        const QString secondary = QStringLiteral(
            "QPushButton { background: %1; color: %2; border: 1px solid %3;"
            " border-radius: %4px; padding: 0 10px; font-size: 12px; }"
            "QPushButton:hover { background: %5; }"
            "QPushButton:pressed { background: %6; }"
            "QPushButton:disabled { color: %7; background: transparent; }")
            .arg(DesignTokens::kSurface().name(),
                 DesignTokens::kText().name(),
                 DesignTokens::kBorder().name(),
                 QString::number(DesignTokens::kRadiusBtn),
                 DesignTokens::kButtonHoverBg().name(),
                 DesignTokens::kAccentLight().name(),
                 DesignTokens::kTextFaint().name())
            + UiUtils::focusBorderRule();
        const QString primary = QStringLiteral(
            "QPushButton { background: %1; color: %2; border: 1px solid %1;"
            " border-radius: %3px; padding: 0 12px; font-size: 12px; font-weight: 600; }"
            "QPushButton:hover { background: %4; border-color: %4; }"
            "QPushButton:pressed { background: %5; border-color: %5; }"
            "QPushButton:disabled { background: %6; color: %7; border-color: %6; }")
            .arg(DesignTokens::kAccent().name(),
                 DesignTokens::kOnAccent().name(),
                 QString::number(DesignTokens::kRadiusBtn),
                 DesignTokens::kAccentHover().name(),
                 DesignTokens::kAccentPressed().name(),
                 DesignTokens::kProgressBg().name(),
                 DesignTokens::kTextMute().name())
            + UiUtils::focusBorderRule();
        m_primaryBtn->setStyleSheet(primary);
        m_yesterdayBtn->setStyleSheet(secondary);
        m_weeklyBtn->setStyleSheet(secondary);
    }

    void setPhrase(const QString &phrase) { m_phraseLabel->setText(phrase); }

    /// 融合主按钮的文案与可用状态，由外层按当前报告状态设定。
    void setPrimaryButton(const QString &text, bool enabled)
    {
        m_primaryBtn->setText(text);
        m_primaryBtn->setEnabled(enabled);
    }

signals:
    void primaryClicked();
    void yesterdayClicked();
    void weeklyClicked();

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        const bool compact = width() < DesignTokens::kAiCompactBreakpoint;
        m_mainLayout->setDirection(compact ? QBoxLayout::TopToBottom
                                           : QBoxLayout::LeftToRight);
        m_mainLayout->setSpacing(compact ? DesignTokens::kControlGap
                                         : DesignTokens::kSpacingLg);
        m_actionsLayout->setDirection(compact ? QBoxLayout::TopToBottom
                                              : QBoxLayout::LeftToRight);
        m_mainLayout->setAlignment(m_actionArea,
            compact ? Qt::AlignLeft : Qt::AlignRight | Qt::AlignVCenter);
        setMinimumHeight(compact ? DesignTokens::kAiMinHeightCompact - 62
                                 : 78);
        if (parentWidget())
            parentWidget()->setMinimumHeight(compact ? DesignTokens::kAiMinHeightCompact
                                                     : DesignTokens::kAiMinHeightWide);
    }

private:
    QBoxLayout *m_mainLayout = nullptr;
    QBoxLayout *m_actionsLayout = nullptr;
    QWidget *m_insightArea = nullptr;
    QWidget *m_actionArea = nullptr;
    QLabel *m_phraseLabel = nullptr;
    QPushButton *m_primaryBtn = nullptr;
    QPushButton *m_weeklyBtn = nullptr;
    QPushButton *m_yesterdayBtn = nullptr;
};

AiReportCard::AiReportCard(AiClient *ai, QWidget *parent)
    : CardFrame(QStringLiteral("AI 使用报告"), parent)
    , m_ai(ai)
{
    setObjectName(QStringLiteral("aiReportCard"));
    setMinimumHeight(DesignTokens::kAiMinHeightWide);
    titleLabel()->setObjectName(QStringLiteral("aiReportCardTitle"));
    contentLayout()->setContentsMargins(DesignTokens::kCardPaddingHorizontal, 14,
                                        DesignTokens::kCardPaddingHorizontal,
                                        DesignTokens::kSpacingLg);
    contentLayout()->setSpacing(DesignTokens::kControlGap);

    // 状态信息（日期/生成态）移到标题右侧，仿 TrendCard 的标题行模式。
    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(DesignTokens::kControlGap);
    QLabel *title = titleLabel();
    contentLayout()->removeWidget(title);
    header->addWidget(title);
    header->addStretch();
    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("aiReportCardStatus"));
    m_statusLabel->setFont(DesignTokens::eyebrowFont(11));
    header->addWidget(m_statusLabel, 0, Qt::AlignVCenter);
    contentLayout()->insertLayout(0, header);

    m_summaryCard = new SummaryCard(this);
    connect(m_summaryCard, &SummaryCard::primaryClicked, this, [this]() {
        // 融合按钮统一分流：已配置且缓存非今日 → 先生成（完成后由 main 自动
        // 打开日报网页）；否则（已最新 / 未配置）直接打开今日报告。
        if (m_ai->isConfigured() && !isCacheFreshToday()) {
            setLoading(true);
            emit generateRequested();
        } else {
            emit dailyReportOpenRequested();
        }
    });
    connect(m_summaryCard, &SummaryCard::yesterdayClicked,
            this, &AiReportCard::yesterdayReportOpenRequested);
    connect(m_summaryCard, &SummaryCard::weeklyClicked,
            this, &AiReportCard::showWeeklyMenu);
    contentLayout()->addWidget(m_summaryCard);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) {
        m_summaryCard->applyTheme();
        refreshContent();
    });

    m_summaryCard->applyTheme();
    reloadState();
}

void AiReportCard::setWeeklyReportPath(const QString &path)
{
    // 路径为空或文件已不存在时视为未生成（菜单内按钮据此禁用）。
    m_weeklyReportPath =
        (!path.isEmpty() && QFile::exists(path)) ? path : QString();
}

void AiReportCard::reloadState()
{
    m_reportText = m_ai->cachedReport(AiPeriod::daily());
    m_error.clear();
    m_loading = false;
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
    menu.setObjectName(QStringLiteral("aiWeeklyReportMenu"));
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

bool AiReportCard::isCacheFreshToday() const
{
    return m_ai->cachedReportDate(AiPeriod::daily())
        == QDate::currentDate().toString(Qt::ISODate);
}

void AiReportCard::setHeaderStatus(const QString &text, const QColor &color)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;").arg(color.name()));
}

void AiReportCard::refreshContent()
{
    const bool hasReport = !m_reportText.isEmpty();

    if (m_loading) {
        m_summaryCard->setPrimaryButton(QStringLiteral("正在生成…"), false);
        setHeaderStatus(QStringLiteral("今日洞察 · 正在生成"),
                        DesignTokens::kAccent());
        m_summaryCard->setPhrase(QStringLiteral("正在分析今日使用数据"));
        return;
    }

    if (!m_error.isEmpty()) {
        m_summaryCard->setPrimaryButton(QStringLiteral("重新生成"), true);
        setHeaderStatus(QStringLiteral("今日洞察 · 生成失败"),
                        DesignTokens::kError());
        m_summaryCard->setPhrase(QStringLiteral("生成失败，可以重试"));
        return;
    }

    if (!m_ai->isConfigured()) {
        m_reportText.clear();
        // 未配置时点击融合按钮直接打开统计版日报网页。
        m_summaryCard->setPrimaryButton(QStringLiteral("今日报告"), true);
        setHeaderStatus(QStringLiteral("AI 尚未配置"),
                        DesignTokens::kTextMute());
        m_summaryCard->setPhrase(QStringLiteral("今日统计已经准备好"));
        return;
    }

    if (hasReport) {
        const bool fresh = isCacheFreshToday();
        m_summaryCard->setPrimaryButton(
            fresh ? QStringLiteral("今日报告")
                  : QStringLiteral("生成分析"),
            true);
        const QString cachedDate = m_ai->cachedReportDate(AiPeriod::daily());
        setHeaderStatus(
            fresh ? QStringLiteral("%1 · 已更新").arg(todayText())
                  : QStringLiteral("缓存于 %1 · 可重新生成").arg(cachedDate),
            DesignTokens::kTextMute());
        updateSummaryCard();
        return;
    }

    m_summaryCard->setPrimaryButton(QStringLiteral("生成分析"), true);
    setHeaderStatus(QStringLiteral("今日洞察 · 尚未生成"),
                    DesignTokens::kTextMute());
    m_summaryCard->setPhrase(QStringLiteral("生成一份基于今日记录的使用分析"));
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
    if (phrase.size() <= 24)
        return phrase;
    return phrase.left(24) + QStringLiteral("…");
}

QString AiReportCard::todayText() const
{
    const QDate d = QDate::currentDate();
    return QStringLiteral("%1月%2日 · 星期%3")
        .arg(d.month()).arg(d.day()).arg(weekdayCn(d.dayOfWeek()));
}

// SummaryCard 定义在本文件内且含 Q_OBJECT，需显式包含 moc 输出。
#include "ai_report_card.moc"
