#include "ui/compare_card.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"
#include "ui/ui_utils.h"

#include <QPainter>
#include <QPainterPath>

CompareCard::CompareCard(QWidget *parent)
    : CardFrame(QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe4\xb8\x8e\xe6\x98\xa8\xe6\x97\xa5\xe5\xaf\xb9\xe6\xaf\x94"), parent)
{
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { update(); });
}

void CompareCard::setData(int todaySeconds, int yesterdaySeconds)
{
    m_today = todaySeconds;
    m_yesterday = yesterdaySeconds;
    update();
}

void CompareCard::paintEvent(QPaintEvent *event)
{
    CardFrame::paintEvent(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const qreal w = width();
    const qreal h = height();
    const qreal pad = 20;
    const qreal top = 56;
    const qreal gap = 48;
    const qreal colW = (w - pad * 2 - gap) / 2.0;
    const qreal availH = h - top - 16;
    const qreal colH = qBound<qreal>(80.0, availH, 116.0);
    const qreal colY = top + (availH - colH) / 2.0;

    if (m_today == 0 && m_yesterday == 0) {
        p.setFont(DesignTokens::appFont(13));
        p.setPen(DesignTokens::kTextFaint());
        p.drawText(QRectF(0, top, w, h - top), Qt::AlignCenter,
                   QString::fromUtf8("\xe6\x9a\x82\xe6\x97\xa0\xe6\x95\xb0\xe6\x8d\xae"));
        return;
    }

    const qreal radius = 12;
    const struct { const char *label; int value; QColor bg; QColor bar; bool accent; } cols[2] = {
        {"\xe4\xbb\x8a\xe6\x97\xa5", m_today,
         DesignTokens::kCompareTodayBg(), DesignTokens::kAccent(), true},
        {"\xe6\x98\xa8\xe6\x97\xa5", m_yesterday,
         DesignTokens::kCompareYesterdayBg(), DesignTokens::kCompareYesterdayBar(), false}
    };

    for (int i = 0; i < 2; ++i) {
        const qreal x = pad + i * (colW + gap);
        QRectF r(x, colY, colW, colH);
        QPainterPath clip;
        clip.addRoundedRect(r, radius, radius);
        p.setClipPath(clip);
        p.fillRect(r, cols[i].bg);
        p.setClipping(false);
        p.setPen(QPen(DesignTokens::kBorder(), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r, radius, radius);

        // Eyebrow label.
        p.setFont(DesignTokens::eyebrowFont(11));
        p.setPen(cols[i].accent ? DesignTokens::kAccent() : DesignTokens::kTextMute());
        p.drawText(QRectF(x, colY + 10, colW, 20), Qt::AlignCenter,
                   QString::fromUtf8(cols[i].label));

        // Big number.
        p.setFont(DesignTokens::appFont(qMin(26, qRound(colW * 0.2)), QFont::Bold));
        p.setPen(DesignTokens::kTextStrong());
        p.drawText(QRectF(x, colY + 32, colW, 36), Qt::AlignCenter,
                   UiUtils::formatDuration(cols[i].value));

        // Progress bar.
        const qreal barY = colY + colH - 20;
        const qreal barH = 5;
        QRectF barRect(x + 14, barY, colW - 28, barH);
        p.setPen(Qt::NoPen);
        p.setBrush(DesignTokens::kProgressBg());
        p.drawRoundedRect(barRect, barH / 2.0, barH / 2.0);

        const int maxVal = qMax(m_today, m_yesterday);
        if (maxVal > 0 && cols[i].value > 0) {
            const qreal ratio = static_cast<qreal>(cols[i].value) / maxVal;
            QRectF fillRect(barRect.x(), barRect.y(), barRect.width() * ratio, barH);
            p.setBrush(cols[i].bar);
            p.drawRoundedRect(fillRect, barH / 2.0, barH / 2.0);
        }
    }
}
