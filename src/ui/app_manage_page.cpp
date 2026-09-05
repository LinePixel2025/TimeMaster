#include "app_manage_page.h"

#include "icon/app_icon_provider.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"
#include "ui/ui_utils.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

constexpr int kIconSize = 20;
constexpr int kDetailLabelWidth = 82;

/// 组别下拉/列表的「未分组」占位 id（app_groups 的 id 恒为正数）。
constexpr int kNoGroupId = -1;

QString ungroupedLabel()
{
    return QString::fromUtf8("\xe6\x9c\xaa\xe5\x88\x86\xe7\xbb\x84"); // 未分组
}

/// 应用条目：图标 + 名称，被屏蔽的加后缀并置灰，被合并的标注目标。
QString itemLabel(const AppEntry &entry)
{
    QString label = entry.displayName;
    if (entry.ignored)
        label += QString::fromUtf8(" · 已屏蔽");
    else if (!entry.mergedInto.isEmpty())
        label += QString::fromUtf8(" · 已合并");
    return label;
}

} // namespace

AppManagePage::AppManagePage(DatabaseManager *db, QWidget *parent)
    : QWidget(parent), m_db(db)
{
    setStyleSheet(pageStyle());

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(DesignTokens::kSpacingMd);

    // ---- 应用列表 + 右侧属性面板 ----
    auto *split = new QHBoxLayout();
    split->setSpacing(DesignTokens::kSpacingLg);

    auto *listPanel = new QVBoxLayout();
    listPanel->setSpacing(DesignTokens::kSpacingSm);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QString::fromUtf8("搜索应用..."));
    m_search->setClearButtonEnabled(true);
    m_search->setToolTip(QString::fromUtf8("按显示名或进程名筛选应用"));
    connect(m_search, &QLineEdit::textChanged, this, &AppManagePage::filterApps);

    m_appList = new QListWidget(this);
    m_appList->setIconSize(QSize(kIconSize, kIconSize));
    m_appList->setUniformItemSizes(true);
    m_appList->setToolTip(QString::fromUtf8("已追踪过的应用；选中后在右侧调整名称、组别、合并与屏蔽"));
    connect(m_appList, &QListWidget::currentItemChanged,
            this, &AppManagePage::onAppSelected);

    listPanel->addWidget(m_search);
    listPanel->addWidget(m_appList, 1);
    split->addLayout(listPanel, 3);

    auto *detailPanel = new QVBoxLayout();
    detailPanel->setSpacing(DesignTokens::kSpacingSm);

    m_detailTitle = new QLabel(QString::fromUtf8("应用详情"), this);
    m_detailTitle->setObjectName(QStringLiteral("sectionTitle"));

    m_detailStats = new QLabel(this);
    m_detailStats->setObjectName(QStringLiteral("statusLabel"));
    m_detailStats->setWordWrap(true);

    m_renameBtn = new QPushButton(QString::fromUtf8("重命名"), this);
    m_renameBtn->setObjectName(QStringLiteral("secondaryBtn"));
    m_renameBtn->setToolTip(QString::fromUtf8("自定义该应用的显示名称，统计与排行随之更新"));
    connect(m_renameBtn, &QPushButton::clicked, this, &AppManagePage::onRenameClicked);

    m_groupCombo = new QComboBox(this);
    m_groupCombo->setToolTip(QString::fromUtf8("把该应用归入某个组别；组别排行按此聚合"));
    connect(m_groupCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AppManagePage::onGroupChanged);

    m_mergeCombo = new QComboBox(this);
    m_mergeCombo->setToolTip(QString::fromUtf8("把该应用的时长并入另一个应用，合并计时；可随时解除"));
    connect(m_mergeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AppManagePage::onMergeChanged);

    m_ignoreCheck = new QCheckBox(QString::fromUtf8("屏蔽此应用，不再追踪"), this);
    m_ignoreCheck->setToolTip(QString::fromUtf8("屏蔽后该应用不再记录会话；已有历史数据仍可通过取消屏蔽恢复统计"));
    connect(m_ignoreCheck, &QCheckBox::toggled,
            this, &AppManagePage::onIgnoreToggled);

    auto addRow = [this](QVBoxLayout *layout, const QString &labelText, QWidget *field) {
        auto *row = new QHBoxLayout();
        row->setSpacing(DesignTokens::kSpacingSm);
        auto *label = new QLabel(labelText, this);
        label->setObjectName(QStringLiteral("statusLabel"));
        label->setMinimumWidth(kDetailLabelWidth);
        row->addWidget(label);
        row->addWidget(field, 1);
        layout->addLayout(row);
    };

    addRow(detailPanel, QString::fromUtf8("所属组别"), m_groupCombo);
    addRow(detailPanel, QString::fromUtf8("合并到"), m_mergeCombo);

    detailPanel->addWidget(m_renameBtn);
    detailPanel->addWidget(m_ignoreCheck);
    detailPanel->addWidget(m_detailStats);
    detailPanel->addStretch(1);

    m_detailHint = new QLabel(
        QString::fromUtf8("在左侧选择一个应用以调整其名称、组别、合并与屏蔽状态。"), this);
    m_detailHint->setObjectName(QStringLiteral("statusLabel"));
    m_detailHint->setWordWrap(true);
    m_detailHint->setAlignment(Qt::AlignTop);
    detailPanel->insertWidget(1, m_detailHint);

    split->addLayout(detailPanel, 4);
    root->addLayout(split, 1);

    // ---- 组别管理 ----
    auto *groupPanel = new QVBoxLayout();
    groupPanel->setSpacing(DesignTokens::kSpacingSm);

    auto *groupHeader = new QHBoxLayout();
    groupHeader->setSpacing(DesignTokens::kSpacingSm);
    auto *groupTitle = new QLabel(QString::fromUtf8("组别管理"), this);
    groupTitle->setObjectName(QStringLiteral("sectionTitle"));
    groupHeader->addWidget(groupTitle);
    groupHeader->addStretch(1);

    auto *addGroupBtn = new QPushButton(QString::fromUtf8("+ 新建组别"), this);
    addGroupBtn->setObjectName(QStringLiteral("secondaryBtn"));
    addGroupBtn->setToolTip(QString::fromUtf8("新增一个自定义组别"));
    connect(addGroupBtn, &QPushButton::clicked, this, &AppManagePage::onAddGroup);
    groupHeader->addWidget(addGroupBtn);

    m_renameGroupBtn = new QPushButton(QString::fromUtf8("重命名"), this);
    m_renameGroupBtn->setObjectName(QStringLiteral("secondaryBtn"));
    connect(m_renameGroupBtn, &QPushButton::clicked, this, &AppManagePage::onRenameGroup);
    groupHeader->addWidget(m_renameGroupBtn);

    m_removeGroupBtn = new QPushButton(QString::fromUtf8("删除"), this);
    m_removeGroupBtn->setObjectName(QStringLiteral("secondaryBtn"));
    m_removeGroupBtn->setToolTip(QString::fromUtf8("删除组别，组内应用自动回到「未分组」"));
    connect(m_removeGroupBtn, &QPushButton::clicked, this, &AppManagePage::onRemoveGroup);
    groupHeader->addWidget(m_removeGroupBtn);

    m_groupList = new QListWidget(this);
    m_groupList->setToolTip(QString::fromUtf8("组别列表；未归入任何组别的应用会计入「未分组」"));
    m_groupList->setFixedHeight(132);
    m_groupList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_groupList, &QListWidget::itemSelectionChanged,
            this, &AppManagePage::updateGroupActions);

    groupPanel->addLayout(groupHeader);
    groupPanel->addWidget(m_groupList);
    root->addLayout(groupPanel);

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
    const QString focus = DesignTokens::kFocusBorder().name(QColor::HexArgb);
    const QString accent = DesignTokens::kAccent().name(QColor::HexArgb);
    const QString hoverBg = DesignTokens::kButtonHoverBg().name(QColor::HexArgb);

    return QStringLiteral(
        // 透明背景：页面嵌入对话框的滚动区，底色由对话框统一绘制。
        "QWidget { background: transparent; }"
        "QLabel#sectionTitle { color: %4; font-size: 12px; font-weight: 600; background: transparent; }"
        "QLabel#statusLabel { color: %5; font-size: 12px; background: transparent; }"
        "QLineEdit { background: %1; color: %3; border: 1px solid %2; border-radius: 6px;"
        " padding: 6px 9px; selection-background-color: %7; }"
        "QLineEdit:focus { border-color: %6; }"
        "QListWidget { background: %1; color: %3; border: 1px solid %2; border-radius: 8px;"
        " padding: 4px; outline: 0; }"
        "QListWidget::item { padding: 6px 8px; border-radius: 6px; margin: 1px 2px; }"
        "QListWidget::item:selected { background: %7; color: %3; }"
        "QListWidget::item:hover:!selected { background: %8; }"
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
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background: %9; min-height: 24px; border-radius: 3px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
        .arg(surface, border, text, textMute, textFaint, focus, accent, hoverBg,
             DesignTokens::kTextFaint().name(QColor::HexArgb));
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
        const QString path = item->data(Qt::UserRole + 1).toString();
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
        auto *item = new QListWidgetItem(itemLabel(entry), m_appList);
        item->setData(Qt::UserRole, entry.processKey);
        item->setData(Qt::UserRole + 1, entry.processName);
        if (entry.ignored)
            item->setForeground(DesignTokens::kTextFaint());
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
        m_detailTitle->setText(QString::fromUtf8("应用详情"));
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
        // 目标已被其它应用合并时不会出现在候选里，补一项保证回显不丢。
        m_mergeCombo->addItem(entry.mergedInto, entry.mergedInto);
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
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QString AppManagePage::mergeTargetFor(int index) const
{
    return m_mergeCombo->itemData(index).toString();
}

void AppManagePage::filterApps(const QString &text)
{
    const QString needle = text.trimmed();
    for (int i = 0; i < m_appList->count(); ++i) {
        QListWidgetItem *item = m_appList->item(i);
        if (needle.isEmpty()) {
            item->setHidden(false);
            continue;
        }
        const bool hit = item->text().contains(needle, Qt::CaseInsensitive)
                         || item->data(Qt::UserRole).toString().contains(needle, Qt::CaseInsensitive);
        item->setHidden(!hit);
    }
}

void AppManagePage::onAppSelected(QListWidgetItem *, QListWidgetItem *)
{
    refreshDetails();
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
    if (trimmed.isEmpty())
        return;
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
    if (trimmed.isEmpty())
        return;
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
