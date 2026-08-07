#include "ui/hero_card.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"
#include "ui/ui_utils.h"

#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

HeroCard::HeroCard(QWidget *parent)
    : CardFrame(QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe6\x80\xbb\xe6\x97\xb6\xe9\x95\xbf"), parent)
{
    contentLayout()->setContentsMargins(24, 18, 24, 22);
    contentLayout()->setSpacing(4);

    m_timeLabel = new QLabel("0m", this);
    m_timeLabel->setFont(DesignTokens::appFont(36, QFont::Bold));
    m_timeLabel->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(DesignTokens::kTextStrong().name()));
    contentLayout()->addWidget(m_timeLabel);

    m_subLabel = new QLabel(this);
    m_subLabel->setFont(DesignTokens::appFont(13));
    m_subLabel->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(DesignTokens::kTextMute().name()));
    contentLayout()->addWidget(m_subLabel);

    contentLayout()->addStretch();

    m_goalLabel = new QLabel(this);
    m_goalLabel->setFont(DesignTokens::appFont(13, QFont::Medium));
    m_goalLabel->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(DesignTokens::kAccent().name()));
    contentLayout()->addWidget(m_goalLabel);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) {
        m_timeLabel->setStyleSheet(
            QString("color: %1; background: transparent;")
                .arg(DesignTokens::kTextStrong().name()));
        m_subLabel->setStyleSheet(
            QString("color: %1; background: transparent;")
                .arg(DesignTokens::kTextMute().name()));
        m_goalLabel->setStyleSheet(
            QString("color: %1; background: transparent;")
                .arg(DesignTokens::kAccent().name()));
        update();
    });
}

void HeroCard::setData(int todaySeconds, int yesterdaySeconds, int goalSeconds)
{
    m_today = todaySeconds;
    m_yesterday = yesterdaySeconds;
    m_goal = goalSeconds;
    updateDisplay();
    update();
}

void HeroCard::updateDisplay()
{
    m_timeLabel->setText(UiUtils::formatDuration(m_today));

    QString sub = QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe6\x80\xbb\xe6\x97\xb6\xe9\x95\xbf");
    if (m_yesterday > 0) {
        const int delta = UiUtils::percentChange(m_today, m_yesterday);
        if (delta > 0)
            sub += QString::fromUtf8("  \xe2\x86\x91 %1%").arg(delta);
        else if (delta < 0)
            sub += QString::fromUtf8("  \xe2\x86\x93 %1%").arg(-delta);
        else
            sub += QString::fromUtf8("  \xe2\x80\x94 0%");
    }
    m_subLabel->setText(sub);

    if (m_goal > 0) {
        const int pct = qMin(100, m_today * 100 / m_goal);
        m_goalLabel->setText(
            QString::fromUtf8("\xe7\x9b\xae\xe6\xa0\x87 %1h \xc2\xb7 %2%")
                .arg(qMax(0, m_goal) / 3600)
                .arg(pct));
    } else {
        m_goalLabel->clear();
    }
}

void HeroCard::paintEvent(QPaintEvent *event)
{
    CardFrame::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // Goal ring in the top-right corner.
    const int ringSize = 84;
    const QRectF ringRect(width() - ringSize - 24, 24, ringSize, ringSize);
    const qreal lineWidth = 8.0;
    const qreal radius = (ringSize - lineWidth) / 2.0;

    QPen bgPen(DesignTokens::kProgressBg(), lineWidth);
    bgPen.setCapStyle(Qt::RoundCap);
    painter.setPen(bgPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(ringRect.center(), radius, radius);

    if (m_goal > 0 && m_today > 0) {
        const qreal ratio = qMin(1.0, static_cast<qreal>(m_today) / m_goal);
        QPen fgPen(DesignTokens::kAccent(), lineWidth);
        fgPen.setCapStyle(Qt::RoundCap);
        painter.setPen(fgPen);
        painter.drawArc(ringRect, 90 * 16, -qRound(ratio * 360 * 16));

        QFont pctFont = DesignTokens::appFont(14, QFont::Bold);
        painter.setFont(pctFont);
        painter.setPen(DesignTokens::kTextStrong());
        painter.drawText(ringRect, Qt::AlignCenter,
                         QString("%1%").arg(qMin(100, m_today * 100 / m_goal)));
    }
}
