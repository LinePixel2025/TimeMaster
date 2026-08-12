#include "ui/compare_card.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"
#include "ui/ui_utils.h"

#include <QPainter>
#include <QVBoxLayout>

namespace {

QString deltaText(int deltaSeconds, int changePercent, bool hasYesterday)
{
    if (!hasYesterday)
        return QStringLiteral("昨日暂无数据");

    const QString arrow = deltaSeconds < 0 ? QStringLiteral("↓") : QStringLiteral("↑");
    const QString duration = UiUtils::formatDuration(qAbs(deltaSeconds));
    return QStringLiteral("%1 %2 · %3%").arg(arrow, duration)
        .arg(qAbs(changePercent));
}

} // namespace

class CompareArea : public QWidget
{
public:
    explicit CompareArea(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(120);
        connect(ThemeManager::instance(), &ThemeManager::themeChanged,
                this, [this](ThemeManager::Theme) { update(); });
    }

    void setData(int todaySeconds, int yesterdaySeconds)
    {
        m_today = todaySeconds;
        m_yesterday = yesterdaySeconds;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);

        const qreal w = width();
        const qreal h = height();
        if (m_today == 0 && m_yesterday == 0) {
            p.setFont(DesignTokens::appFont(13));
            p.setPen(DesignTokens::kTextFaint());
            p.drawText(rect(), Qt::AlignCenter, QStringLiteral("暂无数据"));
            return;
        }

        const bool hasYesterday = m_yesterday > 0;
        const int delta = m_today - m_yesterday;
        const int percent = UiUtils::percentChange(m_today, m_yesterday);

        // A compact conclusion sits above the values; the values remain the
        // strongest visual element in the card.
        p.setFont(DesignTokens::appFont(12));
        p.setPen(DesignTokens::kTextMute());
        p.drawText(QRectF(0, 2, w, 18), Qt::AlignCenter, QStringLiteral("相较昨日"));

        p.setFont(DesignTokens::appFont(16, QFont::DemiBold));
        p.setPen(delta <= 0 ? DesignTokens::kAccent() : DesignTokens::kError());
        p.drawText(QRectF(0, 19, w, 26), Qt::AlignCenter,
                   deltaText(delta, percent, hasYesterday));

        const qreal centerX = w / 2.0;
        const qreal valueY = 58;
        const qreal labelH = 20;
        const qreal valueH = qMax<qreal>(42, h - valueY - 4);
        const qreal leftCenter = w * 0.27;
        const qreal rightCenter = w * 0.73;

        p.setPen(QPen(DesignTokens::kSeparator(), 1));
        p.drawLine(QPointF(centerX, valueY), QPointF(centerX, valueY + valueH));

        p.setFont(DesignTokens::eyebrowFont(12));
        p.setPen(DesignTokens::kAccent());
        p.drawText(QRectF(leftCenter - 70, valueY, 140, labelH),
                   Qt::AlignCenter, QStringLiteral("今日"));
        p.setPen(DesignTokens::kTextMute());
        p.drawText(QRectF(rightCenter - 70, valueY, 140, labelH),
                   Qt::AlignCenter, QStringLiteral("昨日"));

        const QString todayText = UiUtils::formatDuration(m_today);
        const QString yesterdayText = UiUtils::formatDuration(m_yesterday);
        const int maxTextWidth = qMax(40, qRound(centerX - 30));
        const QFont baseFont = DesignTokens::appFont(38, QFont::Bold);
        int valueFontSize = 38;
        QFontMetrics baseMetrics(baseFont);
        const int widest = qMax(baseMetrics.horizontalAdvance(todayText),
                                baseMetrics.horizontalAdvance(yesterdayText));
        if (widest > maxTextWidth)
            valueFontSize = qMax(22, qRound(38.0 * maxTextWidth / widest));
        // 高度兜底：卡片被压缩或高 DPI 时 38pt 数字行高超出可用高度，
        // 会溢出组件边框被裁切；按行高再收缩一次，保证大数字完整可见。
        const qreal valueRectH = valueH - labelH + 2;
        if (valueRectH >= 8) {
            const int heightBudget = qMax(1, qRound(valueRectH) - 2);
            const QFontMetrics scaledMetrics(
                DesignTokens::appFont(valueFontSize, QFont::Bold));
            if (scaledMetrics.height() > heightBudget)
                valueFontSize = qMax(16, qRound(static_cast<qreal>(valueFontSize)
                                                * heightBudget / scaledMetrics.height()));
        }
        p.setFont(DesignTokens::appFont(valueFontSize, QFont::Bold));
        p.setPen(DesignTokens::kTextStrong());
        p.drawText(QRectF(0, valueY + labelH - 2, centerX - 14, valueRectH),
                   Qt::AlignCenter, todayText);
        p.drawText(QRectF(centerX + 14, valueY + labelH - 2, centerX - 14, valueRectH),
                   Qt::AlignCenter, yesterdayText);
    }

private:
    int m_today = 0;
    int m_yesterday = 0;
};

CompareCard::CompareCard(QWidget *parent)
    : CardFrame(QStringLiteral("今日与昨日对比"), parent)
{
    m_area = new CompareArea(this);
    contentLayout()->addWidget(m_area, 1);
}

void CompareCard::setData(int todaySeconds, int yesterdaySeconds)
{
    if (m_area)
        m_area->setData(todaySeconds, yesterdaySeconds);
}
