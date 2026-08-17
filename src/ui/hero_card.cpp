#include "ui/hero_card.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"
#include "ui/ui_utils.h"

#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QTime>
#include <QVBoxLayout>
#include <QtMath>

#include <algorithm>
#include <cmath>

/// 24 小时日晷：宽卡画上弧，窄卡降级为横向时段条。当前小时用刻度针标出。
class SundialArea : public QWidget
{
public:
    explicit SundialArea(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(72);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(ThemeManager::instance(), &ThemeManager::themeChanged,
                this, [this](ThemeManager::Theme) { update(); });
    }

    void setHours(const std::array<int, 24> &hours)
    {
        m_hours = hours;
        m_maxHour = 1;
        m_empty = true;
        for (int v : m_hours) {
            m_maxHour = std::max(m_maxHour, v);
            if (v > 0)
                m_empty = false;
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);

        if (m_empty) {
            p.setFont(DesignTokens::appFont(13));
            p.setPen(DesignTokens::kTextFaint());
            p.drawText(rect(), Qt::AlignCenter, QStringLiteral("今日暂无记录"));
            return;
        }

        if (width() < 280)
            paintBar(p);
        else
            paintArc(p);
    }

private:
    void paintArc(QPainter &p)
    {
        const qreal w = width();
        const qreal h = height();
        const qreal margin = 10.0;
        const qreal labelH = 16.0;
        const qreal availableH = h - labelH - 4.0;
        const qreal radius = qMin(w * 0.46, availableH * 1.05);
        const QPointF center(w / 2.0, margin + radius);
        const QRectF arcRect(center.x() - radius, center.y() - radius,
                             radius * 2.0, radius * 2.0);
        const qreal trackW = qBound(8.0, radius * 0.16, 16.0);

        QPen track(DesignTokens::kProgressBg(), trackW);
        track.setCapStyle(Qt::FlatCap);
        p.setPen(track);
        p.setBrush(Qt::NoBrush);
        p.drawArc(arcRect, 0, 180 * 16);

        const int nowHour = QTime::currentTime().hour();
        for (int hour = 0; hour < 24; ++hour) {
            const qreal startA = 180.0 - hour * (180.0 / 24.0);
            const qreal span = 180.0 / 24.0 - 0.8;
            const qreal t = qreal(m_hours[hour]) / qreal(m_maxHour);
            if (t <= 0.0)
                continue;
            QColor c = DesignTokens::kAccent();
            c.setAlphaF(0.28 + 0.72 * t);
            QPen seg(c, trackW);
            seg.setCapStyle(Qt::FlatCap);
            p.setPen(seg);
            p.drawArc(arcRect, qRound(startA * 16), -qRound(span * 16));
        }

        // 当前时刻刻度针。
        const qreal nowA = 180.0 - (nowHour + QTime::currentTime().minute() / 60.0)
                           * (180.0 / 24.0);
        const qreal rad = qDegreesToRadians(nowA);
        const QPointF inner(center.x() + std::cos(rad) * (radius - trackW),
                            center.y() - std::sin(rad) * (radius - trackW));
        const QPointF outer(center.x() + std::cos(rad) * (radius + 6.0),
                            center.y() - std::sin(rad) * (radius + 6.0));
        p.setPen(QPen(DesignTokens::kTextStrong(), 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(inner, outer);
        p.setBrush(DesignTokens::kAccent());
        p.setPen(Qt::NoPen);
        p.drawEllipse(outer, 3.2, 3.2);

        p.setFont(DesignTokens::appFont(11));
        p.setPen(DesignTokens::kTextFaint());
        p.drawText(QRectF(center.x() - radius - 2, h - labelH, 28, labelH),
                   Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("0"));
        p.drawText(QRectF(center.x() - 14, margin - 2, 28, labelH),
                   Qt::AlignCenter, QStringLiteral("12"));
        p.drawText(QRectF(center.x() + radius - 26, h - labelH, 28, labelH),
                   Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("24"));
    }

    void paintBar(QPainter &p)
    {
        const qreal w = width();
        const qreal h = height();
        const qreal top = 8.0;
        const qreal barH = qMax(18.0, h - 28.0);
        const qreal gap = 2.0;
        const qreal cellW = (w - 23.0 * gap) / 24.0;
        const int nowHour = QTime::currentTime().hour();

        for (int hour = 0; hour < 24; ++hour) {
            const qreal x = hour * (cellW + gap);
            const qreal t = qreal(m_hours[hour]) / qreal(m_maxHour);
            QColor c = DesignTokens::kProgressBg();
            if (t > 0.0) {
                c = DesignTokens::kAccent();
                c.setAlphaF(0.28 + 0.72 * t);
            }
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawRoundedRect(QRectF(x, top, cellW, barH), 2.0, 2.0);
            if (hour == nowHour) {
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(DesignTokens::kTextStrong(), 1.2));
                p.drawRoundedRect(QRectF(x, top, cellW, barH), 2.0, 2.0);
            }
        }

        p.setFont(DesignTokens::appFont(11));
        p.setPen(DesignTokens::kTextFaint());
        p.drawText(QRectF(0, h - 16, 24, 16), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("0"));
        p.drawText(QRectF(w / 2.0 - 12, h - 16, 24, 16), Qt::AlignCenter,
                   QStringLiteral("12"));
        p.drawText(QRectF(w - 24, h - 16, 24, 16), Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("24"));
    }

    std::array<int, 24> m_hours {};
    int m_maxHour = 1;
    bool m_empty = true;
};

HeroCard::HeroCard(QWidget *parent)
    : CardFrame(QStringLiteral("今日总时长"), parent)
{
    contentLayout()->setContentsMargins(24, 14, 24, 16);
    contentLayout()->setSpacing(8);

    m_timeLabel = new QLabel(QStringLiteral("0m"), this);
    m_timeLabel->setFont(DesignTokens::monoFont(40, QFont::Bold));
    contentLayout()->addWidget(m_timeLabel);

    m_subLabel = new QLabel(this);
    m_subLabel->setFont(DesignTokens::appFont(13));
    m_subLabel->setWordWrap(true);
    contentLayout()->addWidget(m_subLabel);

    m_sundial = new SundialArea(this);
    contentLayout()->addWidget(m_sundial, 1);

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
    m_sundial->setHours(hourTotals);
    updateDisplay();
}

void HeroCard::applyLabelColors()
{
    m_timeLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;")
            .arg(DesignTokens::kTextStrong().name()));
}

void HeroCard::updateDisplay()
{
    m_timeLabel->setText(UiUtils::formatDuration(m_today));

    QStringList parts;
    if (m_yesterday > 0) {
        const int delta = m_today - m_yesterday;
        const int pct = UiUtils::percentChange(m_today, m_yesterday);
        if (delta < 0)
            parts << QStringLiteral("比昨日少 %1 · %2%")
                         .arg(UiUtils::formatDuration(-delta)).arg(qAbs(pct));
        else if (delta > 0)
            parts << QStringLiteral("比昨日多 %1 · %2%")
                         .arg(UiUtils::formatDuration(delta)).arg(pct);
        else
            parts << QStringLiteral("与昨日持平");
    } else {
        parts << QStringLiteral("昨日暂无数据");
    }

    if (m_goal > 0) {
        const int pct = qMin(100, m_today * 100 / m_goal);
        if (m_today >= m_goal) {
            const int overMinutes = (m_today - m_goal) / 60;
            parts << (overMinutes > 0
                          ? QStringLiteral("已超目标 %1 分钟").arg(overMinutes)
                          : QStringLiteral("已达目标"));
            m_subLabel->setStyleSheet(
                QStringLiteral("color: %1; background: transparent;")
                    .arg(DesignTokens::kAmber().name()));
        } else {
            parts << QStringLiteral("距目标 %1 · %2%")
                         .arg(UiUtils::formatDuration(m_goal - m_today))
                         .arg(pct);
            m_subLabel->setStyleSheet(
                QStringLiteral("color: %1; background: transparent;")
                    .arg(DesignTokens::kTextMute().name()));
        }
    } else {
        m_subLabel->setStyleSheet(
            QStringLiteral("color: %1; background: transparent;")
                .arg(DesignTokens::kTextMute().name()));
    }

    m_subLabel->setText(parts.join(QStringLiteral("  ·  ")));
}
