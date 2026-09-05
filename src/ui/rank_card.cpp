#include "ui/rank_card.h"
#include "icon/app_icon_provider.h"
#include "ui/design_tokens.h"
#include "ui/rank_layout.h"
#include "ui/theme_manager.h"
#include "ui/ui_utils.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
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

        if (m_item.isGroup) {
            // 组别没有单一进程可取图标，用首字色块替代，保持与应用行同样的缩进。
            painter.setPen(Qt::NoPen);
            painter.setBrush(DesignTokens::kAccent());
            painter.drawRoundedRect(QRectF(iconX, 1, iconSize, iconSize), 5.0, 5.0);
            painter.setFont(DesignTokens::appFont(10, QFont::Medium));
            painter.setPen(DesignTokens::readableTextOn(DesignTokens::kAccent()));
            const QString initial = m_item.appName.isEmpty()
                ? QString() : QString(m_item.appName.front());
            painter.drawText(QRectF(iconX, 1, iconSize, iconSize), Qt::AlignCenter, initial);
        } else if (!m_icon.isNull()) {
            m_icon.paint(&painter, QRectF(iconX, 1, iconSize, iconSize).toRect());
        }

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
    // 同主窗口：透明规则用 id 选择器，避免级联到 QToolTip。
    m_scrollArea->viewport()->setObjectName(QStringLiteral("rankViewport"));
    m_scrollArea->viewport()->setStyleSheet(
        QStringLiteral("#rankViewport { background: transparent; }"));

    m_listWidget = new QWidget();
    m_listWidget->setObjectName(QStringLiteral("rankListWidget"));
    m_listWidget->setStyleSheet(
        QStringLiteral("#rankListWidget { background: transparent; }"));
    m_listLayout = new QVBoxLayout(m_listWidget);
    m_listLayout->setContentsMargins(0, 0, 3, 0);
    m_listLayout->setSpacing(DesignTokens::kRankRowSpacing);
    m_scrollArea->setWidget(m_listWidget);

    // 头部：标题 + 应用/组别切换。按钮默认隐藏，setData 收到组别数据后才显示。
    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(2);
    QLabel *title = titleLabel();
    contentLayout()->removeWidget(title);
    header->addWidget(title);
    header->addStretch();

    m_appBtn = new QPushButton(QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8"), this);
    m_groupBtn = new QPushButton(QString::fromUtf8("\xe7\xbb\x84\xe5\x88\xab"), this);
    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);
    for (QPushButton *button : {m_appBtn, m_groupBtn}) {
        button->setCursor(Qt::PointingHandCursor);
        button->setCheckable(true);
        button->setFixedHeight(DesignTokens::kToggleButtonHeight);
        button->setStyleSheet(toggleStyle(button));
        button->hide();
    }
    m_modeGroup->addButton(m_appBtn, int(Mode::Apps));
    m_modeGroup->addButton(m_groupBtn, int(Mode::Groups));
    m_appBtn->setChecked(true);
    connect(m_modeGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id) {
        setMode(static_cast<Mode>(id));
    });

    header->addWidget(m_appBtn);
    header->addWidget(m_groupBtn);
    contentLayout()->insertLayout(0, header);
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
                // 切换按钮与组别色块都取 accent/文本 token，主题切换后需重设。
                m_appBtn->setStyleSheet(toggleStyle(m_appBtn));
                m_groupBtn->setStyleSheet(toggleStyle(m_groupBtn));
            });
    connect(ThemeManager::instance(), &ThemeManager::accentChanged,
            this, [this]() {
                updateToggleStyles();
                update();
            });
}

void RankCard::refresh(const QVector<QVariantMap> &rankData)
{
    m_appData = rankData;
    m_groupData.clear();
    // 无组别数据时用户无从切换，隐藏按钮并锁定应用模式。
    m_appBtn->hide();
    m_groupBtn->hide();
    m_updating = true;
    m_appBtn->setChecked(true);
    m_updating = false;
    m_mode = Mode::Apps;
    setTitle(QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe4\xbd\xbf\xe7\x94\xa8\xe6\x8e\x92\xe8\xa1\x8c"));
    renderCurrent();
}

void RankCard::setData(const QVector<QVariantMap> &appData,
                       const QVector<QVariantMap> &groupData)
{
    m_appData = appData;
    m_groupData = groupData;
    const bool hasGroups = !groupData.isEmpty();
    m_appBtn->setVisible(hasGroups);
    m_groupBtn->setVisible(hasGroups);
    if (!hasGroups && m_mode != Mode::Apps) {
        m_updating = true;
        m_appBtn->setChecked(true);
        m_updating = false;
        m_mode = Mode::Apps;
    }
    updateToggleStyles();
    renderCurrent();
}

void RankCard::setMode(Mode mode)
{
    if (m_updating || m_mode == mode)
        return;
    m_mode = mode;
    updateToggleStyles();
    renderCurrent();
}

void RankCard::updateToggleStyles()
{
    setTitle(m_mode == Mode::Groups
        ? QString::fromUtf8("\xe7\xbb\x84\xe5\x88\xab\xe4\xbd\xbf\xe7\x94\xa8\xe6\x8e\x92\xe8\xa1\x8c")
        : QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe4\xbd\xbf\xe7\x94\xa8\xe6\x8e\x92\xe8\xa1\x8c"));
    m_appBtn->setStyleSheet(toggleStyle(m_appBtn));
    m_groupBtn->setStyleSheet(toggleStyle(m_groupBtn));
}

QString RankCard::toggleStyle(QPushButton *button) const
{
    if (button->isChecked()) {
        return QStringLiteral(
            "QPushButton { border: none; border-radius: 6px; padding: 0 8px;"
            " font-size: 11px; color: %1; background: %2; }")
            .arg(DesignTokens::kOnAccent().name(), DesignTokens::kAccent().name())
            + UiUtils::focusBorderRule();
    }
    return QStringLiteral(
        "QPushButton { border: none; border-radius: 6px; padding: 0 8px;"
        " font-size: 11px; color: %1; background: transparent; }"
        "QPushButton:hover { color: %3; background: %2; }")
        .arg(DesignTokens::kTextMute().name(),
             DesignTokens::kButtonHoverBg().name(QColor::HexArgb),
             DesignTokens::kText().name())
        + UiUtils::focusBorderRule();
}

void RankCard::renderCurrent()
{
    while (m_listLayout->count() > 0) {
        QLayoutItem *item = m_listLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const QVector<RankLayout::Item> items =
        RankLayout::normalize(m_mode == Mode::Groups ? m_groupData : m_appData);
    if (items.isEmpty()) {
        const QString what = m_mode == Mode::Groups
            ? QString::fromUtf8("组别") : QString::fromUtf8("应用");
        auto *empty = new QLabel(
            QString::fromUtf8("今日暂无%1使用记录").arg(what), m_listWidget);
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
        const QIcon icon = item.isGroup
            ? QIcon()
            : AppIconProvider::instance()->icon(item.processName, 18);
        m_listLayout->addWidget(new RankItem(item, icon, m_listWidget));
    }

    if (contentHeight < DesignTokens::kRankListHeight)
        m_listLayout->addStretch(1);
}
