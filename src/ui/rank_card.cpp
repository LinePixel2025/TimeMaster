#include "ui/rank_card.h"
#include "icon/app_icon_provider.h"
#include "ui/design_tokens.h"
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
    RankItem(int rank, const QString &appName, int totalSeconds,
             const QIcon &icon, QWidget *parent)
        : QWidget(parent), m_rank(rank), m_appName(appName),
          m_totalSeconds(totalSeconds), m_icon(icon)
    {
        setFixedHeight(44);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const qreal h = height();
        const qreal w = width();

        // Rank badge.
        const qreal badgeSize = 24;
        const qreal badgeX = 12;
        const QRectF badgeRect(badgeX, (h - badgeSize) / 2.0, badgeSize, badgeSize);
        painter.setPen(Qt::NoPen);
        if (m_rank <= 3) {
            const QColor colors[3] = {
                QColor("#FFC53D"), QColor("#C0C4CC"), QColor("#E6A23C")
            };
            painter.setBrush(colors[m_rank - 1]);
        } else {
            painter.setBrush(DesignTokens::kButtonHoverBg());
        }
        painter.drawEllipse(badgeRect);

        painter.setFont(DesignTokens::appFont(11, QFont::Medium));
        painter.setPen(m_rank <= 3 ? Qt::white : DesignTokens::kTextMute());
        painter.drawText(badgeRect, Qt::AlignCenter, QString::number(m_rank));

        // App icon.
        const qreal iconSize = 22;
        const qreal iconX = badgeX + badgeSize + 10;
        if (!m_icon.isNull()) {
            m_icon.paint(&painter, QRectF(iconX, (h - iconSize) / 2.0,
                                          iconSize, iconSize).toRect());
        }

        // Name.
        const qreal nameX = iconX + (m_icon.isNull() ? 0 : iconSize + 8);
        const qreal timeW = 64;
        const qreal nameW = qMax<qreal>(40, w - nameX - timeW - 24);
        painter.setFont(DesignTokens::appFont(13));
        painter.setPen(DesignTokens::kText());
        const QString displayName = QFontMetrics(DesignTokens::appFont(13))
            .elidedText(m_appName, Qt::ElideRight, qRound(nameW));
        painter.drawText(QRectF(nameX, 0, nameW, h),
                         Qt::AlignLeft | Qt::AlignVCenter, displayName);

        // Time.
        painter.setFont(DesignTokens::appFont(13, QFont::Medium));
        painter.setPen(DesignTokens::kTextStrong());
        painter.drawText(QRectF(w - timeW - 16, 0, timeW, h),
                         Qt::AlignRight | Qt::AlignVCenter,
                         UiUtils::formatDuration(m_totalSeconds));
    }

private:
    int m_rank;
    QString m_appName;
    int m_totalSeconds;
    QIcon m_icon;
};

} // namespace

RankCard::RankCard(QWidget *parent)
    : CardFrame(QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe4\xbd\xbf\xe7\x94\xa8\xe6\x8e\x92\xe8\xa1\x8c"), parent)
{
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
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
    m_listLayout->setContentsMargins(0, 4, 8, 0);
    m_listLayout->setSpacing(2);
    m_listLayout->addStretch();

    m_scrollArea->setWidget(m_listWidget);
    contentLayout()->addWidget(m_scrollArea, 1);

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
    while (m_listLayout->count() > 1) {
        QLayoutItem *item = m_listLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (rankData.isEmpty()) {
        auto *empty = new QLabel(
            QString::fromUtf8("\xe6\x9a\x82\xe6\x97\xa0\xe6\x95\xb0\xe6\x8d\xae"),
            m_listWidget);
        empty->setAlignment(Qt::AlignCenter);
        empty->setFont(DesignTokens::appFont(13));
        empty->setStyleSheet(
            QString("color: %1; background: transparent; padding: 24px 0;")
                .arg(DesignTokens::kTextFaint().name()));
        m_listLayout->insertWidget(0, empty);
        return;
    }

    for (int i = 0; i < rankData.size(); ++i) {
        const QString processName = rankData[i][QStringLiteral("process_name")].toString();
        const QIcon icon = AppIconProvider::instance()->icon(processName, 22);
        auto *item = new RankItem(
            i + 1,
            rankData[i][QStringLiteral("app_name")].toString(),
            rankData[i][QStringLiteral("total_seconds")].toInt(),
            icon,
            m_listWidget);
        m_listLayout->insertWidget(i, item);
    }
}
