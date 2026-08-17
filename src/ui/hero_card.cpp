#include "ui/hero_card.h"
#include "ui/design_tokens.h"
#include "ui/period_distribution_layout.h"
#include "ui/theme_manager.h"
#include "ui/ui_utils.h"

#include <QBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QResizeEvent>
#include <QWidget>

#include <array>

class PeriodDistributionArea : public QWidget
{
public:
    explicit PeriodDistributionArea(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(72);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(ThemeManager::instance(), &ThemeManager::themeChanged,
                this, [this](ThemeManager::Theme) { update(); });
    }

    void setHours(const std::array<int, 24> &hours)
    {
        m_periods = PeriodDistributionLayout::aggregate(hours);
        m_total = PeriodDistributionLayout::totalSeconds(m_periods);
        m_peak = PeriodDistributionLayout::peakPeriod(m_periods);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        if (m_peak < 0) {
            painter.setFont(DesignTokens::appFont(12));
            painter.setPen(DesignTokens::kTextPlaceholder());
            painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("今日暂无记录"));
            return;
        }

        static const QString kPeriodNames[] = {
            QStringLiteral("凌晨"), QStringLiteral("上午"),
            QStringLiteral("下午"), QStringLiteral("晚间")
        };

        const int peakPercent = PeriodDistributionLayout::percent(m_periods[m_peak], m_total);
        painter.setFont(DesignTokens::appFont(10, QFont::Medium));
        painter.setPen(DesignTokens::kTextMute());
        painter.drawText(QRectF(0, 0, width(), 16), Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("使用高峰 · %1 %2%").arg(kPeriodNames[m_peak])
                         .arg(peakPercent));

        constexpr qreal top = 21.0;
        constexpr qreal gap = 6.0;
        constexpr qreal trackHeight = 5.0;
        const qreal cellWidth = qMax<qreal>(0.0, (width() - 3.0 * gap) / 4.0);
        const qreal durationY = top + 18.0;
        const qreal trackY = height() - trackHeight - 4.0;

        for (int index = 0; index < PeriodDistributionLayout::kPeriodCount; ++index) {
            const qreal x = index * (cellWidth + gap);
            const QRectF cellRect(x, top, cellWidth, trackY - top - 3.0);
            const bool isPeak = index == m_peak;
            const bool hasUsage = m_periods[index] > 0;

            painter.setFont(DesignTokens::appFont(10,
                isPeak ? QFont::DemiBold : QFont::Medium));
            painter.setPen(isPeak ? DesignTokens::kTextStrong()
                                  : DesignTokens::kTextMute());
            painter.drawText(QRectF(cellRect.left(), cellRect.top(), cellRect.width(), 15),
                             Qt::AlignLeft | Qt::AlignVCenter, kPeriodNames[index]);

            painter.setFont(DesignTokens::monoFont(10,
                isPeak ? QFont::DemiBold : QFont::Medium));
            painter.setPen(hasUsage ? DesignTokens::kText()
                                    : DesignTokens::kTextFaint());
            painter.drawText(QRectF(cellRect.left(), durationY, cellRect.width(), 15),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             UiUtils::formatCompact(m_periods[index]));

            const QRectF trackRect(x, trackY, cellWidth, trackHeight);
            painter.setPen(Qt::NoPen);
            painter.setBrush(DesignTokens::kProgressBg());
            painter.drawRoundedRect(trackRect, trackHeight / 2.0, trackHeight / 2.0);
            if (!hasUsage)
                continue;

            QColor fill = DesignTokens::kAccent();
            if (!isPeak)
                fill.setAlphaF(0.48);
            const qreal ratio = static_cast<qreal>(m_periods[index]) / m_total;
            const qreal fillWidth = qMin(cellWidth,
                qMax<qreal>(3.0, cellWidth * ratio));
            painter.setBrush(fill);
            painter.drawRoundedRect(QRectF(x, trackY, fillWidth, trackHeight),
                                   trackHeight / 2.0, trackHeight / 2.0);
        }
    }

private:
    std::array<int, PeriodDistributionLayout::kPeriodCount> m_periods {};
    int m_total = 0;
    int m_peak = -1;
};

HeroCard::HeroCard(QWidget *parent)
    : CardFrame(QStringLiteral("今日总时长"), parent)
{
    setObjectName(QStringLiteral("heroCard"));
    setMinimumHeight(DesignTokens::kHeroMinHeightWide);
    titleLabel()->setObjectName(QStringLiteral("heroCardTitle"));
    contentLayout()->setContentsMargins(DesignTokens::kCardPaddingHorizontal, 14,
                                        DesignTokens::kCardPaddingHorizontal,
                                        DesignTokens::kSpacingLg);
    contentLayout()->setSpacing(DesignTokens::kControlGap);

    m_bodyLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    m_bodyLayout->setContentsMargins(0, 0, 0, 0);
    m_bodyLayout->setSpacing(DesignTokens::kSpacingLg);

    m_summaryArea = new QWidget(this);
    m_summaryArea->setObjectName(QStringLiteral("heroSummaryArea"));
    auto *summaryLayout = new QVBoxLayout(m_summaryArea);
    summaryLayout->setContentsMargins(0, 0, 0, 0);
    summaryLayout->setSpacing(DesignTokens::kCompactGap);

    m_timeLabel = new QLabel(QStringLiteral("0m"), m_summaryArea);
    m_timeLabel->setObjectName(QStringLiteral("heroTimeLabel"));
    m_timeLabel->setFont(DesignTokens::monoFont(40, QFont::Bold));
    summaryLayout->addWidget(m_timeLabel);

    m_comparisonLabel = new QLabel(m_summaryArea);
    m_comparisonLabel->setObjectName(QStringLiteral("heroComparisonLabel"));
    m_comparisonLabel->setFont(DesignTokens::appFont(12));
    m_comparisonLabel->setWordWrap(true);
    summaryLayout->addWidget(m_comparisonLabel);

    m_goalLabel = new QLabel(m_summaryArea);
    m_goalLabel->setObjectName(QStringLiteral("heroGoalLabel"));
    m_goalLabel->setFont(DesignTokens::appFont(12, QFont::Medium));
    m_goalLabel->setWordWrap(true);
    summaryLayout->addWidget(m_goalLabel);

    m_periodDistribution = new PeriodDistributionArea(this);
    m_periodDistribution->setObjectName(QStringLiteral("heroPeriodDistribution"));

    m_bodyLayout->addWidget(m_summaryArea, 4);
    m_bodyLayout->addWidget(m_periodDistribution, 3);
    contentLayout()->addLayout(m_bodyLayout);
    updateCompactLayout();

    applyLabelColors();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) {
                applyLabelColors();
                updateDisplay();
            });
}

void HeroCard::setData(int todaySeconds, int yesterdaySeconds, int goalSeconds,
                       const std::array<int, 24> &hourTotals)
{
    m_today = todaySeconds;
    m_yesterday = yesterdaySeconds;
    m_goal = goalSeconds;
    m_periodDistribution->setHours(hourTotals);
    updateDisplay();
}

void HeroCard::resizeEvent(QResizeEvent *event)
{
    CardFrame::resizeEvent(event);
    updateCompactLayout();
}

void HeroCard::applyLabelColors()
{
    m_timeLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;")
            .arg(DesignTokens::kTextStrong().name()));
    m_comparisonLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;")
            .arg(DesignTokens::kTextMute().name()));
}

void HeroCard::updateCompactLayout()
{
    const bool compact = width() < DesignTokens::kHeroCompactBreakpoint;
    if (m_compactLayout == compact)
        return;

    m_compactLayout = compact;
    m_bodyLayout->setDirection(compact ? QBoxLayout::TopToBottom
                                       : QBoxLayout::LeftToRight);
    m_bodyLayout->setSpacing(compact ? DesignTokens::kControlGap
                                     : DesignTokens::kSpacingLg);
    m_periodDistribution->setFixedHeight(compact ? 76 : 88);
    setMinimumHeight(compact ? DesignTokens::kHeroMinHeightCompact
                             : DesignTokens::kHeroMinHeightWide);
}

void HeroCard::updateDisplay()
{
    m_timeLabel->setText(UiUtils::formatDuration(m_today));

    if (m_yesterday > 0) {
        const int delta = m_today - m_yesterday;
        const int pct = UiUtils::percentChange(m_today, m_yesterday);
        if (delta < 0)
            m_comparisonLabel->setText(
                QStringLiteral("较昨日少 %1 · %2%")
                    .arg(UiUtils::formatDuration(-delta)).arg(qAbs(pct)));
        else if (delta > 0)
            m_comparisonLabel->setText(
                QStringLiteral("较昨日多 %1 · %2%")
                    .arg(UiUtils::formatDuration(delta)).arg(pct));
        else
            m_comparisonLabel->setText(QStringLiteral("与昨日持平"));
    } else {
        m_comparisonLabel->setText(QStringLiteral("昨日暂无数据"));
    }

    if (m_goal <= 0) {
        m_goalLabel->setText(QStringLiteral("未设置每日目标"));
        m_goalLabel->setStyleSheet(
            QStringLiteral("color: %1; background: transparent;")
                .arg(DesignTokens::kTextMute().name()));
        return;
    }

    const int pct = qMin(100, m_today * 100 / m_goal);
    if (m_today >= m_goal) {
        const int overMinutes = (m_today - m_goal) / 60;
        m_goalLabel->setText(overMinutes > 0
            ? QStringLiteral("目标已超 %1 分钟").arg(overMinutes)
            : QStringLiteral("今日目标已达成"));
        m_goalLabel->setStyleSheet(
            QStringLiteral("color: %1; background: transparent;")
                .arg(DesignTokens::kSuccess().name()));
    } else {
        m_goalLabel->setText(QStringLiteral("距目标 %1 · 已完成 %2%")
            .arg(UiUtils::formatDuration(m_goal - m_today)).arg(pct));
        m_goalLabel->setStyleSheet(
            QStringLiteral("color: %1; background: transparent;")
                .arg(DesignTokens::kAmber().name()));
    }
}
