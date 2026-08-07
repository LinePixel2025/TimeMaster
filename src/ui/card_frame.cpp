#include "ui/card_frame.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>

CardFrame::CardFrame(const QString &title, QWidget *parent)
    : QFrame(parent)
{
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_contentLayout = new QVBoxLayout();
    m_contentLayout->setContentsMargins(20, 18, 20, 20);
    m_contentLayout->setSpacing(12);

    if (!title.isEmpty())
        setTitle(title);

    outer->addLayout(m_contentLayout);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) {
        update();
        if (m_titleLabel) {
            m_titleLabel->setStyleSheet(
                QString("color: %1; background: transparent;")
                    .arg(DesignTokens::kTextStrong().name()));
        }
    });
}

CardFrame::CardFrame(QWidget *parent)
    : CardFrame(QString(), parent)
{
}

void CardFrame::setTitle(const QString &title)
{
    if (!m_titleLabel) {
        m_titleLabel = new QLabel(this);
        m_titleLabel->setFont(DesignTokens::appFont(15, QFont::Medium));
        m_titleLabel->setStyleSheet(
            QString("color: %1; background: transparent;")
                .arg(DesignTokens::kTextStrong().name()));
        m_contentLayout->insertWidget(0, m_titleLabel);
    }
    m_titleLabel->setText(title);
}

void CardFrame::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const QRectF rect(0.5, 0.5, width() - 1.0, height() - 1.0);
    QPainterPath path;
    path.addRoundedRect(rect, DesignTokens::kRadiusCard, DesignTokens::kRadiusCard);

    painter.setPen(QPen(DesignTokens::kCardBorder(), 1.0));
    painter.setBrush(DesignTokens::kSurface());
    painter.drawPath(path);
}
