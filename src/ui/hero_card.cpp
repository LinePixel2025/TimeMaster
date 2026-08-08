#include "ui/hero_card.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"
#include "ui/ui_utils.h"

#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>

/// Self-contained goal progress ring that fills its widget area.
class GoalRing : public QWidget
{
public:
    explicit GoalRing(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(96, 96);
        connect(ThemeManager::instance(), &ThemeManager::themeChanged,
                this, [this](ThemeManager::Theme) { update(); });
    }

    void setProgress(int pct)
    {
        m_pct = qBound(0, pct, 100);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const qreal side = qMin(width(), height());
        const qreal lineWidth = qMax<qreal>(6.0, side * 0.09);
        const qreal inset = lineWidth / 2.0 + 2.0;
        const QRectF ringRect((width() - side) / 2.0 + inset,
                              (height() - side) / 2.0 + inset,
                              side - inset * 2.0,
                              side - inset * 2.0);
        const qreal radius = ringRect.width() / 2.0;

        QPen bgPen(DesignTokens::kProgressBg(), lineWidth);
        bgPen.setCapStyle(Qt::RoundCap);
        painter.setPen(bgPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(ringRect.center(), radius, radius);

        if (m_pct > 0) {
            QPen fgPen(DesignTokens::kAccent(), lineWidth);
            fgPen.setCapStyle(Qt::RoundCap);
            painter.setPen(fgPen);
            painter.drawArc(ringRect, 90 * 16, -qRound(m_pct * 3.6 * 16));
        }

        painter.setFont(DesignTokens::appFont(qMax(13, qRound(side * 0.16)), QFont::Bold));
        painter.setPen(DesignTokens::kTextStrong());
        painter.drawText(ringRect, Qt::AlignCenter, QString("%1%").arg(m_pct));
    }

private:
    int m_pct = 0;
};

HeroCard::HeroCard(QWidget *parent)
    : CardFrame(QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe6\x80\xbb\xe6\x97\xb6\xe9\x95\xbf"), parent)
{
    contentLayout()->setContentsMargins(24, 14, 24, 16);
    contentLayout()->setSpacing(10);

    auto *contentRow = new QHBoxLayout();
    contentRow->setContentsMargins(0, 0, 0, 0);
    contentRow->setSpacing(16);

    auto *leftCol = new QVBoxLayout();
    leftCol->setContentsMargins(0, 0, 0, 0);
    leftCol->setSpacing(6);

    m_timeLabel = new QLabel("0m", this);
    m_timeLabel->setFont(DesignTokens::appFont(34, QFont::Bold));
    m_timeLabel->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(DesignTokens::kTextStrong().name()));
    leftCol->addWidget(m_timeLabel);

    m_subLabel = new QLabel(this);
    m_subLabel->setFont(DesignTokens::appFont(13));
    m_subLabel->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(DesignTokens::kTextMute().name()));
    leftCol->addWidget(m_subLabel);

    m_goalLabel = new QLabel(this);
    m_goalLabel->setFont(DesignTokens::appFont(13, QFont::Medium));
    m_goalLabel->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(DesignTokens::kAccent().name()));
    leftCol->addWidget(m_goalLabel);
    leftCol->addStretch();

    m_ring = new GoalRing(this);
    m_ring->setFixedSize(110, 110);

    contentRow->addLayout(leftCol, 1);
    contentRow->addWidget(m_ring, 0, Qt::AlignVCenter);
    contentLayout()->addLayout(contentRow, 1);

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
    });
}

void HeroCard::setData(int todaySeconds, int yesterdaySeconds, int goalSeconds)
{
    m_today = todaySeconds;
    m_yesterday = yesterdaySeconds;
    m_goal = goalSeconds;
    updateDisplay();
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
        m_ring->setProgress(pct);
        m_goalLabel->setText(
            QString::fromUtf8("\xe7\x9b\xae\xe6\xa0\x87 %1h \xc2\xb7 %2%")
                .arg(qMax(0, m_goal) / 3600)
                .arg(pct));
    } else {
        m_ring->setProgress(0);
        m_goalLabel->clear();
    }
}
