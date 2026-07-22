#include "rank_card.h"
#include "icon/app_icon_provider.h"
#include "ui/design_tokens.h"

#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

// ===================== AppRankItem =====================

AppRankItem::AppRankItem(int rank, const QString &appName, int totalSeconds,
                         const QIcon &icon, QWidget *parent)
    : QWidget(parent), m_rank(rank), m_appName(appName),
      m_totalSeconds(totalSeconds), m_icon(icon)
{
    setFixedHeight(48);
}

void AppRankItem::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal h = height();
    const qreal w = width();

    // --- Rank badge ---
    constexpr qreal badgeSize = 26.0;
    constexpr qreal badgeX = 16.0;
    const qreal badgeY = (h - badgeSize) / 2.0;
    const QRectF badgeRect(badgeX, badgeY, badgeSize, badgeSize);

    if (m_rank >= 1 && m_rank <= 3) {
        QRadialGradient medalGrad(badgeRect.center(), badgeSize / 2.0);
        QColor textColor;
        switch (m_rank) {
        case 1: // Gold
            medalGrad.setColorAt(0.0, QColor("#FFE066"));
            medalGrad.setColorAt(1.0, QColor("#FFB800"));
            textColor = Qt::white;
            break;
        case 2: // Silver
            medalGrad.setColorAt(0.0, QColor("#E8E8E8"));
            medalGrad.setColorAt(1.0, QColor("#B0B0B0"));
            textColor = QColor("#4A4A4A");
            break;
        case 3: // Bronze
            medalGrad.setColorAt(0.0, QColor("#D49972"));
            medalGrad.setColorAt(1.0, QColor("#A55C2C"));
            textColor = Qt::white;
            break;
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(medalGrad);
        painter.drawEllipse(badgeRect);

        painter.setFont(DesignTokens::appFont(12, QFont::Medium));
        painter.setPen(textColor);
        painter.drawText(badgeRect, Qt::AlignCenter, QString::number(m_rank));
    } else {
        // Rank 4+ — gray circle
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 20));
        painter.drawEllipse(badgeRect);

        painter.setFont(DesignTokens::appFont(12, QFont::Medium));
        painter.setPen(DesignTokens::kTextMute());
        painter.drawText(badgeRect, Qt::AlignCenter, QString::number(m_rank));
    }

    // --- App icon ---
    constexpr qreal iconSize = 24.0;
    constexpr qreal iconX = 48.0;
    const qreal iconY = (h - iconSize) / 2.0;

    if (!m_icon.isNull()) {
        m_icon.paint(&painter,
                     QRect(qRound(iconX), qRound(iconY),
                           qRound(iconSize), qRound(iconSize)));
    }

    // --- App name (elided) ---
    constexpr qreal nameX = 80.0;
    constexpr qreal rightMargin = 16.0;
    constexpr qreal timeWidth = 70.0;
    const qreal timeX = w - rightMargin - timeWidth;
    const qreal nameWidth = qMax<qreal>(0.0, timeX - nameX - 8.0);

    const QFont nameFont = DesignTokens::appFont(13);
    painter.setFont(nameFont);
    painter.setPen(DesignTokens::kText());
    const QString displayName = QFontMetrics(nameFont).elidedText(
        m_appName, Qt::ElideRight, qRound(nameWidth));
    painter.drawText(QRectF(nameX, 0, nameWidth, h),
                     Qt::AlignLeft | Qt::AlignVCenter, displayName);

    // --- Time ---
    const int minutes = m_totalSeconds / 60;
    const int hours = minutes / 60;
    const int remMins = minutes % 60;
    const QString timeText = hours > 0
        ? QString("%1h%2m").arg(hours).arg(remMins)
        : QString("%1m").arg(minutes);

    painter.setFont(DesignTokens::appFont(12, QFont::Medium));
    painter.setPen(DesignTokens::kTextStrong());
    painter.drawText(QRectF(timeX, 0, timeWidth, h),
                     Qt::AlignRight | Qt::AlignVCenter, timeText);
}

// ===================== RankCard =====================

RankCard::RankCard(QWidget *parent)
    : QFrame(parent)
{
    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(16, 16, 16, 16);
    outerLayout->setSpacing(12);

    // --- Title row ---
    QWidget *titleRow = new QWidget(this);
    QHBoxLayout *titleLayout = new QHBoxLayout(titleRow);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(8);

    auto *accentDot = new QWidget(titleRow);
    accentDot->setFixedSize(6, 6);
    accentDot->setStyleSheet(
        QString("background-color: %1; border-radius: 3px;")
            .arg(DesignTokens::kAccent().name()));

    auto *titleLabel = new QLabel(
        QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe4\xbd\xbf\xe7\x94\xa8\xe6\x8e\x92\xe8\xa1\x8c"
                          " (\xe4\xbb\x8a\xe6\x97\xa5)"),
        titleRow);
    titleLabel->setFont(DesignTokens::appFont(14, QFont::Medium));
    titleLabel->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(DesignTokens::kTextStrong().name()));

    titleLayout->addWidget(accentDot, 0, Qt::AlignVCenter);
    titleLayout->addWidget(titleLabel, 0, Qt::AlignVCenter);
    titleLayout->addStretch();
    outerLayout->addWidget(titleRow);

    // --- Scroll area ---
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(
        QString(
            "QScrollArea { background: transparent; border: none; }"
            "QScrollBar:vertical { background: %1; width: 6px; margin: 0; border: none; }"
            "QScrollBar::handle:vertical { background: %2; min-height: 24px; border-radius: 3px; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
            .arg(DesignTokens::kBorder().name(QColor::HexArgb))
            .arg(DesignTokens::kTextFaint().name()));
    scrollArea->viewport()->setStyleSheet("background: transparent;");

    m_listWidget = new QWidget();
    m_listWidget->setStyleSheet("background: transparent;");
    m_listLayout = new QVBoxLayout(m_listWidget);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(8);
    m_listLayout->addStretch();

    scrollArea->setWidget(m_listWidget);
    outerLayout->addWidget(scrollArea);
}

void RankCard::refresh(const QVector<QVariantMap> &rankData)
{
    // Remove all items (keep the trailing stretch)
    while (m_listLayout->count() > 1) {
        QLayoutItem *item = m_listLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (rankData.isEmpty()) {
        auto *emptyLabel = new QLabel(
            QString::fromUtf8("\xe6\x9a\x82\xe6\x97\xa0\xe6\x95\xb0\xe6\x8d\xae"),
            m_listWidget);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setFont(DesignTokens::appFont(13));
        emptyLabel->setStyleSheet(
            QString("color: %1; background: transparent; padding: 32px 0;")
                .arg(DesignTokens::kTextFaint().name()));
        m_listLayout->insertWidget(0, emptyLabel);
        return;
    }

    for (int i = 0; i < rankData.size(); ++i) {
        const QString processName = rankData[i]["process_name"].toString();
        const QIcon icon = AppIconProvider::instance()->icon(processName, 24);
        auto *item = new AppRankItem(
            i + 1,
            rankData[i]["app_name"].toString(),
            rankData[i]["total_seconds"].toInt(),
            icon,
            m_listWidget);
        m_listLayout->insertWidget(i, item);
    }
}
