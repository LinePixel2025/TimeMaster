#include "app_manage_page.h"

#include "icon/app_icon_provider.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"
#include "ui/ui_utils.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
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
#include <QVBoxLayout>

namespace {

constexpr int kIconSize = 24;
constexpr int kRowHeight = 44;
constexpr int kDetailLabelWidth = 82;
constexpr int kCardMarginH = 16;
constexpr int kCardMarginV = 14;

/// 组别下拉/列表的「未分组」占位 id（app_groups 的 id 恒为正数）。
constexpr int kNoGroupId = -1;

/// 「隐藏无记录进程」开关的持久化键。
const QString kHidePlaceholderKey = QStringLiteral("app_manage_hide_placeholder");

// 应用列表 item 的 data role：把 AppEntry 字段平铺存储，供自绘 delegate 读取。
constexpr int kRoleKey = Qt::UserRole;          // processKey
constexpr int kRolePath = Qt::UserRole + 1;      // processName（取图标）
constexpr int kRoleName = Qt::UserRole + 2;      // displayName
constexpr int kRoleIgnored = Qt::UserRole + 3;   // bool
constexpr int kRoleMerged = Qt::UserRole + 4;    // QString mergedInto
constexpr int kRoleSeconds = Qt::UserRole + 5;   // int
constexpr int kRoleSessions = Qt::UserRole + 6;  // int
constexpr int kRolePlaceholder = Qt::UserRole + 7; // bool

QString ungroupedLabel()
{
    return QString::fromUtf8("未分组");
}

/// 是否存在任何真实会话：无记录（占位）条目只在追踪层取不到 exe 路径时出现。
bool isPlaceholder(const AppEntry &entry)
{
    return entry.totalSeconds == 0 && entry.sessionCount == 0;
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

        // 图标
        const QRect iconRect(r.left() + 10, r.top() + (r.height() - kIconSize) / 2,
                             kIconSize, kIconSize);
        const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        if (!icon.isNull())
            icon.paint(painter, iconRect);

        // 名称：占位条目显示友好名；否则显示解析出的应用名。
        const qreal nameX = iconRect.right() + 10;
        const qreal rightPad = 12.0;
        const qreal durW = 52.0;
        const qreal sessW = 64.0;
        const qreal detailW = durW + sessW + 8.0;
        const qreal nameW = qMax<qreal>(0.0, r.right() - rightPad - nameX - detailW);

        QString name;
        if (placeholder)
            name = QString::fromUtf8("未知进程（%1）").arg(processKey.mid(4));
        else
            name = displayName;

        if (nameW > 0.0) {
            QFont nameFont = DesignTokens::appFont(12);
            painter->setFont(nameFont);
            painter->setPen(DesignTokens::kText());
            const QString shown = QFontMetrics(nameFont).elidedText(
                name, Qt::ElideRight, qRound(nameW));
            painter->drawText(QRectF(nameX, r.top(), nameW, r.height()),
                              Qt::AlignLeft | Qt::AlignVCenter, shown);
        }

        // 状态芯片（已屏蔽 / 已合并），画在名称与时长之间。
        QString chipText;
        if (ignored)
            chipText = QString::fromUtf8("已屏蔽");
        else if (!mergedInto.isEmpty())
            chipText = QString::fromUtf8("已合并");
        if (placeholder)
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

} // namespace

AppManagePage::AppManagePage(DatabaseManager *db, QWidget *parent)
    : QWidget(parent), m_db(db)
{
    setStyleSheet(pageStyle());

    auto *root = new QVBoxLayout(this);
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

    // ===== 上区：左「应用列表」 + 右「应用详情」 =====
    auto *split = new QHBoxLayout();
    split->setSpacing(DesignTokens::kSpacingLg);

    // --- 左：应用列表 ---
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
    m_hidePlaceholder = new QCheckBox(QString::fromUtf8("隐藏无记录进程"), this);
    m_hidePlaceholder->setToolTip(QString::fromUtf8("隐藏仅以进程编号命名的占位应用（无法取得可执行文件路径）"));
    m_updating = true;
    m_hidePlaceholder->setChecked(m_db->getSetting(kHidePlaceholderKey, QStringLiteral("1")) == QStringLiteral("1"));
    m_updating = false;
    connect(m_hidePlaceholder, &QCheckBox::toggled,
            this, &AppManagePage::onHidePlaceholderToggled);
    toolbar->addWidget(m_hidePlaceholder);
    listLayout->addLayout(toolbar);

    m_appList = new QListWidget(this);
    m_appList->setObjectName(QStringLiteral("appList"));
    m_appList->setIconSize(QSize(kIconSize, kIconSize));
    m_appList->setUniformItemSizes(true);
    m_appList->setItemDelegate(new AppItemDelegate(m_appList));
    m_appList->setToolTip(QString::fromUtf8("已追踪过的应用；选中后在右侧调整名称、组别、合并与屏蔽"));
    connect(m_appList, &QListWidget::currentItemChanged,
            this, &AppManagePage::onAppSelected);
    listLayout->addWidget(m_appList, 1);
    split->addWidget(listCard, 3);

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

    m_renameBtn = new QPushButton(QString::fromUtf8("重命名"), this);
    m_renameBtn->setObjectName(QStringLiteral("secondaryBtn"));
    m_renameBtn->setToolTip(QString::fromUtf8("自定义该应用的显示名称，统计与排行随之更新"));
    connect(m_renameBtn, &QPushButton::clicked, this, &AppManagePage::onRenameClicked);
    detailLayout->addWidget(m_renameBtn, 0, Qt::AlignLeft);

    m_ignoreCheck = new QCheckBox(QString::fromUtf8("屏蔽此应用，不再追踪"), this);
    m_ignoreCheck->setToolTip(QString::fromUtf8("屏蔽后该应用不再记录会话；已有历史数据仍可通过取消屏蔽恢复统计"));
    connect(m_ignoreCheck, &QCheckBox::toggled,
            this, &AppManagePage::onIgnoreToggled);
    detailLayout->addWidget(m_ignoreCheck);

    detailLayout->addStretch(1);
    split->addWidget(detailCard, 4);
    root->addLayout(split, 1);

    // ===== 下区：整宽「组别管理」 =====
    QFrame *groupCard = makeCard(QString::fromUtf8("组别管理"));
    auto *groupLayout = qobject_cast<QVBoxLayout *>(groupCard->layout());

    auto *groupHeader = new QHBoxLayout();
    groupHeader->setSpacing(DesignTokens::kSpacingSm);
    groupHeader->addStretch(1);

    auto *addGroupBtn = new QPushButton(QString::fromUtf8("+ 新建组别"), this);
    addGroupBtn->setObjectName(QStringLiteral("secondaryBtn"));
    addGroupBtn->setToolTip(QString::fromUtf8("新增一个自定义组别"));
    connect(addGroupBtn, &QPushButton::clicked, this, &AppManagePage::onAddGroup);
    groupHeader->addWidget(addGroupBtn);

    m_renameGroupBtn = new QPushButton(QString::fromUtf8("重命名"), this);
    m_renameGroupBtn->setObjectName(QStringLiteral("secondaryBtn"));
    m_renameGroupBtn->setToolTip(QString::fromUtf8("重命名选中组别"));
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
    m_groupList->setToolTip(QString::fromUtf8("组别列表；未归入任何组别的应用会计入「未分组」"));
    m_groupList->setFixedHeight(132);
    m_groupList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_groupList, &QListWidget::itemSelectionChanged,
            this, &AppManagePage::updateGroupActions);
    groupLayout->addWidget(m_groupList);
    root->addWidget(groupCard);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { setStyleSheet(pageStyle()); });
    connect(ThemeManager::instance(), &ThemeManager::accentChanged,
            this, [this]() { setStyleSheet(pageStyle()); });

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

    return QStringLiteral(
        "QWidget { background: transparent; }"
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
        "QListWidget#groupList::item { padding: 6px 8px; border-radius: 6px; margin: 1px 2px; }"
        "QListWidget#groupList::item:selected { background: %7; color: %3; }"
        "QListWidget#groupList::item:hover:!selected { background: %9; }"
        "QComboBox { background: %1; color: %3; border: 1px solid %2; border-radius: 6px;"
        " padding: 6px 10px; min-height: 18px; }"
        "QComboBox:focus { border-color: %6; }"
        "QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox QAbstractItemView { background: %1; color: %3; border: 1px solid %2;"
        " selection-background-color: %7; selection-color: %3; outline: 0; }"
        "QCheckBox { color: %3; background: transparent; spacing: 8px; }"
        "QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px;"
        " border: 1px solid %2; background: %1; }"
        "QCheckBox::indicator:checked { background: %7; border-color: %7; }"
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
             hoverBg);
}

void AppManagePage::reload()
{
    refreshGroups();
    refreshList();
    refreshDetails();
    updateGroupActions();
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
    const QString previousKey = currentKey();
    m_apps = m_db->getManagedApps();

    m_appList->clear();
    int restoreIndex = -1;
    for (int i = 0; i < m_apps.size(); ++i) {
        const AppEntry &entry = m_apps.at(i);
        auto *item = new QListWidgetItem(m_appList);
        item->setData(kRoleKey, entry.processKey);
        item->setData(kRolePath, entry.processName);
        item->setData(kRoleName, entry.displayName);
        item->setData(kRoleIgnored, entry.ignored);
        item->setData(kRoleMerged, entry.mergedInto);
        item->setData(kRoleSeconds, entry.totalSeconds);
        item->setData(kRoleSessions, entry.sessionCount);
        item->setData(kRolePlaceholder, isPlaceholder(entry));
        m_appList->addItem(item);
        if (entry.processKey == previousKey)
            restoreIndex = i;
    }
    applyIcons();

    // 重新加载后尽量保持在同一个应用上，避免每次改动都跳回列表顶部。
    if (restoreIndex >= 0)
        m_appList->setCurrentRow(restoreIndex);
    else if (m_appList->count() > 0 && !previousKey.isEmpty())
        m_appList->setCurrentRow(0);

    m_updating = false;
    filterApps(m_search->text());
}

void AppManagePage::refreshGroups()
{
    m_groups = m_db->getGroups();

    const QString previousName = m_groupList->currentItem()
        ? m_groupList->currentItem()->data(Qt::UserRole + 1).toString()
        : QString();
    m_groupList->clear();
    int restoreRow = -1;
    for (const QVariantMap &row : m_groups) {
        const QString name = row.value(QStringLiteral("name")).toString();
        auto *item = new QListWidgetItem(
            QStringLiteral("%1 (%2)").arg(name).arg(row.value(QStringLiteral("members")).toInt()),
            m_groupList);
        item->setData(Qt::UserRole, row.value(QStringLiteral("id")).toInt());
        item->setData(Qt::UserRole + 1, name);
        item->setData(Qt::UserRole + 2, row.value(QStringLiteral("members")).toInt());
        m_groupList->addItem(item);
        if (name == previousName)
            restoreRow = m_groupList->count() - 1;
    }
    // 未分组不是真实组别，只作为排行里的兜底桶，不列入可编辑列表。
    if (restoreRow >= 0)
        m_groupList->setCurrentRow(restoreRow);
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
    m_detailTitle->setText(entry.displayName);

    // 组别下拉：索引 0 为「未分组」，其余按 getGroups() 顺序。
    m_groupCombo->clear();
    m_groupCombo->addItem(ungroupedLabel(), kNoGroupId);
    int groupIndex = 0;
    for (const QVariantMap &row : m_groups) {
        const int id = row.value(QStringLiteral("id")).toInt();
        m_groupCombo->addItem(QStringLiteral("%1 (%2)")
                                  .arg(row.value(QStringLiteral("name")).toString())
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
    else if (isPlaceholder(entry))
        stats += QString::fromUtf8("\n占位条目：无法获取可执行文件路径，仅以进程编号记录。");
    m_detailStats->setText(stats);
}

void AppManagePage::updateGroupActions()
{
    const bool hasGroup = m_groupList->currentItem() != nullptr;
    m_renameGroupBtn->setEnabled(hasGroup);
    m_removeGroupBtn->setEnabled(hasGroup);
}

QString AppManagePage::currentKey() const
{
    QListWidgetItem *item = m_appList->currentItem();
    return item ? item->data(kRoleKey).toString() : QString();
}

QString AppManagePage::mergeTargetFor(int index) const
{
    return m_mergeCombo->itemData(index).toString();
}

void AppManagePage::filterApps(const QString &text)
{
    const QString needle = text.trimmed();
    const bool hidePlaceholder = m_hidePlaceholder && m_hidePlaceholder->isChecked();
    for (int i = 0; i < m_appList->count(); ++i) {
        QListWidgetItem *item = m_appList->item(i);
        if (hidePlaceholder && item->data(kRolePlaceholder).toBool()) {
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

void AppManagePage::onAppSelected(QListWidgetItem *, QListWidgetItem *)
{
    refreshDetails();
}

void AppManagePage::onHidePlaceholderToggled(bool checked)
{
    if (m_updating)
        return;
    m_db->setSetting(kHidePlaceholderKey, checked ? QStringLiteral("1") : QStringLiteral("0"));
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
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QString::fromUtf8("新建组别"), QString::fromUtf8("组别名称"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok)
        return;
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("名称无效"),
                             QString::fromUtf8("组别名称不能为空。"));
        return;
    }
    if (m_db->addGroup(trimmed) < 0) {
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
    const int id = item->data(Qt::UserRole).toInt();
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QString::fromUtf8("重命名组别"), QString::fromUtf8("组别名称"),
        QLineEdit::Normal, item->data(Qt::UserRole + 1).toString(), &ok);
    if (!ok)
        return;
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("名称无效"),
                             QString::fromUtf8("组别名称不能为空。"));
        return;
    }
    m_db->renameGroup(id, trimmed);
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
    const QString name = item->data(Qt::UserRole + 1).toString();
    const int members = item->data(Qt::UserRole + 2).toInt();
    QString prompt = QString::fromUtf8("确定删除组别「%1」吗？").arg(name);
    if (members > 0)
        prompt += QString::fromUtf8("\n组内 %1 个应用将回到「未分组」，使用记录不会丢失。").arg(members);
    if (QMessageBox::question(this, QString::fromUtf8("删除组别"), prompt)
        != QMessageBox::Yes) {
        return;
    }
    m_db->removeGroup(item->data(Qt::UserRole).toInt());
    refreshGroups();
    refreshList();
    refreshDetails();
    emit appsChanged();
}
