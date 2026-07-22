#include "ui/compare_card.h"
#include "ui/design_tokens.h"

#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>

static QString formatHM(int s)
{
    int totalMinutes = qMax(0, s) / 60;
    int h = totalMinutes / 60;
    int m = totalMinutes % 60;
    if (h > 0 && m > 0) return QString("%1h%2m").arg(h).arg(m);
    if (h > 0)          return QString("%1h" ).arg(h);
    if (m > 0)          return QString("%1m" ).arg(m);
    return QStringLiteral("0m");
}

CompareCard::CompareCard(QWidget *parent)
    : QWidget(parent)
{
}

void CompareCard::setData(int todaySeconds, int yesterdaySeconds)
{
    m_todaySeconds     = todaySeconds;
    m_yesterdaySeconds = yesterdaySeconds;
    update();
}

void CompareCard::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const qreal w = width();
    const qreal h = height();
    const qreal pad = 20;

    // — Empty state —
    if (m_todaySeconds == 0 && m_yesterdaySeconds == 0) {
        p.setFont(DesignTokens::appFont(14));
        p.setPen(DesignTokens::kTextFaint());
        p.drawText(QRectF(0, 0, w, h), Qt::AlignCenter,
                   QString::fromUtf8("\xe6\x9a\x82\xe6\x97\xa0\xe6\x95\xb0\xe6\x8d\xae"));
        return;
    }

    // — Layout —
    const qreal gap    = 36; // centre pill + spacing
    const qreal colW   = (w - pad * 2 - gap) / 2.0;
    const qreal colH   = qMin(h - 16.0, 100.0);
    const qreal colY   = (h - colH) / 2.0;
    const qreal leftX  = pad;
    const qreal rightX = leftX + colW + gap;
    const qreal radius = 12;

    // — Left column (今日) —
    {
        QRectF r(leftX, colY, colW, colH);
        QColor accentBg = DesignTokens::kAccent();
        accentBg.setAlpha(20); // ~8 %
        QPainterPath clip;
        clip.addRoundedRect(r, radius, radius);
        p.setClipPath(clip);
        p.fillRect(r, accentBg);
        p.setClipping(false);

        // border
        p.setPen(QPen(QColor(0, 0, 0, 8), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r, radius, radius);

        // eyebrow
        p.setFont(DesignTokens::eyebrowFont(11));
        p.setPen(DesignTokens::kAccent());
        p.drawText(QRectF(leftX, colY + 8, colW, 20), Qt::AlignCenter,
                   QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5"));

        // big number
        p.setFont(DesignTokens::appFont(qMin(28, qRound(colW * 0.2)), QFont::Bold));
        p.setPen(DesignTokens::kTextStrong());
        p.drawText(QRectF(leftX, colY + 28, colW, 36), Qt::AlignCenter,
                   formatHM(m_todaySeconds));

        // subtitle
        p.setFont(DesignTokens::appFont(12));
        p.setPen(DesignTokens::kTextMute());
        p.drawText(QRectF(leftX, colY + 66, colW, 18), Qt::AlignCenter,
                   QString::fromUtf8("\xe6\x80\xbb\xe6\x97\xb6\xe9\x95\xbf"));
    }

    // — Right column (昨日) —
    {
        QRectF r(rightX, colY, colW, colH);
        QPainterPath clip;
        clip.addRoundedRect(r, radius, radius);
        p.setClipPath(clip);
        p.fillRect(r, QColor(0, 0, 0, 4));
        p.setClipping(false);

        p.setPen(QPen(QColor(0, 0, 0, 8), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r, radius, radius);

        p.setFont(DesignTokens::eyebrowFont(11));
        p.setPen(DesignTokens::kTextMute());
        p.drawText(QRectF(rightX, colY + 8, colW, 20), Qt::AlignCenter,
                   QString::fromUtf8("\xe6\x98\xa8\xe6\x97\xa5"));

        p.setFont(DesignTokens::appFont(qMin(28, qRound(colW * 0.2)), QFont::Bold));
        p.setPen(DesignTokens::kTextStrong());
        p.drawText(QRectF(rightX, colY + 28, colW, 36), Qt::AlignCenter,
                   formatHM(m_yesterdaySeconds));

        p.setFont(DesignTokens::appFont(12));
        p.setPen(DesignTokens::kTextMute());
        p.drawText(QRectF(rightX, colY + 66, colW, 18), Qt::AlignCenter,
                   QString::fromUtf8("\xe6\x80\xbb\xe6\x97\xb6\xe9\x95\xbf"));
    }

    // — Centre pill —
    {
        bool hasYesterday = (m_yesterdaySeconds > 0);
        QString label;
        QColor  pillColor;

        if (hasYesterday) {
            int deltaPct = ((m_todaySeconds - m_yesterdaySeconds) * 100)
                           / qMax(1, m_yesterdaySeconds);
            if (deltaPct > 0) {
                label     = QString::fromUtf8("\xe2\x86\x91 %1%").arg(deltaPct);
                pillColor = DesignTokens::kSuccess();
            } else if (deltaPct < 0) {
                label     = QString::fromUtf8("\xe2\x86\x93 %1%").arg(-deltaPct);
                pillColor = DesignTokens::kError();
            } else {
                label     = QString::fromUtf8("\xe2\x80\x94 0%");
                pillColor = DesignTokens::kTextMute();
            }
        } else {
            label     = QString::fromUtf8("\xe6\x96\xb0\xe5\xa2\x9e");
            pillColor = DesignTokens::kAccent();
        }

        QFont pillFont = DesignTokens::appFont(12, QFont::Medium);
        p.setFont(pillFont);
        QFontMetricsF fm(pillFont);

        qreal labelW = fm.horizontalAdvance(label) + 18;
        qreal labelH = fm.height() + 8;
        qreal pillX  = leftX + colW + (gap - labelW) / 2.0;
        QRectF pillRect(pillX, colY + colH / 2.0 - labelH / 2.0, labelW, labelH);

        QColor bg = pillColor;
        bg.setAlpha(32);
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(pillRect, labelH / 2.0, labelH / 2.0);

        p.setPen(pillColor);
        p.setFont(pillFont);
        p.drawText(pillRect, Qt::AlignCenter, label);
    }
}
