#include "ui/dashboard_card.h"
#include "ui/design_tokens.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QBitmap>
#include <QResizeEvent>

DashboardCard::DashboardCard(const QString &id, QWidget *parent)
    : QFrame(parent), m_cardId(id)
{
    setStyleSheet("DashboardCard { background-color: #FFFFFF; }");
    setAutoFillBackground(false);
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(20, 12, 20, 16);
    m_layout->setSpacing(12);
}

void DashboardCard::updateClipMask()
{
    QBitmap mask(size());
    mask.fill(Qt::color0);
    {
        QPainter p(&mask);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(Qt::color1);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect(), DesignTokens::kRadiusCard, DesignTokens::kRadiusCard);
    }
    setMask(mask);
}

void DashboardCard::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    updateClipMask();
}

void DashboardCard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Fill rounded white background — corners stay transparent
    // so the wallpaper shows through instead of a square white block.
    QPainterPath bg;
    bg.addRoundedRect(rect(), DesignTokens::kRadiusCard,
                      DesignTokens::kRadiusCard);
    p.fillPath(bg, QColor("#FFFFFF"));

    // Overlay rounded border — does NOT clip children
    p.setPen(QPen(QColor(0, 0, 0, 20), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(0.5, 0.5, width() - 1.0, height() - 1.0,
                      DesignTokens::kRadiusCard, DesignTokens::kRadiusCard);
}

QSize DashboardCard::minimumSizeHint() const
{
    if (m_contentWidget)
        return m_contentWidget->minimumSizeHint() + QSize(40, 28);
    return QFrame::minimumSizeHint();
}

void DashboardCard::setCardTitle(const QString &t) {
    if (!m_titleLabel) { m_titleLabel = new QLabel(t, this); m_titleLabel->setFont(DesignTokens::appFont(14,QFont::Medium)); m_titleLabel->setStyleSheet("color:#111827;background:transparent;"); m_layout->insertWidget(0, m_titleLabel); }
    else m_titleLabel->setText(t);
}
void DashboardCard::setContentWidget(QWidget *w) {
    if (m_contentWidget) { m_layout->removeWidget(m_contentWidget); m_contentWidget->deleteLater(); }
    m_contentWidget = w;
    if (w) m_layout->addWidget(w, 1);
}
void DashboardCard::mousePressEvent(QMouseEvent *e) { emit clicked(); QFrame::mousePressEvent(e); }
