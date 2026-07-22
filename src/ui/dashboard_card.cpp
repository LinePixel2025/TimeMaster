#include "ui/dashboard_card.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QBitmap>
#include <QResizeEvent>

DashboardCard::DashboardCard(const QString &id, QWidget *parent)
    : QFrame(parent), m_cardId(id)
{
    setStyleSheet(
        QString("DashboardCard { background-color: %1; }")
            .arg(DesignTokens::kSurface().name()));
    setAutoFillBackground(false);
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(20, 12, 20, 16);
    m_layout->setSpacing(12);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) {
        setStyleSheet(
            QString("DashboardCard { background-color: %1; }")
                .arg(DesignTokens::kSurface().name()));
        if (m_titleLabel)
            m_titleLabel->setStyleSheet(
                QString("color:%1;background:transparent;")
                    .arg(DesignTokens::kTextStrong().name()));
        update();
    });
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

    QPainterPath bg;
    bg.addRoundedRect(rect(), DesignTokens::kRadiusCard,
                      DesignTokens::kRadiusCard);
    p.fillPath(bg, DesignTokens::kSurface());

    p.setPen(QPen(DesignTokens::kCardBorder(), 1));
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
    if (!m_titleLabel) {
        m_titleLabel = new QLabel(t, this);
        m_titleLabel->setFont(DesignTokens::appFont(14, QFont::Medium));
        m_titleLabel->setStyleSheet(
            QString("color:%1;background:transparent;")
                .arg(DesignTokens::kTextStrong().name()));
        m_layout->insertWidget(0, m_titleLabel);
    } else {
        m_titleLabel->setText(t);
    }
}

void DashboardCard::setContentWidget(QWidget *w) {
    if (m_contentWidget) { m_layout->removeWidget(m_contentWidget); m_contentWidget->deleteLater(); }
    m_contentWidget = w;
    if (w) m_layout->addWidget(w, 1);
}
void DashboardCard::mousePressEvent(QMouseEvent *e) { emit clicked(); QFrame::mousePressEvent(e); }
