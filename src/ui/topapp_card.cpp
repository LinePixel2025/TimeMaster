#include "ui/topapp_card.h"
#include "ui/design_tokens.h"

#include <QVBoxLayout>

static QString formatHM(int s)
{
    int totalMinutes = qMax(0, s) / 60;
    int h = totalMinutes / 60;
    int m = totalMinutes % 60;
    if (h > 0 && m > 0) return QString("%1h%2m").arg(h).arg(m, 2, 10, QLatin1Char('0'));
    if (h > 0)          return QString("%1h" ).arg(h);
    if (m > 0)          return QString("%1m" ).arg(m);
    return QStringLiteral("0m");
}

TopAppCard::TopAppCard(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 16, 0, 12);
    layout->setAlignment(Qt::AlignCenter);

    m_eyebrowLabel = new QLabel(QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe6\x9c\x80\xe5\xb8\xb8\xe7\x94\xa8"), this);
    m_eyebrowLabel->setFont(DesignTokens::eyebrowFont(11));
    m_eyebrowLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;").arg(DesignTokens::kTextFaint().name()));
    m_eyebrowLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_eyebrowLabel);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(40, 40);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setStyleSheet(QStringLiteral("background: transparent;"));
    layout->addWidget(m_iconLabel, 0, Qt::AlignCenter);

    m_nameLabel = new QLabel(QString::fromUtf8("\xe6\x9a\x82\xe6\x97\xa0\xe6\x95\xb0\xe6\x8d\xae"), this);
    m_nameLabel->setFont(DesignTokens::appFont(18, QFont::Medium));
    m_nameLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;").arg(DesignTokens::kTextStrong().name()));
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setWordWrap(true);
    layout->addWidget(m_nameLabel);

    m_timeLabel = new QLabel(this);
    m_timeLabel->setFont(DesignTokens::appFont(15, QFont::Medium));
    m_timeLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;").arg(DesignTokens::kAccent().name()));
    m_timeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_timeLabel);
}

void TopAppCard::setApp(const QString &name, int seconds, const QIcon &icon)
{
    m_nameLabel->setText(name);
    m_timeLabel->setText(formatHM(seconds));
    if (!icon.isNull()) {
        m_iconLabel->setPixmap(icon.pixmap(40, 40));
    }
}
