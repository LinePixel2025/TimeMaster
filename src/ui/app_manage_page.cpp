#include "app_manage_page.h"

#include "ai/ai_client.h"
#include "icon/app_icon_provider.h"
#include "ui/design_tokens.h"
#include "ui/group_icons.h"
#include "ui/theme_manager.h"
#include "ui/toggle_switch.h"
#include "ui/ui_utils.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kIconSize = 24;
constexpr int kRowHeight = 44;
constexpr int kDetailLabelWidth = 82;
constexpr int kCardMarginH = 16;
constexpr int kCardMarginV = 14;

/// 组别下拉/列表的「未分组」占位 id（app_groups 的 id 恒为正数）。
constexpr int kNoGroupId = -1;

/// 应用列表默认隐藏累计时长低于该秒数的条目（开关可恢复显示）。
constexpr int kMinDisplaySeconds = 120;
constexpr int kGroupRowHeight = 36;

/// 「隐藏低时长应用（<2 分钟）」开关的持久化键。沿用旧名
/// app_manage_hide_placeholder（原「隐藏无记录进程」），保住用户已有选择。
const QString kHideLowUsageKey = QStringLiteral("app_manage_hide_placeholder");

// 应用列表 item 的 data role：把 AppEntry 字段平铺存储，供自绘 delegate 读取。
constexpr int kRoleKey = Qt::UserRole;          // processKey
constexpr int kRolePath = Qt::UserRole + 1;      // processName（取图标）
constexpr int kRoleName = Qt::UserRole + 2;      // displayName
constexpr int kRoleIgnored = Qt::UserRole + 3;   // bool
constexpr int kRoleMerged = Qt::UserRole + 4;    // QString mergedInto
constexpr int kRoleSeconds = Qt::UserRole + 5;   // int
constexpr int kRoleSessions = Qt::UserRole + 6;  // int
constexpr int kRolePlaceholder = Qt::UserRole + 7; // bool
constexpr int kRoleGroupId = Qt::UserRole + 8;   // int 所属组别 id，-1 未分组
constexpr int kRoleCollapsed = Qt::UserRole + 9; // bool 「未识别应用」折叠行
constexpr int kRolePidCount = Qt::UserRole + 10; // int 折叠行包含的 PID 应用数

// 组别列表 item 的 data role：供 GroupItemDelegate 自绘。
constexpr int kRoleGId = Qt::UserRole;           // int
constexpr int kRoleGIcon = Qt::UserRole + 1;     // QString emoji
constexpr int kRoleGName = Qt::UserRole + 2;     // QString
constexpr int kRoleGBuiltin = Qt::UserRole + 3;  // bool
constexpr int kRoleGMembers = Qt::UserRole + 4;  // int

QString ungroupedLabel()
{
    return QString::fromUtf8("未分组");
}

/// 是否存在任何真实会话：无记录（占位）条目只在追踪层取不到 exe 路径时出现。
bool isPlaceholder(const AppEntry &entry)
{
    return entry.totalSeconds == 0 && entry.sessionCount == 0;
}

/// PID 形式进程键：追踪层取不到可执行文件路径时记为 "pid_<数字>"
/// （window_tracker.cpp 的 processName 兜底）。
bool isPidKey(const QString &key)
{
    return key.startsWith(QLatin1String("pid_"), Qt::CaseInsensitive);
}

/// PID 应用的展示名：用户改过名（别名）就显示别名，否则显示「未知进程（编号）」。
/// classifyApp 的兜底名（"Pid_12345"）同样按未识别处理。
QString pidDisplayName(const AppEntry &entry)
{
    if (!entry.displayName.isEmpty()
        && !entry.displayName.startsWith(QLatin1String("pid_"), Qt::CaseInsensitive))
        return entry.displayName;
    return QString::fromUtf8("未知进程（%1）").arg(entry.processKey.mid(4));
}

/// 应用列表行自绘 delegate：图标 + 显示名 + 状态芯片 + 右侧等宽时长与会话数，
/// 仿仪表盘 RankCard 的 RankItem 绘制模式，但保留 QListWidget 的选中/悬停/滚动。
class AppItemDelegate : public QStyledItemDelegate
{
public:
    explicit AppItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return QSize(0, kRowHeight);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::TextAntialiasing);

        const QRect r = option.rect;
        const bool selected = option.state & QStyle::State_Selected;
        const bool hover = option.state & QStyle::State_MouseOver;

        // 整行圆角背景：选中用 accentLight，悬停用 hoverBg，平时透明。
        if (selected) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(DesignTokens::kAccentLight());
            painter->drawRoundedRect(QRectF(r).adjusted(1, 1, -1, -1), 6.0, 6.0);
        } else if (hover) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(DesignTokens::kButtonHoverBg());
            painter->drawRoundedRect(QRectF(r).adjusted(1, 1, -1, -1), 6.0, 6.0);
        }

        const QString processKey = index.data(kRoleKey).toString();
        const QString displayName = index.data(kRoleName).toString();
        const bool ignored = index.data(kRoleIgnored).toBool();
        const QString mergedInto = index.data(kRoleMerged).toString();
        const int seconds = index.data(kRoleSeconds).toInt();
        const int sessions = index.data(kRoleSessions).toInt();
        const bool placeholder = index.data(kRolePlaceholder).toBool();
        const bool collapsed = index.data(kRoleCollapsed).toBool();
        const int pidCount = index.data(kRolePidCount).toInt();

        // 图标：折叠行与 PID 条目画「?」色块，普通行画应用图标。
        const QRect iconRect(r.left() + 10, r.top() + (r.height() - kIconSize) / 2,
                             kIconSize, kIconSize);
        const bool pidRow = collapsed || isPidKey(processKey);
        if (pidRow) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(DesignTokens::kProgressBg());
            painter->drawRoundedRect(QRectF(iconRect).adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);
            painter->setFont(DesignTokens::appFont(13, QFont::Bold));
            painter->setPen(DesignTokens::kTextMute());
            painter->drawText(iconRect, Qt::AlignCenter, QStringLiteral("?"));
        } else {
            const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
            if (!icon.isNull())
                icon.paint(painter, iconRect);
        }

        // 名称：占位条目显示友好名；否则显示解析出的应用名。
        const qreal nameX = iconRect.right() + 10;
        const qreal rightPad = 12.0;
        const qreal durW = 52.0;
        const qreal sessW = 64.0;
        const qreal detailW = durW + sessW + 8.0;
        const qreal nameW = qMax<qreal>(0.0, r.right() - rightPad - nameX - detailW);

        // 名称：折叠行固定文案；其余行的 kRoleName 已由调用方算好
        // （普通应用 = displayName，PID 应用 = 「未知进程（编号）」）。
        const QString name = collapsed ? QString::fromUtf8("未识别应用")
                                       : displayName;

        if (nameW > 0.0) {
            QFont nameFont = DesignTokens::appFont(12);
            painter->setFont(nameFont);
            painter->setPen(DesignTokens::kText());
            const QString shown = QFontMetrics(nameFont).elidedText(
                name, Qt::ElideRight, qRound(nameW));
            painter->drawText(QRectF(nameX, r.top(), nameW, r.height()),
                              Qt::AlignLeft | Qt::AlignVCenter, shown);
        }

        // 状态芯片（已屏蔽 / 已合并 / N 个），画在名称与时长之间。
        QString chipText;
        if (collapsed)
            chipText = QString::fromUtf8("%1 个").arg(pidCount);
        else if (ignored)
            chipText = QString::fromUtf8("已屏蔽");
        else if (!mergedInto.isEmpty())
            chipText = QString::fromUtf8("已合并");
        if (placeholder && !collapsed)
            chipText = QString::fromUtf8("无记录");

        if (!chipText.isEmpty()) {
            QFont chipFont = DesignTokens::appFont(9, QFont::Medium);
            const int chipW = QFontMetrics(chipFont).horizontalAdvance(chipText) + 16;
            const int chipH = 18;
            const int chipY = r.top() + (r.height() - chipH) / 2;
            const int chipX = static_cast<int>(r.right() - rightPad - durW - sessW - 8 - chipW);
            if (chipX > nameX + 8) {
                QColor chipBg = DesignTokens::kProgressBg();
                QColor chipFg = DesignTokens::kTextMute();
                if (ignored)
                    chipFg = DesignTokens::kTextFaint();
                painter->setPen(Qt::NoPen);
                painter->setBrush(chipBg);
                painter->drawRoundedRect(QRect(chipX, chipY, chipW, chipH), chipH / 2.0, chipH / 2.0);
                painter->setFont(chipFont);
                painter->setPen(chipFg);
                painter->drawText(QRect(chipX, chipY, chipW, chipH), Qt::AlignCenter, chipText);
            }
        }

        // 右侧时长 + 会话数，等宽字体右对齐。
        const qreal durX = r.right() - rightPad - sessW - durW;
        painter->setFont(DesignTokens::monoFont(11, QFont::Medium));
        painter->setPen(DesignTokens::kTextStrong());
        painter->drawText(QRectF(durX, r.top(), durW, r.height()),
                          Qt::AlignRight | Qt::AlignVCenter,
                          UiUtils::formatDuration(seconds));

        const qreal sessX = r.right() - rightPad - sessW;
        painter->setPen(DesignTokens::kTextMute());
        painter->setFont(DesignTokens::monoFont(9));
        painter->drawText(QRectF(sessX, r.top(), sessW, r.height()),
                          Qt::AlignRight | Qt::AlignVCenter,
                          QString::fromUtf8("%1 次").arg(sessions));

        painter->restore();
    }
};

/// 组别行自绘 delegate：emoji 图标 + 名称 + 预设芯片 + 右侧成员数。
/// 与应用列表同款选中/悬停圆角背景，替换旧的纯文本「名称 (N)」行。
class GroupItemDelegate : public QStyledItemDelegate
{
public:
    explicit GroupItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return QSize(0, kGroupRowHeight);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::TextAntialiasing);

        const QRect r = option.rect;
        const bool selected = option.state & QStyle::State_Selected;
        const bool hover = option.state & QStyle::State_MouseOver;
        if (selected) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(DesignTokens::kAccentLight());
            painter->drawRoundedRect(QRectF(r).adjusted(1, 1, -1, -1), 6.0, 6.0);
        } else if (hover) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(DesignTokens::kButtonHoverBg());
            painter->drawRoundedRect(QRectF(r).adjusted(1, 1, -1, -1), 6.0, 6.0);
        }

        const QString icon = index.data(kRoleGIcon).toString();
        const QString name = index.data(kRoleGName).toString();
        const bool builtin = index.data(kRoleGBuiltin).toBool();
        const int members = index.data(kRoleGMembers).toInt();

        // emoji 图标列：Windows 的 DirectWrite 会按彩色字体渲染。
        QFont iconFont = DesignTokens::appFont(14);
        painter->setFont(iconFont);
        painter->setPen(DesignTokens::kText());
        painter->drawText(QRectF(r.left() + 10, r.top(), 26, r.height()),
                          Qt::AlignCenter, icon);

        const qreal rightPad = 12.0;
        const qreal membersW = 60.0;
        const qreal nameX = r.left() + 10 + 26 + 8;

        // 右侧成员数，等宽弱色。
        painter->setFont(DesignTokens::monoFont(10));
        painter->setPen(DesignTokens::kTextMute());
        painter->drawText(QRectF(r.right() - rightPad - membersW, r.top(), membersW, r.height()),
                          Qt::AlignRight | Qt::AlignVCenter,
                          QString::fromUtf8("%1 应用").arg(members));

        // 预设芯片画在成员数左侧，区分内置组别与自定义组别；名称再为它让位。
        qreal nameRight = r.right() - rightPad - membersW - 8;
        if (builtin) {
            QFont chipFont = DesignTokens::appFont(9, QFont::Medium);
            const QString chipText = QString::fromUtf8("预设");
            const int chipW = QFontMetrics(chipFont).horizontalAdvance(chipText) + 14;
            const int chipH = 16;
            const int chipX = int(nameRight - chipW);
            const int chipY = r.top() + (r.height() - chipH) / 2;
            painter->setPen(Qt::NoPen);
            painter->setBrush(DesignTokens::kProgressBg());
            painter->drawRoundedRect(QRect(chipX, chipY, chipW, chipH), chipH / 2.0, chipH / 2.0);
            painter->setFont(chipFont);
            painter->setPen(DesignTokens::kTextFaint());
            painter->drawText(QRect(chipX, chipY, chipW, chipH), Qt::AlignCenter, chipText);
            nameRight = chipX - 8;
        }

        // 名称，可省略号截断。
        QFont nameFont = DesignTokens::appFont(12);
        painter->setFont(nameFont);
        painter->setPen(DesignTokens::kText());
        painter->drawText(QRectF(nameX, r.top(), qMax<qreal>(0.0, nameRight - nameX), r.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(nameFont).elidedText(
                              name, Qt::ElideRight, qMax(0, int(nameRight - nameX))));

        painter->restore();
    }
};

/// 新建/编辑组别对话框：名称输入 + emoji 图标色板单选。
class GroupEditDialog : public QDialog
{
public:
    GroupEditDialog(const QString &title, const QString &name, const QString &icon,
                    QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(title);
        applyStyle();

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(18, 16, 18, 14);
        layout->setSpacing(8);

        auto *nameLabel = new QLabel(QString::fromUtf8("组别名称"), this);
        layout->addWidget(nameLabel);
        m_name = new QLineEdit(name, this);
        m_name->setPlaceholderText(QString::fromUtf8("例如：学习成长"));
        m_name->setClearButtonEnabled(true);
        layout->addWidget(m_name);

        auto *iconLabel = new QLabel(QString::fromUtf8("组别图标"), this);
        layout->addWidget(iconLabel);

        auto *grid = new QGridLayout();
        grid->setSpacing(6);
        const QStringList &colors = GroupIcons::palette();
        QToolButton *target = nullptr;
        for (int i = 0; i < colors.size(); ++i) {
            auto *btn = new QToolButton(this);
            btn->setText(colors.at(i));
            btn->setObjectName(QStringLiteral("groupIconPick"));
            btn->setCheckable(true);
            btn->setAutoExclusive(true);
            btn->setFixedSize(34, 32);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setProperty("iconValue", colors.at(i));
            connect(btn, &QToolButton::toggled, this, [this, btn](bool on) {
                if (on)
                    m_icon = btn->property("iconValue").toString();
            });
            grid->addWidget(btn, i / 8, i % 8);
            if (colors.at(i) == icon)
                target = btn;
        }
        if (!target && !colors.isEmpty())
            target = qobject_cast<QToolButton *>(grid->itemAtPosition(0, 0)->widget());
        if (target) {
            target->setChecked(true);
            if (m_icon.isEmpty())
                m_icon = target->property("iconValue").toString();
        }
        layout->addLayout(grid);

        auto *box = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        if (auto *okBtn = box->button(QDialogButtonBox::Ok))
            okBtn->setText(QString::fromUtf8("确定"));
        if (auto *cancelBtn = box->button(QDialogButtonBox::Cancel))
            cancelBtn->setText(QString::fromUtf8("取消"));
        connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(box);

        connect(m_name, &QLineEdit::returnPressed, this, &QDialog::accept);
        m_name->setFocus();
    }

    QString groupName() const { return m_name->text().trimmed(); }
    QString groupIcon() const { return m_icon; }

private:
    /// 对话框是独立顶层窗口，不继承页面 QSS，需自带主题色。
    void applyStyle()
    {
        const QString surface = DesignTokens::kSurface().name(QColor::HexArgb);
        const QString border = DesignTokens::kBorder().name(QColor::HexArgb);
        const QString text = DesignTokens::kText().name(QColor::HexArgb);
        const QString hoverBg = DesignTokens::kButtonHoverBg().name(QColor::HexArgb);
        const QString accent = DesignTokens::kAccent().name(QColor::HexArgb);
        const QString accentLight = DesignTokens::kAccentLight().name(QColor::HexArgb);
        setStyleSheet(QStringLiteral(
            "QDialog { background: %1; }"
            "QLabel { color: %2; background: transparent; }"
            "QLineEdit { background: %1; color: %2; border: 1px solid %3;"
            " border-radius: 6px; padding: 6px 9px; }"
            "QToolButton#groupIconPick { background: %1; border: 1px solid %3;"
            " border-radius: 8px; font-size: 16px; }"
            "QToolButton#groupIconPick:hover { background: %4; }"
            "QToolButton#groupIconPick:checked { border: 1px solid %5; background: %6; }"
            "QPushButton { background: %1; color: %2; border: 1px solid %3;"
            " border-radius: 6px; padding: 6px 16px; }"
            "QPushButton:hover { background: %4; }"
            "QPushButton:default { border-color: %5; }")
            .arg(surface, text, border, hoverBg, accent, accentLight));
    }

    QLineEdit *m_name = nullptr;
    QString m_icon;
};

} // namespace

AppManagePage::AppManagePage(DatabaseManager *db, AiClient *ai, QWidget *parent)
    : QWidget(parent), m_db(db), m_ai(ai)
{
    // pageStyle 的透明背景规则依赖此 id 选择器；绝不能改回无选择器的
    // QWidget { background: transparent; }——它会级联进以本页为父的
    // QMessageBox/QToolTip，把弹窗背景掏空成黑色（见 QSS 级联陷阱）。
    setObjectName(QStringLiteral("appManagePage"));
    setStyleSheet(pageStyle());

    // 布局：应用列表是本页主角（hero）——占满整页高度、约 3/5 宽；
    // 右列上方为应用详情，下方为紧凑的组别管理卡片。
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(DesignTokens::kSpacingLg);

    // 卡片容器：复用设置对话框的卡片视觉（Surface + 描边 + 圆角）。
    // 返回 QFrame 本身，调用方用 addWidget 加入父布局——与 settings_dialog 的
    // addSectionCard 一致，布局才能管理到卡片 widget 的几何，否则卡片会塌缩到 sizeHint。
    auto makeCard = [this](const QString &title) {
        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("appCard"));
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(kCardMarginH, kCardMarginV, kCardMarginH, kCardMarginV);
        layout->setSpacing(DesignTokens::kSpacingSm);
        auto *titleLabel = new QLabel(title, card);
        titleLabel->setObjectName(QStringLiteral("appCardTitle"));
        layout->addWidget(titleLabel);
        return card;
    };

    // --- 左：应用列表（hero 卡片） ---
    QFrame *listCard = makeCard(QString::fromUtf8("应用列表"));
    auto *listLayout = qobject_cast<QVBoxLayout *>(listCard->layout());
    auto *toolbar = new QHBoxLayout();
    toolbar->setSpacing(DesignTokens::kSpacingSm);
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QString::fromUtf8("搜索应用..."));
    m_search->setClearButtonEnabled(true);
    m_search->setToolTip(QString::fromUtf8("按显示名或进程名筛选应用"));
    connect(m_search, &QLineEdit::textChanged, this, &AppManagePage::filterApps);
    toolbar->addWidget(m_search, 1);
    m_hideLowUsage = new ToggleSwitch(QString::fromUtf8("隐藏低时长应用"), this);
    m_hideLowUsage->setToolTip(QString::fromUtf8(
        "隐藏累计使用时长不足 2 分钟的应用（含无记录占位进程）；\n"
        "取消勾选可显示并管理这些应用，例如解除误屏蔽。"));
    m_updating = true;
    m_hideLowUsage->setChecked(m_db->getSetting(kHideLowUsageKey, QStringLiteral("1")) == QStringLiteral("1"));
    m_updating = false;
    connect(m_hideLowUsage, &QCheckBox::toggled,
            this, &AppManagePage::onHideLowUsageToggled);
    toolbar->addWidget(m_hideLowUsage);

    // 「只看该组」筛选芯片：点选右侧组别时出现，点 ✕ 退出筛选。
    m_groupFilterChip = new QPushButton(this);
    m_groupFilterChip->setObjectName(QStringLiteral("groupFilterChip"));
    m_groupFilterChip->setCursor(Qt::PointingHandCursor);
    m_groupFilterChip->setToolTip(QString::fromUtf8("正在按组别筛选应用列表，点击退出筛选"));
    m_groupFilterChip->setVisible(false);
    connect(m_groupFilterChip, &QPushButton::clicked, this, &AppManagePage::clearGroupFilter);
    toolbar->addWidget(m_groupFilterChip);
    listLayout->addLayout(toolbar);

    m_appList = new QListWidget(this);
    m_appList->setObjectName(QStringLiteral("appList"));
    m_appList->setIconSize(QSize(kIconSize, kIconSize));
    m_appList->setUniformItemSizes(true);
    m_appList->setItemDelegate(new AppItemDelegate(m_appList));
    m_appList->setToolTip(QString::fromUtf8(
        "已追踪过的应用，按累计时长从高到低排序；选中后在右侧调整名称、组别、合并与屏蔽。\n"
        "取不到可执行路径、以进程编号记录的应用折叠在末尾的「未识别应用」行。"));
    connect(m_appList, &QListWidget::currentItemChanged,
            this, &AppManagePage::onAppSelected);
    connect(m_appList, &QListWidget::itemClicked,
            this, &AppManagePage::onAppRowClicked);
    listLayout->addWidget(m_appList, 1);

    // --- 右：应用详情 ---
    QFrame *detailCard = makeCard(QString::fromUtf8("应用详情"));
    auto *detailLayout = qobject_cast<QVBoxLayout *>(detailCard->layout());

    m_detailStats = new QLabel(this);
    m_detailStats->setObjectName(QStringLiteral("appDetailStats"));
    m_detailStats->setWordWrap(true);
    m_detailStats->setAlignment(Qt::AlignTop);
    detailLayout->addWidget(m_detailStats);

    m_detailTitle = new QLabel(this);
    m_detailTitle->setObjectName(QStringLiteral("appDetailTitle"));
    m_detailTitle->setWordWrap(true);
    detailLayout->addWidget(m_detailTitle);

    m_detailHint = new QLabel(
        QString::fromUtf8("在左侧选择一个应用以调整其名称、组别、合并与屏蔽状态。"), this);
    m_detailHint->setObjectName(QStringLiteral("appDetailHint"));
    m_detailHint->setWordWrap(true);
    m_detailHint->setAlignment(Qt::AlignTop);
    detailLayout->addWidget(m_detailHint);

    auto addRow = [this](QVBoxLayout *layout, const QString &labelText, QWidget *field) {
        auto *row = new QHBoxLayout();
        row->setSpacing(DesignTokens::kSpacingSm);
        auto *label = new QLabel(labelText, this);
        label->setObjectName(QStringLiteral("appDetailLabel"));
        label->setMinimumWidth(kDetailLabelWidth);
        row->addWidget(label);
        row->addWidget(field, 1);
        layout->addLayout(row);
    };

    m_groupCombo = new QComboBox(this);
    m_groupCombo->setToolTip(QString::fromUtf8("把该应用归入某个组别；组别排行按此聚合"));
    connect(m_groupCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AppManagePage::onGroupChanged);
    addRow(detailLayout, QString::fromUtf8("所属组别"), m_groupCombo);

    m_mergeCombo = new QComboBox(this);
    m_mergeCombo->setToolTip(QString::fromUtf8("把该应用的时长并入另一个应用，合并计时；可随时解除"));
    connect(m_mergeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AppManagePage::onMergeChanged);
    addRow(detailLayout, QString::fromUtf8("合并到"), m_mergeCombo);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(DesignTokens::kSpacingSm);
    m_renameBtn = new QPushButton(QString::fromUtf8("重命名"), this);
    m_renameBtn->setObjectName(QStringLiteral("secondaryBtn"));
    m_renameBtn->setToolTip(QString::fromUtf8("自定义该应用的显示名称，统计与排行随之更新"));
    connect(m_renameBtn, &QPushButton::clicked, this, &AppManagePage::onRenameClicked);
    btnRow->addWidget(m_renameBtn);

    // AI 识别只对「未识别应用」（PID 形式记录）开放：根据窗口标题推断应用名。
    m_identifyBtn = new QPushButton(QString::fromUtf8("AI 识别"), this);
    m_identifyBtn->setObjectName(QStringLiteral("secondaryBtn"));
    m_identifyBtn->setToolTip(QString::fromUtf8(
        "根据该应用记录到的窗口标题，让 AI 推断它是什么应用并填入显示名称。\n"
        "需要先在「AI 智能」页配置接口；结果需确认后才会应用。"));
    m_identifyBtn->setVisible(false);
    connect(m_identifyBtn, &QPushButton::clicked, this, &AppManagePage::onIdentifyClicked);
    btnRow->addWidget(m_identifyBtn);
    btnRow->addStretch(1);
    detailLayout->addLayout(btnRow);

    m_ignoreCheck = new ToggleSwitch(QString::fromUtf8("屏蔽此应用，不再追踪"), this);
    m_ignoreCheck->setToolTip(QString::fromUtf8("屏蔽后该应用不再记录会话；已有历史数据仍可通过取消屏蔽恢复统计"));
    connect(m_ignoreCheck, &QCheckBox::toggled,
            this, &AppManagePage::onIgnoreToggled);
    detailLayout->addWidget(m_ignoreCheck);

    detailLayout->addStretch(1);

    // ===== 右列下方：紧凑「组别管理」（点选即筛选应用列表，说明放 tooltip） =====
    QFrame *groupCard = makeCard(QString::fromUtf8("组别管理"));
    auto *groupLayout = qobject_cast<QVBoxLayout *>(groupCard->layout());

    auto *groupHeader = new QHBoxLayout();
    groupHeader->setSpacing(DesignTokens::kSpacingSm);
    groupHeader->addStretch(1);

    auto *addGroupBtn = new QPushButton(QString::fromUtf8("+ 新建组别"), this);
    addGroupBtn->setObjectName(QStringLiteral("secondaryBtn"));
    addGroupBtn->setToolTip(QString::fromUtf8("新增一个自定义组别，可挑选图标"));
    connect(addGroupBtn, &QPushButton::clicked, this, &AppManagePage::onAddGroup);
    groupHeader->addWidget(addGroupBtn);

    m_renameGroupBtn = new QPushButton(QString::fromUtf8("编辑"), this);
    m_renameGroupBtn->setObjectName(QStringLiteral("secondaryBtn"));
    m_renameGroupBtn->setToolTip(QString::fromUtf8("重命名选中组别或更换其图标"));
    connect(m_renameGroupBtn, &QPushButton::clicked, this, &AppManagePage::onRenameGroup);
    groupHeader->addWidget(m_renameGroupBtn);

    m_removeGroupBtn = new QPushButton(QString::fromUtf8("删除"), this);
    m_removeGroupBtn->setObjectName(QStringLiteral("secondaryBtn"));
    m_removeGroupBtn->setToolTip(QString::fromUtf8("删除组别，组内应用自动回到「未分组」"));
    connect(m_removeGroupBtn, &QPushButton::clicked, this, &AppManagePage::onRemoveGroup);
    groupHeader->addWidget(m_removeGroupBtn);
    groupLayout->addLayout(groupHeader);

    m_groupList = new QListWidget(this);
    m_groupList->setObjectName(QStringLiteral("groupList"));
    m_groupList->setToolTip(QString::fromUtf8(
        "组别列表；点击组别可在应用列表中只看其成员。\n"
        "未归入任何组别的应用会计入「未分组」"));
    m_groupList->setItemDelegate(new GroupItemDelegate(m_groupList));
    m_groupList->setFixedHeight(140);
    m_groupList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_groupList, &QListWidget::itemSelectionChanged,
            this, &AppManagePage::onGroupSelectionChanged);
    groupLayout->addWidget(m_groupList);

    // ===== 组装：左列表（hero）+ 右列（详情 / 组别管理） =====
    root->addWidget(listCard, 3);
    auto *rightColumn = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightColumn);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(DesignTokens::kSpacingLg);
    rightLayout->addWidget(detailCard, 1);
    rightLayout->addWidget(groupCard);
    root->addWidget(rightColumn, 2);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { setStyleSheet(pageStyle()); });
    connect(ThemeManager::instance(), &ThemeManager::accentChanged,
            this, [this]() { setStyleSheet(pageStyle()); });

    if (m_ai) {
        connect(m_ai, &AiClient::identifyReady, this, &AppManagePage::onIdentifyReady);
        connect(m_ai, &AiClient::identifyFailed, this, &AppManagePage::onIdentifyFailed);
    }

    reload();
}

QString AppManagePage::pageStyle() const
{
    const QString surface = DesignTokens::kSurface().name(QColor::HexArgb);
    const QString border = DesignTokens::kBorder().name(QColor::HexArgb);
    const QString text = DesignTokens::kText().name(QColor::HexArgb);
    const QString textMute = DesignTokens::kTextMute().name(QColor::HexArgb);
    const QString textFaint = DesignTokens::kTextFaint().name(QColor::HexArgb);
    const QString textStrong = DesignTokens::kTextStrong().name(QColor::HexArgb);
    const QString focus = DesignTokens::kFocusBorder().name(QColor::HexArgb);
    const QString accent = DesignTokens::kAccent().name(QColor::HexArgb);
    const QString hoverBg = DesignTokens::kButtonHoverBg().name(QColor::HexArgb);
    const QString accentLight = DesignTokens::kAccentLight().name(QColor::HexArgb);

    return QStringLiteral(
        "#appManagePage { background: transparent; }"
        "QFrame#appCard { background: %1; border: 1px solid %2; border-radius: 8px; }"
        "QLabel#appCardTitle { color: %4; font-size: 12px; font-weight: 600; background: transparent; }"
        "QLabel#appDetailTitle { color: %8; font-size: 14px; font-weight: 600; background: transparent; }"
        "QLabel#appDetailStats { color: %5; font-size: 12px; background: transparent; }"
        "QLabel#appDetailHint { color: %6; font-size: 12px; background: transparent; }"
        "QLabel#appDetailLabel { color: %5; font-size: 12px; background: transparent; }"
        "QLineEdit { background: %1; color: %3; border: 1px solid %2; border-radius: 6px;"
        " padding: 6px 9px; selection-background-color: %7; }"
        "QLineEdit:focus { border-color: %6; }"
        "QListWidget#appList { background: transparent; border: none; outline: 0; }"
        "QListWidget#groupList { background: %1; color: %3; border: 1px solid %2;"
        " border-radius: 8px; padding: 4px; outline: 0; }"
        "QPushButton#groupFilterChip { background: %10; color: %8; border: none;"
        " border-radius: 10px; padding: 3px 10px; font-size: 11px; }"
        "QPushButton#groupFilterChip:hover { border: 1px solid %8;"
        " padding: 2px 9px; }"
        "QComboBox { background: %1; color: %3; border: 1px solid %2; border-radius: 6px;"
        " padding: 6px 10px; min-height: 18px; }"
        "QComboBox:focus { border-color: %6; }"
        "QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox QAbstractItemView { background: %1; color: %3; border: 1px solid %2;"
        " selection-background-color: %7; selection-color: %3; outline: 0; }"
        // 开关统一用 ToggleSwitch 自绘，不再配 QCheckBox 规则。
        "QPushButton#secondaryBtn { background: %1; color: %3; border: 1px solid %2;"
        " border-radius: 6px; padding: 6px 14px; min-height: 20px; }"
        "QPushButton#secondaryBtn:hover { background: %9; }"
        "QPushButton#secondaryBtn:pressed { background: %9; }"
        "QPushButton#secondaryBtn:disabled { color: %6; }"
        "QPushButton#secondaryBtn:focus { border-color: %6; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background: %6; min-height: 24px; border-radius: 3px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
        .arg(surface, border, text, textMute, textFaint, textStrong, focus, accent,
             hoverBg)
        .arg(accentLight);
}

void AppManagePage::reload()
{
    refreshGroups();
    refreshList();
    refreshDetails();
}

void AppManagePage::applyIcons()
{
    for (int i = 0; i < m_appList->count(); ++i) {
        QListWidgetItem *item = m_appList->item(i);
        const QString path = item->data(kRolePath).toString();
        if (path.isEmpty())
            continue;
        item->setIcon(AppIconProvider::instance()->icon(path, kIconSize));
    }
}

void AppManagePage::refreshList()
{
    m_updating = true;
    const QString previousKey = m_currentKey;
    m_apps = m_db->getManagedApps();

    // PID 形式条目折叠为末尾一行，普通条目按时长降序逐行展示。
    m_pidApps.clear();
    QVector<AppEntry> normalApps;
    for (const AppEntry &entry : m_apps) {
        if (isPidKey(entry.processKey))
            m_pidApps.append(entry);
        else
            normalApps.append(entry);
    }

    m_appList->clear();
    int restoreIndex = -1;
    for (int i = 0; i < normalApps.size(); ++i) {
        const AppEntry &entry = normalApps.at(i);
        auto *item = new QListWidgetItem(m_appList);
        item->setData(kRoleKey, entry.processKey);
        item->setData(kRolePath, entry.processName);
        item->setData(kRoleName, entry.displayName);
        item->setData(kRoleIgnored, entry.ignored);
        item->setData(kRoleMerged, entry.mergedInto);
        item->setData(kRoleSeconds, entry.totalSeconds);
        item->setData(kRoleSessions, entry.sessionCount);
        item->setData(kRolePlaceholder, isPlaceholder(entry));
        item->setData(kRoleGroupId, entry.groupId);
        if (entry.processKey == previousKey)
            restoreIndex = i;
    }

    int collapsedRow = -1;
    if (!m_pidApps.isEmpty()) {
        int pidSeconds = 0;
        int pidSessions = 0;
        for (const AppEntry &entry : m_pidApps) {
            pidSeconds += entry.totalSeconds;
            pidSessions += entry.sessionCount;
        }
        auto *item = new QListWidgetItem(m_appList);
        item->setData(kRoleCollapsed, true);
        item->setData(kRoleName, QString::fromUtf8("未识别应用"));
        item->setData(kRoleSeconds, pidSeconds);
        item->setData(kRoleSessions, pidSessions);
        item->setData(kRolePidCount, m_pidApps.size());
        item->setToolTip(QString::fromUtf8(
            "%1 个应用因取不到可执行文件路径，以进程编号记录。\n"
            "点击查看清单，可逐个改名或用 AI 识别。").arg(m_pidApps.size()));
        collapsedRow = m_appList->count() - 1;
    }
    applyIcons();

    // 重新加载后尽量保持在同一个应用上，避免每次改动都跳回列表顶部。
    if (restoreIndex >= 0) {
        m_appList->setCurrentRow(restoreIndex);
    } else if (isPidKey(previousKey) && collapsedRow >= 0) {
        // 详情面板正指向某个 PID 应用：列表高亮留在折叠行上。
        m_appList->setCurrentRow(collapsedRow);
    } else if (m_appList->count() > 0 && !previousKey.isEmpty()) {
        m_appList->setCurrentRow(0);
    }

    m_updating = false;
    filterApps(m_search->text());
    refreshPidDialog();
}

void AppManagePage::refreshGroups()
{
    // clear()/addItem() 期间抑制选中变化，避免筛选被中间态误重置。
    m_updating = true;
    m_groups = m_db->getGroups();

    const int previousId = m_groupList->currentItem()
        ? m_groupList->currentItem()->data(kRoleGId).toInt()
        : -1;
    m_groupList->clear();
    int restoreRow = -1;
    for (const QVariantMap &row : m_groups) {
        const int id = row.value(QStringLiteral("id")).toInt();
        const QString name = row.value(QStringLiteral("name")).toString();
        const int members = row.value(QStringLiteral("members")).toInt();
        auto *item = new QListWidgetItem(m_groupList);
        item->setText(name); // delegate 全自绘，文本仅供无障碍/查找
        item->setData(kRoleGId, id);
        item->setData(kRoleGIcon, row.value(QStringLiteral("icon")).toString());
        item->setData(kRoleGName, name);
        item->setData(kRoleGBuiltin, row.value(QStringLiteral("builtin")).toBool());
        item->setData(kRoleGMembers, members);
        item->setToolTip(QString::fromUtf8("「%1」%2 个应用；选中后应用列表只展示组内应用")
                             .arg(name).arg(members));
        m_groupList->addItem(item);
        if (id == previousId)
            restoreRow = m_groupList->count() - 1;
    }
    // 未分组不是真实组别，只作为排行里的兜底桶，不列入可编辑列表。
    if (restoreRow >= 0)
        m_groupList->setCurrentRow(restoreRow);
    m_updating = false;

    onGroupSelectionChanged(); // 列表重建后同步按钮态与「只看该组」筛选
}

void AppManagePage::refreshDetails()
{
    const QString key = currentKey();
    const bool hasSelection = !key.isEmpty();
    m_detailHint->setVisible(!hasSelection);
    m_detailTitle->setVisible(hasSelection);
    m_groupCombo->setEnabled(hasSelection);
    m_mergeCombo->setEnabled(hasSelection);
    m_ignoreCheck->setEnabled(hasSelection);
    m_renameBtn->setEnabled(hasSelection);
    // AI 识别只对 PID 形式记录的应用开放（按窗口标题推断应用名）。
    m_identifyBtn->setVisible(isPidKey(key));
    m_identifyBtn->setEnabled(hasSelection && m_aiPendingKey.isEmpty());

    AppEntry entry;
    bool found = false;
    for (const AppEntry &candidate : m_apps) {
        if (candidate.processKey != key)
            continue;
        entry = candidate;
        found = true;
        break;
    }
    if (!found) {
        m_detailTitle->clear();
        m_detailStats->clear();
        m_updating = true;
        m_groupCombo->clear();
        m_mergeCombo->clear();
        m_ignoreCheck->setChecked(false);
        m_updating = false;
        return;
    }

    m_updating = true;
    m_detailTitle->setText(isPidKey(entry.processKey) ? pidDisplayName(entry)
                                                      : entry.displayName);

    // 组别下拉：索引 0 为「未分组」，其余按 getGroups() 顺序。
    m_groupCombo->clear();
    m_groupCombo->addItem(ungroupedLabel(), kNoGroupId);
    int groupIndex = 0;
    for (const QVariantMap &row : m_groups) {
        const int id = row.value(QStringLiteral("id")).toInt();
        m_groupCombo->addItem(QStringLiteral("%1 %2 (%3)")
                                  .arg(row.value(QStringLiteral("icon")).toString(),
                                       row.value(QStringLiteral("name")).toString())
                                  .arg(row.value(QStringLiteral("members")).toInt()),
                              id);
        if (id == entry.groupId)
            groupIndex = m_groupCombo->count() - 1;
    }
    m_groupCombo->setCurrentIndex(groupIndex);

    // 合并下拉：只列未被合并的应用（根应用），避免形成多层链，并排除自身。
    m_mergeCombo->clear();
    m_mergeCombo->addItem(QString::fromUtf8("不合并"), QString());
    int mergeIndex = 0;
    for (const AppEntry &candidate : m_apps) {
        if (candidate.processKey == key || !candidate.mergedInto.isEmpty())
            continue;
        m_mergeCombo->addItem(candidate.displayName, candidate.processKey);
        if (candidate.processKey == entry.mergedInto)
            mergeIndex = m_mergeCombo->count() - 1;
    }
    if (!entry.mergedInto.isEmpty() && mergeIndex == 0) {
        // 目标已被其它应用合并时不会出现在候选里，补一项保证回显不丢，用显示名而非键。
        QString display = entry.mergedInto;
        for (const AppEntry &candidate : m_apps) {
            if (candidate.processKey == entry.mergedInto) {
                display = candidate.displayName;
                break;
            }
        }
        m_mergeCombo->addItem(display, entry.mergedInto);
        mergeIndex = m_mergeCombo->count() - 1;
    }
    m_mergeCombo->setCurrentIndex(mergeIndex);

    m_ignoreCheck->setChecked(entry.ignored);
    m_updating = false;

    QString stats = QString::fromUtf8("累计 %1 · %2 次会话")
                        .arg(UiUtils::formatDuration(entry.totalSeconds))
                        .arg(entry.sessionCount);
    if (entry.ignored)
        stats += QString::fromUtf8("\n已屏蔽：不再记录新的使用时间。");
    else if (!entry.mergedInto.isEmpty())
        stats += QString::fromUtf8("\n时长已并入其它应用（原始数据保留，可随时解除）。");
    else if (isPidKey(entry.processKey))
        stats += QString::fromUtf8(
            "\n以 PID 形式记录：取不到可执行文件路径。可重命名，或用「AI 识别」"
            "根据窗口标题推断应用名。");
    else if (isPlaceholder(entry))
        stats += QString::fromUtf8("\n占位条目：该应用已被屏蔽且尚无任何使用记录。");
    m_detailStats->setText(stats);
}

void AppManagePage::onGroupSelectionChanged()
{
    if (m_updating)
        return;
    // 用选中项而非 currentItem：clearSelection() 后 currentItem 可能仍指向旧行，
    // 只读 currentItem 会让筛选无法退出。
    const auto selected = m_groupList->selectedItems();
    QListWidgetItem *item = selected.isEmpty() ? nullptr : selected.first();
    const bool hasGroup = item != nullptr;
    m_renameGroupBtn->setEnabled(hasGroup);
    m_removeGroupBtn->setEnabled(hasGroup);

    // 选中组别 → 应用列表只看其组员；取消选中即退出筛选。
    m_groupFilterId = hasGroup ? item->data(kRoleGId).toInt() : -1;
    if (hasGroup) {
        m_groupFilterChip->setText(QString::fromUtf8("只看 %1 %2 ✕")
                                       .arg(item->data(kRoleGIcon).toString(),
                                            item->data(kRoleGName).toString()));
    }
    m_groupFilterChip->setVisible(hasGroup);
    filterApps(m_search->text());
}

void AppManagePage::clearGroupFilter()
{
    m_groupList->clearSelection();
    m_groupList->setCurrentItem(nullptr); // 触发 selectionChanged 统一收敛
}

QString AppManagePage::currentKey() const
{
    return m_currentKey;
}

QString AppManagePage::mergeTargetFor(int index) const
{
    return m_mergeCombo->itemData(index).toString();
}

void AppManagePage::filterApps(const QString &text)
{
    const QString needle = text.trimmed();
    const bool hideLow = m_hideLowUsage && m_hideLowUsage->isChecked();
    for (int i = 0; i < m_appList->count(); ++i) {
        QListWidgetItem *item = m_appList->item(i);
        if (item->data(kRoleCollapsed).toBool()) {
            // 折叠行不受低时长开关影响（它是入口行）；按组筛选时整体隐藏，
            // 搜索命中「未识别」或「pid」时保留。
            const bool visible = m_groupFilterId < 0
                                 && (needle.isEmpty()
                                     || QString::fromUtf8("未识别应用").contains(needle)
                                     || QStringLiteral("pid").contains(needle, Qt::CaseInsensitive));
            item->setHidden(!visible);
            continue;
        }
        // 累计不足 2 分钟（含无记录占位进程）默认不显示，开关可恢复。
        if (hideLow && item->data(kRoleSeconds).toInt() < kMinDisplaySeconds) {
            item->setHidden(true);
            continue;
        }
        // 「只看该组」：组别筛选与搜索关键字取交集。
        if (m_groupFilterId >= 0 && item->data(kRoleGroupId).toInt() != m_groupFilterId) {
            item->setHidden(true);
            continue;
        }
        if (needle.isEmpty()) {
            item->setHidden(false);
            continue;
        }
        const bool hit = item->data(kRoleName).toString().contains(needle, Qt::CaseInsensitive)
                         || item->data(kRoleKey).toString().contains(needle, Qt::CaseInsensitive);
        item->setHidden(!hit);
    }
}

void AppManagePage::onAppSelected(QListWidgetItem *current, QListWidgetItem *)
{
    if (!current)
        return;
    // 折叠行不代表单个应用：不改变详情面板的当前选择（弹窗负责选中）。
    if (current->data(kRoleCollapsed).toBool())
        return;
    m_currentKey = current->data(kRoleKey).toString();
    refreshDetails();
}

void AppManagePage::onAppRowClicked(QListWidgetItem *item)
{
    if (!item || !item->data(kRoleCollapsed).toBool())
        return;
    ensurePidDialog();
    refreshPidDialog();
    m_pidDialog->show();
    m_pidDialog->raise();
    m_pidDialog->activateWindow();
}

void AppManagePage::onHideLowUsageToggled(bool checked)
{
    if (m_updating)
        return;
    m_db->setSetting(kHideLowUsageKey, checked ? QStringLiteral("1") : QStringLiteral("0"));
    filterApps(m_search->text());
}

void AppManagePage::onRenameClicked()
{
    const QString key = currentKey();
    if (key.isEmpty())
        return;
    QString current;
    for (const AppEntry &entry : m_apps)
        if (entry.processKey == key)
            current = entry.displayName;

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QString::fromUtf8("重命名应用"), QString::fromUtf8("显示名称"),
        QLineEdit::Normal, current, &ok);
    if (!ok)
        return;
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("名称无效"),
                             QString::fromUtf8("显示名称不能为空。"));
        return;
    }
    // 空串会静默失败并保留旧行，恢复默认名要显式删除别名。
    if (trimmed == current)
        return;
    m_db->setAppAlias(key, trimmed);
    refreshList();
    refreshDetails();
    emit appsChanged();
}

void AppManagePage::onGroupChanged(int index)
{
    if (m_updating)
        return;
    const QString key = currentKey();
    if (key.isEmpty())
        return;
    m_db->setAppGroup(key, m_groupCombo->itemData(index).toInt());
    refreshGroups();
    refreshList();
    refreshDetails();
    emit appsChanged();
}

void AppManagePage::onMergeChanged(int index)
{
    if (m_updating)
        return;
    const QString key = currentKey();
    if (key.isEmpty())
        return;
    const QString target = mergeTargetFor(index);
    if (target.isEmpty())
        m_db->removeAppMerge(key);
    else if (!m_db->setAppMerge(key, target))
        QMessageBox::warning(this, QString::fromUtf8("无法合并"),
                             QString::fromUtf8("该应用不能作为合并目标，请选择另一个应用。"));
    refreshList();
    refreshDetails();
    emit appsChanged();
}

void AppManagePage::onIgnoreToggled(bool checked)
{
    if (m_updating)
        return;
    const QString key = currentKey();
    if (key.isEmpty())
        return;
    if (checked) {
        m_db->addIgnoredApp(key);
    } else {
        const QMap<int, QString> ignored = m_db->getIgnoredApps();
        for (auto it = ignored.cbegin(); it != ignored.cend(); ++it) {
            if (it.value() == key)
                m_db->removeIgnoredApp(it.key());
        }
    }
    refreshList();
    refreshDetails();
    emit appsChanged();
}

void AppManagePage::onAddGroup()
{
    GroupEditDialog dlg(QString::fromUtf8("新建组别"), QString(), QString(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QString name = dlg.groupName();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("名称无效"),
                             QString::fromUtf8("组别名称不能为空。"));
        return;
    }
    if (m_db->addGroup(name, dlg.groupIcon()) < 0) {
        QMessageBox::warning(this, QString::fromUtf8("无法新建组别"),
                             QString::fromUtf8("该组别名称已存在，请换一个名称。"));
        return;
    }
    refreshGroups();
    refreshList();
    refreshDetails();
    emit appsChanged();
}

void AppManagePage::onRenameGroup()
{
    QListWidgetItem *item = m_groupList->currentItem();
    if (!item)
        return;
    const int id = item->data(kRoleGId).toInt();
    const QString oldName = item->data(kRoleGName).toString();
    const QString oldIcon = item->data(kRoleGIcon).toString();
    GroupEditDialog dlg(QString::fromUtf8("编辑组别"), oldName, oldIcon, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QString name = dlg.groupName();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("名称无效"),
                             QString::fromUtf8("组别名称不能为空。"));
        return;
    }
    if (name != oldName)
        m_db->renameGroup(id, name);
    if (!dlg.groupIcon().isEmpty() && dlg.groupIcon() != oldIcon)
        m_db->setGroupIcon(id, dlg.groupIcon());
    refreshGroups();
    refreshList();
    refreshDetails();
    emit appsChanged();
}

void AppManagePage::onRemoveGroup()
{
    QListWidgetItem *item = m_groupList->currentItem();
    if (!item)
        return;
    const QString name = item->data(kRoleGName).toString();
    const int members = item->data(kRoleGMembers).toInt();
    QString prompt = QString::fromUtf8("确定删除组别「%1」吗？").arg(name);
    if (members > 0)
        prompt += QString::fromUtf8("\n组内 %1 个应用将回到「未分组」，使用记录不会丢失。").arg(members);
    if (QMessageBox::question(this, QString::fromUtf8("删除组别"), prompt)
        != QMessageBox::Yes) {
        return;
    }
    m_db->removeGroup(item->data(kRoleGId).toInt());
    refreshGroups();
    refreshList();
    refreshDetails();
    emit appsChanged();
}

void AppManagePage::onIdentifyClicked()
{
    if (m_updating)
        return;
    const QString key = m_currentKey;
    if (key.isEmpty() || !isPidKey(key) || !m_aiPendingKey.isEmpty())
        return;
    if (!m_ai || !m_ai->isConfigured()) {
        QMessageBox::information(this, QString::fromUtf8("AI 未配置"),
                                 QString::fromUtf8("请先在设置「AI 智能」页启用并配置"
                                                   "接口（端点 / API Key / 模型），再试一次。"));
        return;
    }
    const QStringList titles = m_db->getRecentWindowTitles(key, 20);
    if (titles.isEmpty()) {
        QMessageBox::information(this, QString::fromUtf8("无法识别"),
                                 QString::fromUtf8("该应用没有记录到窗口标题，"
                                                   "无法用 AI 识别，可直接重命名。"));
        return;
    }
    if (!m_ai->identifyApp(titles, key)) {
        QMessageBox::information(this, QString::fromUtf8("无法识别"),
                                 QString::fromUtf8("窗口标题均为进程编号兜底，没有可用于识别的信息。"));
        return;
    }
    m_aiPendingKey = key;
    m_identifyBtn->setEnabled(false);
    m_identifyBtn->setText(QString::fromUtf8("识别中…"));
}

void AppManagePage::resetIdentifyButton()
{
    m_aiPendingKey.clear();
    m_identifyBtn->setText(QString::fromUtf8("AI 识别"));
    refreshDetails(); // 按钮 enabled 状态随 m_aiPendingKey 归位
}

void AppManagePage::onIdentifyReady(const QString &tag, const QString &name)
{
    if (tag != m_aiPendingKey)
        return; // 迟到的旧响应，忽略
    resetIdentifyButton();
    if (m_currentKey != tag)
        return; // 用户已切走，不打扰
    const int ret = QMessageBox::question(
        this, QString::fromUtf8("AI 识别结果"),
        QString::fromUtf8("识别该应用为「%1」，应用为显示名称吗？").arg(name),
        QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        m_db->setAppAlias(tag, name);
        refreshList();
        refreshDetails();
        emit appsChanged();
    }
}

void AppManagePage::onIdentifyFailed(const QString &tag, const QString &error)
{
    if (tag != m_aiPendingKey)
        return;
    resetIdentifyButton();
    QMessageBox::warning(this, QString::fromUtf8("AI 识别失败"),
                         error.isEmpty() ? QString::fromUtf8("未能识别出应用名。") : error);
}

void AppManagePage::ensurePidDialog()
{
    if (m_pidDialog)
        return;

    m_pidDialog = new QDialog(this);
    m_pidDialog->setObjectName(QStringLiteral("pidDialog"));
    m_pidDialog->setWindowTitle(QString::fromUtf8("未识别应用"));
    m_pidDialog->setModal(false); // 非模态：选中后右侧详情即时联动
    m_pidDialog->resize(460, 420);

    // 顶层窗口不继承页内 QSS，自带主题色（选择器全部带 id/类型，避免级联污染）。
    const QString surface = DesignTokens::kSurface().name(QColor::HexArgb);
    const QString border = DesignTokens::kBorder().name(QColor::HexArgb);
    const QString text = DesignTokens::kText().name(QColor::HexArgb);
    const QString textMute = DesignTokens::kTextMute().name(QColor::HexArgb);
    const QString textFaint = DesignTokens::kTextFaint().name(QColor::HexArgb);
    const QString hoverBg = DesignTokens::kButtonHoverBg().name(QColor::HexArgb);
    m_pidDialog->setStyleSheet(QStringLiteral(
        "QDialog#pidDialog { background: %1; }"
        "QLabel#pidHint { color: %3; font-size: 12px; background: transparent; }"
        "QListWidget#pidList { background: transparent; border: 1px solid %2;"
        " border-radius: 8px; outline: 0; }"
        "QPushButton#secondaryBtn { background: %1; color: %4; border: 1px solid %2;"
        " border-radius: 6px; padding: 6px 14px; min-height: 20px; }"
        "QPushButton#secondaryBtn:hover { background: %5; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background: %3; min-height: 24px; border-radius: 3px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
        .arg(surface, border, textMute, text, hoverBg));

    auto *layout = new QVBoxLayout(m_pidDialog);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(DesignTokens::kSpacingSm);

    auto *hint = new QLabel(QString::fromUtf8(
        "以下应用取不到可执行文件路径，以进程编号（PID）记录。\n"
        "点击选中某个应用后，可在右侧面板改名，或用「AI 识别」推断应用名。"),
        m_pidDialog);
    hint->setObjectName(QStringLiteral("pidHint"));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_pidList = new QListWidget(m_pidDialog);
    m_pidList->setObjectName(QStringLiteral("pidList"));
    m_pidList->setIconSize(QSize(kIconSize, kIconSize));
    m_pidList->setUniformItemSizes(true);
    m_pidList->setItemDelegate(new AppItemDelegate(m_pidList));
    connect(m_pidList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item)
            return;
        m_currentKey = item->data(kRoleKey).toString();
        refreshDetails();
    });
    layout->addWidget(m_pidList, 1);

    auto *closeBtn = new QPushButton(QString::fromUtf8("关闭"), m_pidDialog);
    closeBtn->setObjectName(QStringLiteral("secondaryBtn"));
    connect(closeBtn, &QPushButton::clicked, m_pidDialog, &QDialog::close);
    layout->addWidget(closeBtn, 0, Qt::AlignRight);
}

void AppManagePage::refreshPidDialog()
{
    if (!m_pidDialog)
        return;
    m_pidList->clear();
    for (const AppEntry &entry : m_pidApps) {
        auto *item = new QListWidgetItem(m_pidList);
        item->setData(kRoleKey, entry.processKey);
        item->setData(kRoleName, pidDisplayName(entry));
        item->setData(kRoleIgnored, entry.ignored);
        item->setData(kRoleMerged, entry.mergedInto);
        item->setData(kRoleSeconds, entry.totalSeconds);
        item->setData(kRoleSessions, entry.sessionCount);
        item->setToolTip(QString::fromUtf8("点击后在右侧面板改名或用 AI 识别"));
    }
    m_pidDialog->setWindowTitle(QString::fromUtf8("未识别应用（%1）").arg(m_pidApps.size()));
    if (m_pidApps.isEmpty())
        m_pidDialog->hide();
}
