#include "ui/rank_card.h"
#include "icon/app_icon_provider.h"
#include "ui/design_tokens.h"
#include "ui/rank_layout.h"
#include "ui/theme_manager.h"
#include "ui/ui_utils.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

namespace {

class RankItem : public QWidget
{
public:
    RankItem(const RankLayout::Item &item, const QIcon &icon, QWidget *parent)
        : QWidget(parent), m_item(item), m_icon(icon)
    {
        setFixedHeight(DesignTokens::kRankRowHeight);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const qreal h = height();
        const qreal w = width();
        constexpr qreal badgeSize = 20.0;
        constexpr qreal badgeX = 0.0;
        constexpr qreal iconSize = 18.0;
        constexpr qreal iconX = badgeX + badgeSize + 8.0;
        const qreal nameX = iconX + iconSize + 7.0;
        const bool compact = w < 250.0;
        const qreal durationW = compact ? 42.0 : 54.0;
        const qreal percentW = compact ? 24.0 : 30.0;
        const qreal detailW = durationW + percentW + 4.0;
        const qreal nameW = qMax<qreal>(0.0, w - nameX - detailW - 4.0);
        const qreal barY = h - DesignTokens::kRankProgressHeight;

        if (m_item.rank <= 3) {
            const QColor colors[3] = {
                QColor("#FFC53D"), QColor("#C0C4CC"), QColor("#E6A23C")
            };
            const QColor badgeColor = colors[m_item.rank - 1];
            painter.setPen(Qt::NoPen);
            painter.setBrush(badgeColor);
            painter.drawEllipse(QRectF(badgeX, 0, badgeSize, badgeSize));
            painter.setFont(DesignTokens::appFont(10, QFont::Medium));
            painter.setPen(DesignTokens::readableTextOn(badgeColor));
            painter.drawText(QRectF(badgeX, 0, badgeSize, badgeSize), Qt::AlignCenter,
                             QString::number(m_item.rank));
        } else {
            painter.setFont(DesignTokens::monoFont(10, QFont::Medium));
            painter.setPen(DesignTokens::kTextMute());
            painter.drawText(QRectF(badgeX, 0, badgeSize, badgeSize), Qt::AlignCenter,
                             QString::number(m_item.rank));
        }

        if (!m_icon.isNull())
            m_icon.paint(&painter, QRectF(iconX, 1, iconSize, iconSize).toRect());

        if (nameW > 0.0) {
            painter.setFont(DesignTokens::appFont(compact ? 11 : 12));
            painter.setPen(DesignTokens::kText());
            const QString displayName = QFontMetrics(painter.font()).elidedText(
                m_item.appName, Qt::ElideRight, qRound(nameW));
            painter.drawText(QRectF(nameX, 0, nameW, 18),
                             Qt::AlignLeft | Qt::AlignVCenter, displayName);
        }

        painter.setFont(DesignTokens::monoFont(compact ? 9 : 10, QFont::Medium));
        painter.setPen(DesignTokens::kTextStrong());
        painter.drawText(QRectF(w - detailW, 0, durationW, 18),
                         Qt::AlignRight | Qt::AlignVCenter,
                         UiUtils::formatRankDuration(m_item.seconds));
        painter.setFont(DesignTokens::appFont(compact ? 9 : 10));
        painter.setPen(DesignTokens::kTextMute());
        painter.drawText(QRectF(w - percentW, 0, percentW, 18), Qt::AlignRight | Qt::AlignVCenter,
                         QStringLiteral("%1%").arg(m_item.sharePercent));

        if (w > nameX) {
            const qreal progressWidth = w - nameX;
            painter.setPen(Qt::NoPen);
            painter.setBrush(DesignTokens::kProgressBg());
            painter.drawRoundedRect(QRectF(nameX, barY, progressWidth,
                                            DesignTokens::kRankProgressHeight), 2.0, 2.0);
            if (m_item.share > 0.0) {
                painter.setBrush(DesignTokens::kAccent());
                painter.drawRoundedRect(QRectF(nameX, barY, progressWidth * m_item.share,
                                                DesignTokens::kRankProgressHeight),
                                        2.0, 2.0);
            }
        }
    }

private:
    RankLayout::Item m_item;
    QIcon m_icon;
};

} // namespace

RankCard::RankCard(QWidget *parent)
    : CardFrame(QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe4\xbd\xbf\xe7\x94\xa8\xe6\x8e\x92\xe8\xa1\x8c"), parent)
{
    setMinimumHeight(DesignTokens::kRankMinHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFixedHeight(DesignTokens::kRankListHeight);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet(
        QString("QScrollArea { background: transparent; border: none; }"
                "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
                "QScrollBar::handle:vertical { background: %1; min-height: 24px; border-radius: 3px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
            .arg(DesignTokens::kTextFaint().name()));
    m_scrollArea->viewport()->setStyleSheet("background: transparent;");

    m_listWidget = new QWidget();
    m_listWidget->setStyleSheet("background: transparent;");
    m_listLayout = new QVBoxLayout(m_listWidget);
    m_listLayout->setContentsMargins(0, 0, 3, 0);
    m_listLayout->setSpacing(DesignTokens::kRankRowSpacing);
    m_scrollArea->setWidget(m_listWidget);
    contentLayout()->addWidget(m_scrollArea);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) {
                m_scrollArea->setStyleSheet(
                    QString("QScrollArea { background: transparent; border: none; }"
                            "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
                            "QScrollBar::handle:vertical { background: %1; min-height: 24px; border-radius: 3px; }"
                            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
                        .arg(DesignTokens::kTextFaint().name()));
    });
}

void RankCard::refresh(const QVector<QVariantMap> &rankData)
{
    while (m_listLayout->count() > 0) {
        QLayoutItem *item = m_listLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const QVector<RankLayout::Item> items = RankLayout::normalize(rankData);
    if (items.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("今日暂无应用使用记录"), m_listWidget);
        empty->setAlignment(Qt::AlignCenter);
        empty->setFont(DesignTokens::appFont(12));
        empty->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        empty->setStyleSheet(QString("color: %1; background: transparent;")
            .arg(DesignTokens::kTextPlaceholder().name()));
        m_listLayout->addWidget(empty, 1);
        return;
    }

    const int contentHeight = items.size() * DesignTokens::kRankRowHeight
        + qMax(0, items.size() - 1) * DesignTokens::kRankRowSpacing;
    if (contentHeight < DesignTokens::kRankListHeight)
        m_listLayout->addStretch(1);

    for (const RankLayout::Item &item : items) {
        const QIcon icon = AppIconProvider::instance()->icon(item.processName, 18);
        m_listLayout->addWidget(new RankItem(item, icon, m_listWidget));
    }

    if (contentHeight < DesignTokens::kRankListHeight)
        m_listLayout->addStretch(1);
}
