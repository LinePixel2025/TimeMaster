#include "settings_dialog.h"
#include "database/database_manager.h"
#include "utility/autostart_helper.h"
#include "icon/app_icon_provider.h"
#include "ui/theme_manager.h"
#include "ui/design_tokens.h"
#include "ui/ui_utils.h"
#include "ui/settings_icons.h"
#include "utility/process_identity.h"
#include "push/lineweb_pusher.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QColorDialog>
#include <QHeaderView>
#include <QShortcut>
#include <QFrame>
#include <QButtonGroup>
#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QApplication>
#include <QTimer>
#include <QDialogButtonBox>

namespace {

// 左侧边栏导航项定义：图标种类、显示名、悬停提示。
struct NavDef {
    SettingsIcons::Kind kind;
    QString text;
    QString tooltip;
};

const QList<NavDef> kNavDefs = {
    { SettingsIcons::Apps,    QString::fromUtf8("应用管理"), QString::fromUtf8("管理已识别应用、屏蔽不需要统计的应用，以及应用别名") },
    { SettingsIcons::Timer,   QString::fromUtf8("追踪设置"), QString::fromUtf8("调整前台窗口追踪的总开关与计时参数") },
    { SettingsIcons::Palette, QString::fromUtf8("个性化"),   QString::fromUtf8("外观主题、开机自启与本地默认目标") },
    { SettingsIcons::Bell,    QString::fromUtf8("提醒"),     QString::fromUtf8("定时使用提醒与每周周报的设置") },
    { SettingsIcons::Cloud,   QString::fromUtf8("云端同步"), QString::fromUtf8("向云端推送当日时长并同步云端每日目标") },
    { SettingsIcons::Sparkle, QString::fromUtf8("AI 智能"),  QString::fromUtf8("AI 报告、提醒文案与周报的接口配置") },
    { SettingsIcons::Info,    QString::fromUtf8("关于"),     QString::fromUtf8("应用名称、版本与简介") },
};

// 主题色预设：hex 为空串表示内置默认玉色（accent_color 存空串）。
struct AccentPreset {
    const char *name;
    const char *hex;
};

const QList<AccentPreset> kAccentPresets = {
    { "默认玉绿", ""        },
    { "海蓝",     "#1565C0" },
    { "靛紫",     "#5E35B1" },
    { "玫红",     "#C2185B" },
    { "珊瑚",     "#D84315" },
    { "琥珀",     "#B26A00" },
    { "青碧",     "#00838F" },
    { "石墨",     "#455A64" },
};

/// 预设色块的展示色：默认玉绿固定显示内置 brand 色。
QString accentPresetHex(const AccentPreset &preset)
{
    return *preset.hex ? QString::fromLatin1(preset.hex)
                       : ThemeManager::defaultAccent().name();
}

// 对话框全局样式。所有颜色取自 DesignTokens，主题切换时整体重设，
// 避免逐控件 setStyleSheet 造成旧主题颜色残留。
QString settingsStyle()
{
    const QString bg          = DesignTokens::kBg().name(QColor::HexArgb);
    const QString surface     = DesignTokens::kSurface().name(QColor::HexArgb);
    const QString border      = DesignTokens::kBorder().name(QColor::HexArgb);
    const QString text        = DesignTokens::kText().name(QColor::HexArgb);
    const QString textStrong  = DesignTokens::kTextStrong().name(QColor::HexArgb);
    const QString textMute    = DesignTokens::kTextMute().name(QColor::HexArgb);
    const QString textFaint   = DesignTokens::kTextFaint().name(QColor::HexArgb);
    const QString accent      = DesignTokens::kAccent().name(QColor::HexArgb);
    const QString onAccent    = DesignTokens::kOnAccent().name(QColor::HexArgb);
    const QString focus       = DesignTokens::kFocusBorder().name(QColor::HexArgb);
    const QString accentHover = DesignTokens::kAccentHover().name(QColor::HexArgb);
    const QString accentPress = DesignTokens::kAccentPressed().name(QColor::HexArgb);
    const QString accentLight = DesignTokens::kAccentLight().name(QColor::HexArgb);
    const QString hoverBg     = DesignTokens::kButtonHoverBg().name(QColor::HexArgb);

    // 圆形主题色块的底色无法用单一 QSS 表达，按 swatchHex 属性逐预设生成规则。
    QString swatchRules;
    for (const AccentPreset &preset : kAccentPresets) {
        const QString hex = accentPresetHex(preset);
        swatchRules += QStringLiteral(
            "QPushButton#accentSwatch[swatchHex=\"%1\"] { background: %1; }").arg(hex);
    }

    return QStringLiteral(
        "QDialog { background: %1; }"
        "QFrame#sidebar { background: %2; border-right: 1px solid %3; }"
        "QFrame#sectionCard { background: %2; border: 1px solid %3; border-radius: 8px; }"
        "QLabel#sideTitle { color: %5; background: transparent; }"

        "QLabel#pageTitle { color: %5; font-size: 17px; font-weight: 600; background: transparent; }"
        "QLabel#sectionTitle { color: %7; font-size: 12px; font-weight: 600; background: transparent; }"
        "QLabel#statusLabel { color: %7; font-size: 12px; background: transparent; }"
        "QLabel#aboutName { color: %5; background: transparent; }"
        "QLabel#aboutVersion { color: %7; background: transparent; }"
        "QLabel#aboutDesc { color: %8; background: transparent; }"

        "QPushButton#navItem { background: transparent; color: %4; border: 1px solid transparent; border-radius: 6px;"
        " text-align: left; padding: 0 12px; font-size: 13px; }"
        "QPushButton#navItem:hover { background: %13; }"
        "QPushButton#navItem:checked { background: %12; color: %6; font-weight: 600; }"
        "QPushButton#navItem:focus { border-color: %14; }"
        "QPushButton#navItem:disabled { color: %8; }"

        "QPushButton#accentBtn { background: %6; color: %15; border: 1px solid transparent; border-radius: 6px;"
        " padding: 8px 22px; font-size: 13px; font-weight: 600; }"
        "QPushButton#accentBtn:hover { background: %10; }"
        "QPushButton#accentBtn:pressed { background: %11; }"
        "QPushButton#accentBtn:focus { border-color: %14; }"
        "QPushButton#accentBtn:disabled { background: %3; color: %8; }"

        "QPushButton#secondaryBtn { background: %2; color: %4; border: 1px solid %3;"
        " border-radius: 6px; padding: 7px 14px; font-size: 13px; }"
        "QPushButton#secondaryBtn:hover { background: %13; }"
        "QPushButton#secondaryBtn:pressed { background: %12; }"
        "QPushButton#secondaryBtn:focus { border-color: %14; }"
        "QPushButton#secondaryBtn:disabled { color: %8; background: transparent; }"

        "QLineEdit, QSpinBox, QComboBox, QTimeEdit { color: %5; background: %2;"
        " border: 1px solid %3; border-radius: 6px; padding: 7px 9px; min-height: 18px;"
        " selection-background-color: %12; }"
        "QLineEdit:hover, QSpinBox:hover, QComboBox:hover, QTimeEdit:hover { border-color: %8; }"
        "QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QTimeEdit:focus { border-color: %14; }"
        "QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled, QTimeEdit:disabled { color: %8; background: %13; }"
        "QLineEdit::placeholder { color: %8; }"

        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox::down-arrow { image: none; width: 0; height: 0;"
        " border-left: 4px solid transparent; border-right: 4px solid transparent;"
        " border-top: 5px solid %7; }"
        "QComboBox QAbstractItemView { background: %2; color: %4; border: 1px solid %3;"
        " selection-background-color: %12; selection-color: %5; outline: 0; }"

        "QSpinBox::up-button, QSpinBox::down-button, QTimeEdit::up-button, QTimeEdit::down-button {"
        " border: none; background: transparent; width: 18px; }"
        "QSpinBox::up-arrow { image: none; width: 0; height: 0;"
        " border-left: 3px solid transparent; border-right: 3px solid transparent;"
        " border-bottom: 4px solid %7; }"
        "QSpinBox::down-arrow { image: none; width: 0; height: 0;"
        " border-left: 3px solid transparent; border-right: 3px solid transparent;"
        " border-top: 4px solid %7; }"
        "QSpinBox::up-arrow:disabled, QSpinBox::down-arrow:disabled { border-bottom-color: %8; }"
        "QSpinBox::down-arrow:disabled { border-top-color: %8; }"

        "QCheckBox { color: %4; spacing: 12px; padding: 8px 0; }"
        "QCheckBox:disabled { color: %8; }"
        "QCheckBox::indicator { width: 36px; height: 20px; border-radius: 10px; }"
        "QCheckBox::indicator:unchecked { background: %13; border: 1px solid %3; }"
        "QCheckBox::indicator:unchecked:hover { border-color: %6; }"
        "QCheckBox::indicator:checked { background: %6; border: 1px solid %6; }"
        "QCheckBox::indicator:checked:hover { background: %10; border-color: %10; }"
        "QCheckBox::indicator:disabled { background: %13; border-color: %3; }"

        "QListWidget, QTableWidget { color: %4; background: %2; border: 1px solid %3;"
        " border-radius: 8px; alternate-background-color: %13; outline: 0; }"
        "QListWidget::item { padding: 7px 10px; border-radius: 6px; }"
        "QListWidget::item:hover, QTableWidget::item:hover { background: %13; }"
        "QListWidget::item:selected, QTableWidget::item:selected { background: %12; color: %5; }"
        "QHeaderView::section { color: %7; background: %2; border: none;"
        " border-bottom: 1px solid %3; padding: 8px; font-weight: 600; }"
        "QTableWidget { gridline-color: %3; }"

        "QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }"
        "QScrollBar::handle:vertical { background: %8; border-radius: 5px; min-height: 24px; }"
        "QScrollBar::handle:vertical:hover { background: %7; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
        "QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }"
        "QScrollBar::handle:horizontal { background: %8; border-radius: 5px; min-width: 24px; }"
        "QScrollBar::handle:horizontal:hover { background: %7; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"

        "QPushButton#accentSwatch { border: 2px solid transparent; border-radius: 11px; }"
        "QPushButton#accentSwatch:hover { border-color: %7; }"
        "QPushButton#accentSwatch:checked { border-color: %5; }")
        .arg(bg)
        .arg(surface)
        .arg(border)
        .arg(text)
        .arg(textStrong)
        .arg(accent)
        .arg(textMute)
        .arg(textFaint)
        .arg(accentHover)
        .arg(accentPress)
        .arg(accentLight)
        .arg(hoverBg)
        .arg(focus)
        .arg(onAccent)
        + swatchRules;
}

} // namespace

SettingsDialog::SettingsDialog(DatabaseManager *db, QWidget *parent)
    : QDialog(parent), m_db(db)
{
    setWindowTitle(QString::fromUtf8("设置"));
    resize(980, 640);
    setMinimumSize(880, 560);

    // 初始 palette 与样式（themeChanged 时由 applyTheme 整体重设）。
    applyTheme();

    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ================= 左侧边栏导航 =================
    auto *sidebar = new QFrame(this);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFrameShape(QFrame::NoFrame);
    sidebar->setFixedWidth(190);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(14, 20, 14, 16);
    sideLayout->setSpacing(4);

    auto *sideTitle = new QLabel(QString::fromUtf8("设置"), sidebar);
    sideTitle->setObjectName(QStringLiteral("sideTitle"));
    sideTitle->setFont(DesignTokens::appFont(16, QFont::DemiBold));
    sideLayout->addWidget(sideTitle);
    sideLayout->addSpacing(10);

    auto *navGroup = new QButtonGroup(this);
    navGroup->setExclusive(true);
    for (int i = 0; i < kNavDefs.size(); ++i) {
        auto *btn = new QPushButton(kNavDefs[i].text, sidebar);
        btn->setObjectName(QStringLiteral("navItem"));
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(38);
        btn->setIconSize(QSize(17, 17));
        btn->setIcon(SettingsIcons::navIcon(kNavDefs[i].kind, false));
        btn->setToolTip(kNavDefs[i].tooltip);
        sideLayout->addWidget(btn);
        m_navButtons.append(btn);
        navGroup->addButton(btn, i);
    }
    sideLayout->addStretch();
    rootLayout->addWidget(sidebar);

    // ================= 右侧内容区 =================
    auto *content = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(20, 20, 20, 16);
    contentLayout->setSpacing(14);

    m_stack = new QStackedWidget(content);

    // 页面通用辅助：新建页面并添加页面标题。
    auto startPage = [this](const QString &title) {
        auto *page = new QWidget(this);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(14);
        auto *titleLabel = new QLabel(title, page);
        titleLabel->setObjectName(QStringLiteral("pageTitle"));
        layout->addWidget(titleLabel);
        return layout;
    };

    // 卡片辅助：带分区小标题的 Surface 卡片。
    auto addSectionCard = [](QVBoxLayout *pageLayout, const QString &title,
                             QWidget *parent, QVBoxLayout **cardLayoutOut,
                             int stretch = 0) {
        auto *card = new QFrame(parent);
        card->setObjectName(QStringLiteral("sectionCard"));
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 14, 16, 14);
        cardLayout->setSpacing(8);
        auto *titleLabel = new QLabel(title, parent);
        titleLabel->setObjectName(QStringLiteral("sectionTitle"));
        cardLayout->addWidget(titleLabel);
        pageLayout->addWidget(card, stretch);
        if (cardLayoutOut)
            *cardLayoutOut = cardLayout;
    };

    // 表单行辅助：label 固定宽度对齐 + 控件（fill 为 true 时占满剩余宽度）。
    auto addFormRow = [](QVBoxLayout *cardLayout, const QString &labelText,
                         const QString &tooltip, QWidget *field,
                         bool fill, QWidget *parent) {
        auto *row = new QHBoxLayout();
        row->setSpacing(12);
        auto *label = new QLabel(labelText, parent);
        label->setFont(DesignTokens::appFont(13));
        label->setMinimumWidth(160);
        if (!tooltip.isEmpty()) {
            label->setToolTip(tooltip);
            field->setToolTip(tooltip);
        }
        row->addWidget(label);
        row->addWidget(field, fill ? 1 : 0);
        if (!fill)
            row->addStretch(1);
        cardLayout->addLayout(row);
    };

    // 主开关联动：开启时才允许操作关联控件。
    auto bindToggle = [this](QCheckBox *check, const QList<QWidget *> &targets) {
        const auto apply = [targets](bool on) {
            for (QWidget *w : targets)
                w->setEnabled(on);
        };
        apply(check->isChecked());
        connect(check, &QCheckBox::toggled, this, apply);
    };

    // ================= 页面 1：应用管理 =================
    {
        QVBoxLayout *pageLayout = startPage(QString::fromUtf8("应用管理"));
        QVBoxLayout *cardLayout = nullptr;
        addSectionCard(pageLayout,
                       QString::fromUtf8("屏蔽不需要统计的应用"),
                       this, &cardLayout, 1);

        auto *splitLayout = new QHBoxLayout();
        splitLayout->setSpacing(10);

        auto buildPanel = [this](const QString &labelText, const QString &tip,
                                 QLineEdit **searchOut,
                                 QListWidget **listOut,
                                 bool multiSelect) {
            auto *panel = new QVBoxLayout();
            panel->setSpacing(6);
            auto *label = new QLabel(labelText, this);
            label->setFont(DesignTokens::appFont(12, QFont::Medium));
            label->setToolTip(tip);
            panel->addWidget(label);

            auto *search = new QLineEdit(this);
            search->setPlaceholderText(QString::fromUtf8("搜索..."));
            search->setToolTip(tip);
            panel->addWidget(search);

            auto *list = new QListWidget(this);
            list->setToolTip(tip);
            list->setSelectionMode(multiSelect
                ? QAbstractItemView::MultiSelection
                : QAbstractItemView::SingleSelection);
            panel->addWidget(list, 1);

            if (searchOut) *searchOut = search;
            if (listOut) *listOut = list;
            return panel;
        };

        const QString knownTip =
            QString::fromUtf8("已识别到的前台应用；可多选后点击「加入屏蔽」不再统计");
        const QString ignoredTip =
            QString::fromUtf8("被屏蔽的应用；选中后点击「移除屏蔽」或按 Delete 键恢复统计");
        splitLayout->addLayout(buildPanel(
            QString::fromUtf8("已知应用"), knownTip,
            &m_knownSearch, &m_knownAppsList, true), 1);
        connect(m_knownSearch, &QLineEdit::textChanged,
                this, &SettingsDialog::filterKnownApps);

        auto *centerPanel = new QVBoxLayout();
        centerPanel->setSpacing(8);
        centerPanel->addStretch();

        auto *addIgnoredBtn = new QPushButton(
            QString::fromUtf8("→ 加入屏蔽"), this);
        addIgnoredBtn->setObjectName(QStringLiteral("secondaryBtn"));
        addIgnoredBtn->setToolTip(knownTip);
        connect(addIgnoredBtn, &QPushButton::clicked,
                this, &SettingsDialog::onAddIgnored);
        centerPanel->addWidget(addIgnoredBtn);

        auto *removeIgnoredBtn = new QPushButton(
            QString::fromUtf8("← 移除屏蔽"), this);
        removeIgnoredBtn->setObjectName(QStringLiteral("secondaryBtn"));
        removeIgnoredBtn->setToolTip(ignoredTip);
        connect(removeIgnoredBtn, &QPushButton::clicked,
                this, &SettingsDialog::onRemoveIgnored);
        centerPanel->addWidget(removeIgnoredBtn);

        centerPanel->addStretch();
        splitLayout->addLayout(centerPanel);

        splitLayout->addLayout(buildPanel(
            QString::fromUtf8("已屏蔽应用"), ignoredTip,
            &m_ignoredSearch, &m_ignoredAppsList, false), 1);
        connect(m_ignoredSearch, &QLineEdit::textChanged,
                this, &SettingsDialog::filterIgnoredApps);

        auto *delShortcut = new QShortcut(QKeySequence::Delete, m_ignoredAppsList);
        connect(delShortcut, &QShortcut::activated, this, [this]() {
            if (m_ignoredAppsList->currentItem())
                onRemoveIgnored();
        });

        cardLayout->addLayout(splitLayout, 1);

        // ---- 应用别名 ----
        QVBoxLayout *aliasCardLayout = nullptr;
        addSectionCard(pageLayout,
                       QString::fromUtf8("应用名称别名"),
                       this, &aliasCardLayout, 0);

        m_aliasTable = new QTableWidget(0, 2, this);
        m_aliasTable->setHorizontalHeaderLabels({
            QString::fromUtf8("进程名"),
            QString::fromUtf8("显示名")
        });
        m_aliasTable->horizontalHeader()->setStretchLastSection(true);
        m_aliasTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_aliasTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_aliasTable->verticalHeader()->setVisible(false);
        m_aliasTable->setMinimumHeight(130);
        m_aliasTable->setToolTip(
            QString::fromUtf8("把进程名显示为更友好的名称；选中一行后可编辑或删除"));
        aliasCardLayout->addWidget(m_aliasTable);

        auto *aliasBtnRow = new QHBoxLayout();
        aliasBtnRow->setSpacing(8);
        auto *addAliasBtn = new QPushButton(QString::fromUtf8("添加别名"), this);
        addAliasBtn->setObjectName(QStringLiteral("secondaryBtn"));
        addAliasBtn->setToolTip(QString::fromUtf8("为某个进程名设置自定义显示名称"));
        connect(addAliasBtn, &QPushButton::clicked,
                this, &SettingsDialog::onAddAlias);
        aliasBtnRow->addWidget(addAliasBtn);

        auto *editAliasBtn = new QPushButton(QString::fromUtf8("编辑"), this);
        editAliasBtn->setObjectName(QStringLiteral("secondaryBtn"));
        editAliasBtn->setToolTip(QString::fromUtf8("修改选中别名的显示名称"));
        connect(editAliasBtn, &QPushButton::clicked,
                this, &SettingsDialog::onEditAlias);
        aliasBtnRow->addWidget(editAliasBtn);

        auto *deleteAliasBtn = new QPushButton(QString::fromUtf8("删除"), this);
        deleteAliasBtn->setObjectName(QStringLiteral("secondaryBtn"));
        deleteAliasBtn->setToolTip(QString::fromUtf8("移除选中的别名，恢复默认显示名称"));
        connect(deleteAliasBtn, &QPushButton::clicked,
                this, &SettingsDialog::onDeleteAlias);
        aliasBtnRow->addWidget(deleteAliasBtn);
        aliasBtnRow->addStretch();
        aliasCardLayout->addLayout(aliasBtnRow);

        m_stack->addWidget(pageLayout->parentWidget());
    }

    // ================= 页面 2：追踪设置 =================
    {
        QVBoxLayout *pageLayout = startPage(QString::fromUtf8("追踪设置"));

        QVBoxLayout *switchCardLayout = nullptr;
        addSectionCard(pageLayout, QString::fromUtf8("追踪开关"),
                       this, &switchCardLayout, 0);
        m_trackingEnabled = new QCheckBox(
            QString::fromUtf8("启用追踪"), this);
        m_trackingEnabled->setFont(DesignTokens::appFont(13));
        m_trackingEnabled->setToolTip(
            QString::fromUtf8("开启后开始记录前台窗口的使用时长；关闭则不产生任何记录"));
        switchCardLayout->addWidget(m_trackingEnabled);

        QVBoxLayout *paramCardLayout = nullptr;
        addSectionCard(pageLayout, QString::fromUtf8("计时参数"),
                       this, &paramCardLayout, 0);

        const QString pollTip =
            QString::fromUtf8("间隔多久检测一次当前前台窗口（秒）。值越小统计越精确，占用的系统开销略高");
        const QString idleTip =
            QString::fromUtf8("鼠标键盘连续空闲超过该时长即判定为离开（秒），空闲期间不计入使用时长");
        const QString minTrackTip =
            QString::fromUtf8("窗口切换后活跃超过该时长才开始计时（秒）；0 表示不限制，立即计时");
        const QString minRecordTip =
            QString::fromUtf8("单次使用时长低于该值的记录不计入统计和导出（秒）；0 表示不限制");

        addFormRow(paramCardLayout, QString::fromUtf8("轮询间隔（秒）："), pollTip,
                   m_pollInterval = new QSpinBox(this), false, this);
        m_pollInterval->setRange(1, 10);
        m_pollInterval->setValue(1);
        m_pollInterval->setSingleStep(1);

        addFormRow(paramCardLayout, QString::fromUtf8("空闲判定时间（秒）："), idleTip,
                   m_idleThreshold = new QSpinBox(this), false, this);
        m_idleThreshold->setRange(10, 600);
        m_idleThreshold->setValue(60);
        m_idleThreshold->setSingleStep(10);

        addFormRow(paramCardLayout, QString::fromUtf8("最低计时阈值（秒）："), minTrackTip,
                   m_minTrackingSeconds = new QSpinBox(this), false, this);
        m_minTrackingSeconds->setRange(0, 30);
        m_minTrackingSeconds->setValue(0);
        m_minTrackingSeconds->setSingleStep(1);

        addFormRow(paramCardLayout, QString::fromUtf8("最低记录阈值（秒）："), minRecordTip,
                   m_minRecordThreshold = new QSpinBox(this), false, this);
        m_minRecordThreshold->setRange(0, 300);
        m_minRecordThreshold->setValue(40);
        m_minRecordThreshold->setSingleStep(5);
        m_minRecordThreshold->setSuffix(QString::fromUtf8(" 秒"));

        // 追踪关闭时参数置灰，保持层级清晰。
        bindToggle(m_trackingEnabled, {m_pollInterval, m_idleThreshold,
                                       m_minTrackingSeconds, m_minRecordThreshold});

        pageLayout->addStretch(1);
        m_stack->addWidget(pageLayout->parentWidget());
    }

    // ================= 页面 3：个性化 =================
    {
        QVBoxLayout *pageLayout = startPage(QString::fromUtf8("个性化"));

        QVBoxLayout *appearanceCardLayout = nullptr;
        addSectionCard(pageLayout, QString::fromUtf8("外观"),
                       this, &appearanceCardLayout, 0);
        m_darkMode = new QCheckBox(
            QString::fromUtf8("暗色模式"), this);
        m_darkMode->setFont(DesignTokens::appFont(13));
        m_darkMode->setToolTip(
            QString::fromUtf8("切换应用整体配色为暗色或亮色"));
        appearanceCardLayout->addWidget(m_darkMode);

        // 主题色：预设色块 + 自定义取色。点击即时预览（不落库），
        // 保存时 commitAccent 落库，取消/Esc 由 reject() 还原。
        auto *accentRow = new QHBoxLayout();
        accentRow->setSpacing(8);
        auto *accentLabel = new QLabel(QString::fromUtf8("主题色:"), this);
        accentLabel->setFont(DesignTokens::appFont(13));
        accentLabel->setToolTip(
            QString::fromUtf8("主界面与报告的强调色；支持预设与自定义取色"));
        accentRow->addWidget(accentLabel);

        for (int i = 0; i < kAccentPresets.size(); ++i) {
            const AccentPreset &preset = kAccentPresets[i];
            auto *swatch = new QPushButton(this);
            swatch->setObjectName(QStringLiteral("accentSwatch"));
            swatch->setCheckable(true);
            swatch->setFixedSize(22, 22);
            swatch->setCursor(Qt::PointingHandCursor);
            swatch->setToolTip(QString::fromUtf8(preset.name));
            swatch->setProperty("swatchHex", accentPresetHex(preset));
            connect(swatch, &QPushButton::clicked, this, [this, i]() {
                const char *hex = kAccentPresets[i].hex;
                ThemeManager::instance()->setAccentColor(
                    *hex ? QColor(QString::fromLatin1(hex)) : QColor(), false);
                updateAccentSwatches();
            });
            accentRow->addWidget(swatch);
            m_accentSwatches.append(swatch);
        }

        auto *accentCustomBtn = new QPushButton(
            QString::fromUtf8("自定义…"), this);
        accentCustomBtn->setObjectName(QStringLiteral("secondaryBtn"));
        accentCustomBtn->setCursor(Qt::PointingHandCursor);
        accentCustomBtn->setToolTip(
            QString::fromUtf8("从调色板中选取任意主题色"));
        connect(accentCustomBtn, &QPushButton::clicked, this, [this]() {
            const QColor initial = ThemeManager::instance()->hasCustomAccent()
                ? ThemeManager::instance()->accentColor()
                : ThemeManager::defaultAccent();
            const QColor picked = QColorDialog::getColor(
                initial, this, QString::fromUtf8("自定义主题色"));
            if (!picked.isValid())
                return;
            ThemeManager::instance()->setAccentColor(picked, false);
            updateAccentSwatches();
        });
        accentRow->addWidget(accentCustomBtn);
        accentRow->addStretch();
        appearanceCardLayout->addLayout(accentRow);

        const QString trendTip =
            QString::fromUtf8("主页「本周趋势」的展示形式；热力图可切换周/月");
        addFormRow(appearanceCardLayout,
                   QString::fromUtf8("趋势展示形式："), trendTip,
                   m_trendFormat = new QComboBox(this), false, this);
        m_trendFormat->addItems({
            QString::fromUtf8("标准图表（柱状/折线）"),
            QString::fromUtf8("热力图"),
        });

        QVBoxLayout *startCardLayout = nullptr;
        addSectionCard(pageLayout, QString::fromUtf8("启动与目标"),
                       this, &startCardLayout, 0);
        m_autoStart = new QCheckBox(
            QString::fromUtf8("开机自启"), this);
        m_autoStart->setFont(DesignTokens::appFont(13));
        m_autoStart->setToolTip(
            QString::fromUtf8("登录 Windows 后自动在后台启动并驻留系统托盘"));
        startCardLayout->addWidget(m_autoStart);

        const QString goalTip =
            QString::fromUtf8("本地默认的每日目标时长（小时）；云端已设置目标时以云端为准");
        addFormRow(startCardLayout,
                   QString::fromUtf8("本地默认目标（小时）："), goalTip,
                   m_dailyGoal = new QSpinBox(this), false, this);
        m_dailyGoal->setRange(1, 24);
        m_dailyGoal->setValue(8);

        pageLayout->addStretch(1);
        m_stack->addWidget(pageLayout->parentWidget());
    }

    // ================= 页面 4：提醒 =================
    {
        QVBoxLayout *pageLayout = startPage(QString::fromUtf8("提醒"));

        QVBoxLayout *remindCardLayout = nullptr;
        addSectionCard(pageLayout, QString::fromUtf8("定时提醒"),
                       this, &remindCardLayout, 1);

        m_reminderEnabled = new QCheckBox(
            QString::fromUtf8("启用定时提醒"), this);
        m_reminderEnabled->setFont(DesignTokens::appFont(13));
        m_reminderEnabled->setToolTip(
            QString::fromUtf8("在下方配置的时间点提醒使用情况"));
        remindCardLayout->addWidget(m_reminderEnabled);

        auto *addRow = new QHBoxLayout();
        addRow->setSpacing(8);
        auto *timeLabel = new QLabel(QString::fromUtf8("时间点:"), this);
        timeLabel->setFont(DesignTokens::appFont(13));
        timeLabel->setToolTip(
            QString::fromUtf8("选择一个时间点并点击「添加」，可配置多个提醒时间"));
        addRow->addWidget(timeLabel);
        m_reminderTimeEdit = new QTimeEdit(this);
        m_reminderTimeEdit->setDisplayFormat(QStringLiteral("HH:mm"));
        m_reminderTimeEdit->setTime(QTime::currentTime());
        m_reminderTimeEdit->setToolTip(timeLabel->toolTip());
        addRow->addWidget(m_reminderTimeEdit);
        m_reminderAddBtn = new QPushButton(QString::fromUtf8("添加"), this);
        m_reminderAddBtn->setObjectName(QStringLiteral("secondaryBtn"));
        m_reminderAddBtn->setToolTip(
            QString::fromUtf8("把左侧所选时间添加到提醒列表"));
        connect(m_reminderAddBtn, &QPushButton::clicked, this, [this]() {
            const QString time = m_reminderTimeEdit->time().toString(QStringLiteral("HH:mm"));
            for (int i = 0; i < m_reminderTimesList->count(); ++i) {
                if (m_reminderTimesList->item(i)->text() == time) {
                    m_reminderTimesList->setCurrentRow(i);
                    return; // 已存在，只选中不重复添加。
                }
            }
            m_reminderTimesList->addItem(time);
            m_reminderTimesList->sortItems();
        });
        addRow->addWidget(m_reminderAddBtn);
        m_reminderRemoveBtn = new QPushButton(QString::fromUtf8("删除"), this);
        m_reminderRemoveBtn->setObjectName(QStringLiteral("secondaryBtn"));
        m_reminderRemoveBtn->setToolTip(
            QString::fromUtf8("移除列表中选中的时间点"));
        connect(m_reminderRemoveBtn, &QPushButton::clicked, this, [this]() {
            delete m_reminderTimesList->takeItem(m_reminderTimesList->currentRow());
        });
        addRow->addWidget(m_reminderRemoveBtn);
        addRow->addStretch();
        remindCardLayout->addLayout(addRow);

        m_reminderTimesList = new QListWidget(this);
        m_reminderTimesList->setMinimumHeight(110);
        m_reminderTimesList->setToolTip(
            QString::fromUtf8("提醒时间点列表；选中一项后可点击「删除」移除"));
        remindCardLayout->addWidget(m_reminderTimesList, 1);

        m_reminderStatus = new QLabel(this);
        m_reminderStatus->setObjectName(QStringLiteral("statusLabel"));
        m_reminderStatus->setToolTip(
            QString::fromUtf8("最近一次提醒触发的时间，用于确认提醒是否正常生效"));
        remindCardLayout->addWidget(m_reminderStatus);

        bindToggle(m_reminderEnabled,
                   {m_reminderTimeEdit, m_reminderAddBtn, m_reminderRemoveBtn,
                    m_reminderTimesList});

        // ---- 间隔提醒 ----
        QVBoxLayout *intervalCardLayout = nullptr;
        addSectionCard(pageLayout, QString::fromUtf8("间隔提醒"),
                       this, &intervalCardLayout, 0);

        m_intervalReminderEnabled = new QCheckBox(
            QString::fromUtf8("启用间隔提醒"), this);
        m_intervalReminderEnabled->setFont(DesignTokens::appFont(13));
        m_intervalReminderEnabled->setToolTip(
            QString::fromUtf8("自启用时刻起，每隔设定的间隔提醒一次使用情况；修改间隔后重新计时"));
        intervalCardLayout->addWidget(m_intervalReminderEnabled);

        auto *intervalRow = new QHBoxLayout();
        intervalRow->setSpacing(8);
        auto *intervalLabel = new QLabel(QString::fromUtf8("提醒间隔:"), this);
        intervalLabel->setFont(DesignTokens::appFont(13));
        intervalLabel->setToolTip(
            QString::fromUtf8("两次间隔提醒之间的分钟数"));
        intervalRow->addWidget(intervalLabel);
        m_intervalReminderMinutes = new QSpinBox(this);
        m_intervalReminderMinutes->setRange(5, 240);
        m_intervalReminderMinutes->setValue(45);
        m_intervalReminderMinutes->setSuffix(QString::fromUtf8(" 分钟"));
        m_intervalReminderMinutes->setToolTip(intervalLabel->toolTip());
        intervalRow->addWidget(m_intervalReminderMinutes);
        intervalRow->addStretch();
        intervalCardLayout->addLayout(intervalRow);

        auto *intervalHint = new QLabel(
            QString::fromUtf8("启用后从当下开始计时，程序重启或修改间隔会重新计时"), this);
        intervalHint->setObjectName(QStringLiteral("statusLabel"));
        intervalCardLayout->addWidget(intervalHint);

        bindToggle(m_intervalReminderEnabled, {m_intervalReminderMinutes});

        // ---- 每周周报 ----
        QVBoxLayout *weeklyCardLayout = nullptr;
        addSectionCard(pageLayout, QString::fromUtf8("每周周报"),
                       this, &weeklyCardLayout, 0);

        m_weeklyReportEnabled = new QCheckBox(
            QString::fromUtf8("每周自动生成周使用日报"), this);
        m_weeklyReportEnabled->setFont(DesignTokens::appFont(13));
        m_weeklyReportEnabled->setToolTip(
            QString::fromUtf8("每周在下方时刻自动生成上一周的用时日报（HTML，托盘通知后可打开。启用 AI 时含 AI 分析，否则为本地报告）"));
        weeklyCardLayout->addWidget(m_weeklyReportEnabled);

        auto *weeklyDayRow = new QHBoxLayout();
        weeklyDayRow->setSpacing(8);
        auto *dayLabel = new QLabel(QString::fromUtf8("生成日:"), this);
        dayLabel->setFont(DesignTokens::appFont(13));
        dayLabel->setToolTip(QString::fromUtf8("每周固定在哪一天生成周报"));
        weeklyDayRow->addWidget(dayLabel);
        m_weeklyReportDay = new QComboBox(this);
        m_weeklyReportDay->addItems({
            QString::fromUtf8("周一"),
            QString::fromUtf8("周二"),
            QString::fromUtf8("周三"),
            QString::fromUtf8("周四"),
            QString::fromUtf8("周五"),
            QString::fromUtf8("周六"),
            QString::fromUtf8("周日"),
        });
        m_weeklyReportDay->setToolTip(dayLabel->toolTip());
        weeklyDayRow->addWidget(m_weeklyReportDay);
        auto *timeLabel2 = new QLabel(QString::fromUtf8("时刻:"), this);
        timeLabel2->setFont(DesignTokens::appFont(13));
        timeLabel2->setToolTip(QString::fromUtf8("在该时刻自动生成周报"));
        weeklyDayRow->addWidget(timeLabel2);
        m_weeklyReportTime = new QTimeEdit(this);
        m_weeklyReportTime->setDisplayFormat(QStringLiteral("HH:mm"));
        m_weeklyReportTime->setTime(QTime(9, 0));
        m_weeklyReportTime->setToolTip(timeLabel2->toolTip());
        weeklyDayRow->addWidget(m_weeklyReportTime);
        weeklyDayRow->addStretch();
        weeklyCardLayout->addLayout(weeklyDayRow);

        bindToggle(m_weeklyReportEnabled, {m_weeklyReportDay, m_weeklyReportTime});

        m_stack->addWidget(pageLayout->parentWidget());
    }

    // ================= 页面 5：云端同步 =================
    {
        QVBoxLayout *pageLayout = startPage(QString::fromUtf8("云端同步"));

        QVBoxLayout *configCardLayout = nullptr;
        addSectionCard(pageLayout, QString::fromUtf8("推送配置"),
                       this, &configCardLayout, 0);

        m_linewebEnabled = new QCheckBox(
            QString::fromUtf8("启用推送"), this);
        m_linewebEnabled->setFont(DesignTokens::appFont(13));
        m_linewebEnabled->setToolTip(
            QString::fromUtf8("开启后按设定间隔自动向云端推送当日总时长"));
        configCardLayout->addWidget(m_linewebEnabled);

        const QString endpointTip =
            QString::fromUtf8("LineWeb 服务地址，例如 https://your-server.com");
        addFormRow(configCardLayout, QString::fromUtf8("API 地址:"), endpointTip,
                   m_linewebEndpoint = new QLineEdit(this), true, this);
        m_linewebEndpoint->setPlaceholderText(QString::fromUtf8("https://your-server.com"));

        const QString tokenTip =
            QString::fromUtf8("服务端分配的推送令牌（X-Screen-Time-Token），点击「显示」可查看明文");
        auto *tokenRow = new QHBoxLayout();
        tokenRow->setSpacing(12);
        auto *tokenLabel = new QLabel(QString::fromUtf8("Token:"), this);
        tokenLabel->setFont(DesignTokens::appFont(13));
        tokenLabel->setMinimumWidth(160);
        tokenLabel->setToolTip(tokenTip);
        tokenRow->addWidget(tokenLabel);
        m_linewebToken = new QLineEdit(this);
        m_linewebToken->setEchoMode(QLineEdit::Password);
        m_linewebToken->setPlaceholderText(QStringLiteral("st_..."));
        m_linewebToken->setToolTip(tokenTip);
        tokenRow->addWidget(m_linewebToken, 1);
        m_linewebTokenToggle = new QPushButton(QString::fromUtf8("显示"), this);
        m_linewebTokenToggle->setObjectName(QStringLiteral("secondaryBtn"));
        m_linewebTokenToggle->setToolTip(
            QString::fromUtf8("点击显示/隐藏 Token 明文"));
        connect(m_linewebTokenToggle, &QPushButton::clicked, this, [this]() {
            const bool show = (m_linewebToken->echoMode() == QLineEdit::Password);
            m_linewebToken->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
            m_linewebTokenToggle->setText(
                show ? QString::fromUtf8("隐藏") : QString::fromUtf8("显示"));
        });
        tokenRow->addWidget(m_linewebTokenToggle);
        configCardLayout->addLayout(tokenRow);

        const QString intervalTip =
            QString::fromUtf8("自动推送的间隔（分钟）");
        addFormRow(configCardLayout, QString::fromUtf8("推送间隔（分钟）:"), intervalTip,
                   m_linewebInterval = new QSpinBox(this), false, this);
        m_linewebInterval->setRange(5, 30);
        m_linewebInterval->setValue(10);

        // 测试按钮先创建（稍后加入「状态与测试」卡片），用于联动置灰。
        m_linewebTestBtn = new QPushButton(QString::fromUtf8("连接测试"), this);
        m_linewebTestBtn->setObjectName(QStringLiteral("secondaryBtn"));
        m_linewebTestBtn->setToolTip(
            QString::fromUtf8("用当前配置连接云端并测试推送，成功后同步云端每日目标"));

        bindToggle(m_linewebEnabled,
                   {m_linewebEndpoint, m_linewebToken, m_linewebTokenToggle,
                    m_linewebInterval, m_linewebTestBtn});

        // ---- 状态与测试 ----
        QVBoxLayout *testCardLayout = nullptr;
        addSectionCard(pageLayout, QString::fromUtf8("状态与测试"),
                       this, &testCardLayout, 0);

        auto *testRow = new QHBoxLayout();
        testRow->setSpacing(10);
        connect(m_linewebTestBtn, &QPushButton::clicked, this, [this]() {
            const QString token = m_linewebToken->text().trimmed();
            const QString endpoint = m_linewebEndpoint->text().trimmed();
            if (token.isEmpty() || endpoint.isEmpty()) {
                QMessageBox::warning(this,
                    QString::fromUtf8("配置不完整"),
                    QString::fromUtf8("请先填写 API 地址和 Token"));
                return;
            }

            QJsonObject body;
            body["totalSeconds"] = m_db->getTodayTotal();
            body["date"] = QDate::currentDate().toString("yyyy-MM-dd");

            QUrl url(normalizeLineWebEndpoint(endpoint) + "/api/health/push");
            QNetworkRequest req(url);
            req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            req.setRawHeader("X-Screen-Time-Token", token.toUtf8());

            auto *nam = new QNetworkAccessManager(this);
            QNetworkReply *reply = nam->post(req, QJsonDocument(body).toJson());
            connect(reply, &QNetworkReply::finished, this, [this, reply, nam, endpoint, token]() {
                reply->deleteLater();
                nam->deleteLater();
                if (reply->error() == QNetworkReply::NoError) {
                    QMessageBox::information(this,
                        QString::fromUtf8("测试成功"),
                        QString::fromUtf8("连接测试成功！"));
                    // 推送验证成功后顺带拉取云端目标写回 daily_goal（云端优先、本地兜底）。
                    fetchGoalFromCloud(endpoint, token);
                } else {
                    QMessageBox::warning(this,
                        QString::fromUtf8("测试失败"),
                        QString::fromUtf8("连接失败：") + reply->errorString());
                }
            });
        });
        testRow->addWidget(m_linewebTestBtn);

        m_linewebStatus = new QLabel(this);
        m_linewebStatus->setObjectName(QStringLiteral("statusLabel"));
        m_linewebStatus->setToolTip(
            QString::fromUtf8("最近一次推送与云端目标拉取的时间"));
        testRow->addWidget(m_linewebStatus);
        testRow->addStretch();
        testCardLayout->addLayout(testRow);

        pageLayout->addStretch(1);
        m_stack->addWidget(pageLayout->parentWidget());
    }

    // ================= 页面 6：AI 智能 =================
    {
        QVBoxLayout *pageLayout = startPage(QString::fromUtf8("AI 智能"));

        QVBoxLayout *configCardLayout = nullptr;
        addSectionCard(pageLayout, QString::fromUtf8("AI 配置"),
                       this, &configCardLayout, 0);

        m_aiEnabled = new QCheckBox(
            QString::fromUtf8("启用 AI 报告"), this);
        m_aiEnabled->setFont(DesignTokens::appFont(13));
        m_aiEnabled->setToolTip(
            QString::fromUtf8("开启后主页报告、定时提醒文案与每周周报可使用 AI 生成"));
        configCardLayout->addWidget(m_aiEnabled);

        const QString aiEndpointTip =
            QString::fromUtf8("OpenAI 兼容接口地址，例如 https://api.deepseek.com");
        addFormRow(configCardLayout, QString::fromUtf8("API 地址:"), aiEndpointTip,
                   m_aiEndpoint = new QLineEdit(this), true, this);
        m_aiEndpoint->setPlaceholderText(QString::fromUtf8("https://api.deepseek.com"));

        const QString aiKeyTip =
            QString::fromUtf8("接口访问密钥（Bearer 认证），可通过服务商管理平台获取，点击「显示」可查看明文");
        auto *aiKeyRow = new QHBoxLayout();
        aiKeyRow->setSpacing(12);
        auto *aiKeyLabel = new QLabel(QString::fromUtf8("API Key:"), this);
        aiKeyLabel->setFont(DesignTokens::appFont(13));
        aiKeyLabel->setMinimumWidth(160);
        aiKeyLabel->setToolTip(aiKeyTip);
        aiKeyRow->addWidget(aiKeyLabel);
        m_aiApiKey = new QLineEdit(this);
        m_aiApiKey->setEchoMode(QLineEdit::Password);
        m_aiApiKey->setPlaceholderText(
            QString::fromUtf8("可通过管理平台获取，以 Bearer 方式验证"));
        m_aiApiKey->setToolTip(aiKeyTip);
        aiKeyRow->addWidget(m_aiApiKey, 1);
        m_aiApiKeyToggle = new QPushButton(QString::fromUtf8("显示"), this);
        m_aiApiKeyToggle->setObjectName(QStringLiteral("secondaryBtn"));
        m_aiApiKeyToggle->setToolTip(
            QString::fromUtf8("点击显示/隐藏 API Key 明文"));
        connect(m_aiApiKeyToggle, &QPushButton::clicked, this, [this]() {
            const bool show = (m_aiApiKey->echoMode() == QLineEdit::Password);
            m_aiApiKey->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
            m_aiApiKeyToggle->setText(
                show ? QString::fromUtf8("隐藏") : QString::fromUtf8("显示"));
        });
        aiKeyRow->addWidget(m_aiApiKeyToggle);
        configCardLayout->addLayout(aiKeyRow);

        const QString modelTip =
            QString::fromUtf8("使用的模型名，例如 deepseek-chat");
        addFormRow(configCardLayout, QString::fromUtf8("模型名:"), modelTip,
                   m_aiModel = new QLineEdit(this), true, this);
        m_aiModel->setPlaceholderText(QStringLiteral("deepseek-chat"));

        // 测试按钮先创建（稍后加入「连接测试」卡片），用于联动置灰。
        m_aiTestBtn = new QPushButton(QString::fromUtf8("连接测试"), this);
        m_aiTestBtn->setObjectName(QStringLiteral("secondaryBtn"));
        m_aiTestBtn->setToolTip(
            QString::fromUtf8("用当前配置请求接口的 /models 校验连通性"));

        bindToggle(m_aiEnabled,
                   {m_aiEndpoint, m_aiApiKey, m_aiApiKeyToggle, m_aiModel,
                    m_aiTestBtn});

        // ---- 连接测试 ----
        QVBoxLayout *testCardLayout = nullptr;
        addSectionCard(pageLayout, QString::fromUtf8("连接测试"),
                       this, &testCardLayout, 0);

        auto *aiTestRow = new QHBoxLayout();
        aiTestRow->setSpacing(10);
        connect(m_aiTestBtn, &QPushButton::clicked, this, [this]() {
            const QString key = m_aiApiKey->text().trimmed();
            const QString endpoint = m_aiEndpoint->text().trimmed();
            if (key.isEmpty() || endpoint.isEmpty()) {
                QMessageBox::warning(this,
                    QString::fromUtf8("配置不完整"),
                    QString::fromUtf8("请先填写 API 地址和 API Key"));
                return;
            }

            QString base = endpoint;
            while (base.endsWith(QLatin1Char('/')))
                base.chop(1);
            QNetworkRequest req(QUrl(base + "/models"));
            req.setRawHeader("Authorization", ("Bearer " + key).toUtf8());
            req.setTransferTimeout(15000);

            auto *nam = new QNetworkAccessManager(this);
            QNetworkReply *reply = nam->get(req);
            connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
                reply->deleteLater();
                nam->deleteLater();
                if (reply->error() == QNetworkReply::NoError) {
                    const QJsonObject obj =
                        QJsonDocument::fromJson(reply->readAll()).object();
                    QMessageBox::information(this,
                        QString::fromUtf8("测试成功"),
                        obj.contains(QStringLiteral("data"))
                            ? QString::fromUtf8("连接成功！当前服务器可用模型数：%1")
                                .arg(obj[QStringLiteral("data")].toArray().size())
                            : QString::fromUtf8("连接成功！"));
                } else {
                    QString err = reply->errorString();
                    const QJsonObject obj =
                        QJsonDocument::fromJson(reply->readAll()).object();
                    const QJsonObject errObj =
                        obj[QStringLiteral("error")].toObject();
                    if (!errObj.isEmpty() &&
                        errObj.contains(QStringLiteral("message")))
                        err = errObj[QStringLiteral("message")].toString();
                    QMessageBox::warning(this,
                        QString::fromUtf8("测试失败"),
                        QString::fromUtf8("连接失败：") + err);
                }
            });
        });
        aiTestRow->addWidget(m_aiTestBtn);
        aiTestRow->addStretch();
        testCardLayout->addLayout(aiTestRow);

        pageLayout->addStretch(1);
        m_stack->addWidget(pageLayout->parentWidget());
    }

    // ================= 页面 7：关于 =================
    {
        QVBoxLayout *pageLayout = startPage(QString::fromUtf8("关于"));
        pageLayout->addStretch(1);

        auto *iconLabel = new QLabel(this);
        iconLabel->setPixmap(QIcon(QStringLiteral(":/icon.svg")).pixmap(64, 64));
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setToolTip(QString::fromUtf8("Time Master 应用图标"));
        pageLayout->addWidget(iconLabel);
        pageLayout->addSpacing(12);

        auto *appNameLabel = new QLabel(QString::fromUtf8("Time Master"), this);
        appNameLabel->setObjectName(QStringLiteral("aboutName"));
        appNameLabel->setFont(DesignTokens::appFont(26, QFont::Bold));
        appNameLabel->setAlignment(Qt::AlignCenter);
        pageLayout->addWidget(appNameLabel);

        auto *versionLabel = new QLabel(
            QString::fromUtf8("v") + QApplication::applicationVersion(), this);
        versionLabel->setObjectName(QStringLiteral("aboutVersion"));
        versionLabel->setFont(DesignTokens::appFont(14));
        versionLabel->setAlignment(Qt::AlignCenter);
        pageLayout->addWidget(versionLabel);
        pageLayout->addSpacing(20);

        auto *descLabel = new QLabel(
            QString::fromUtf8("Windows 桌面时间追踪工具。记录前台应用时长，"
                              "生成今日报告与上周周报，并可同步云端目标。"), this);
        descLabel->setObjectName(QStringLiteral("aboutDesc"));
        descLabel->setFont(DesignTokens::appFont(12));
        descLabel->setAlignment(Qt::AlignCenter);
        descLabel->setWordWrap(true);
        pageLayout->addWidget(descLabel);
        pageLayout->addSpacing(16);

        auto *dataLabel = new QLabel(
            QString::fromUtf8("数据目录\n%1").arg(m_db->databasePath()), this);
        dataLabel->setObjectName(QStringLiteral("aboutDesc"));
        dataLabel->setFont(DesignTokens::appFont(11));
        dataLabel->setAlignment(Qt::AlignCenter);
        dataLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        dataLabel->setWordWrap(true);
        pageLayout->addWidget(dataLabel);

        pageLayout->addStretch(1);
        m_stack->addWidget(pageLayout->parentWidget());
    }

    contentLayout->addWidget(m_stack, 1);

    // ================= 底部按钮 =================
    auto *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    auto *cancelBtn = new QPushButton(QString::fromUtf8("取消"), this);
    cancelBtn->setObjectName(QStringLiteral("secondaryBtn"));
    cancelBtn->setToolTip(QString::fromUtf8("放弃本次修改并关闭设置窗口"));
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addStretch();
    btnLayout->addWidget(cancelBtn);

    auto *saveBtn = new QPushButton(QString::fromUtf8("保存"), this);
    saveBtn->setObjectName(QStringLiteral("accentBtn"));
    saveBtn->setToolTip(QString::fromUtf8("保存全部设置并立即生效"));
    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        saveSettings();
        emit settingsChanged();
        accept();
    });
    btnLayout->addWidget(saveBtn);
    contentLayout->addLayout(btnLayout);

    rootLayout->addWidget(content, 1);

    // 导航切换：显示对应页面并刷新图标选中色。
    connect(navGroup, &QButtonGroup::idClicked,
            this, &SettingsDialog::showPage);
    m_navButtons.first()->setChecked(true);
    showPage(0);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { applyTheme(); });
    connect(ThemeManager::instance(), &ThemeManager::accentChanged,
            this, [this]() { applyTheme(); });

    // 状态标签每 5 秒重读一次数据库，实时反映推送/拉取/提醒结果。
    m_linewebStatusTimer = new QTimer(this);
    m_linewebStatusTimer->setInterval(5000);
    connect(m_linewebStatusTimer, &QTimer::timeout, this, [this]() {
        updateCloudStatus();
        updateReminderStatus();
    });
    m_linewebStatusTimer->start();

    loadSettings();
}

void SettingsDialog::applyTheme()
{
    QPalette pal = palette();
    pal.setColor(QPalette::Window, DesignTokens::kBg());
    pal.setColor(QPalette::Base, DesignTokens::kSurface());
    pal.setColor(QPalette::Text, DesignTokens::kTextStrong());
    pal.setColor(QPalette::WindowText, DesignTokens::kTextStrong());
    setPalette(pal);

    setStyleSheet(settingsStyle());

    // 刷新侧边栏图标颜色（选中态主题色、未选中态次要文字色）。
    for (int i = 0; i < m_navButtons.size(); ++i) {
        if (i < kNavDefs.size()) {
            m_navButtons[i]->setIcon(
                SettingsIcons::navIcon(kNavDefs[i].kind, m_navButtons[i]->isChecked()));
        }
    }
}

void SettingsDialog::showPage(int index)
{
    if (m_stack)
        m_stack->setCurrentIndex(index);
    for (int i = 0; i < m_navButtons.size(); ++i) {
        const bool selected = (i == index);
        m_navButtons[i]->setChecked(selected);
        if (i < kNavDefs.size()) {
            m_navButtons[i]->setIcon(
                SettingsIcons::navIcon(kNavDefs[i].kind, selected));
        }
    }
}

void SettingsDialog::loadSettings()
{
    m_trackingEnabled->setChecked(
        m_db->getSetting("tracking_enabled", "true") == "true");
    m_pollInterval->setValue(m_db->getSetting("poll_interval", "1").toInt());
    m_idleThreshold->setValue(m_db->getSetting("idle_threshold", "60").toInt());
    m_minTrackingSeconds->setValue(
        m_db->getSetting("min_tracking_seconds", "0").toInt());
    m_minRecordThreshold->setValue(
        m_db->getSetting("min_record_threshold", "40").toInt());
    m_autoStart->setChecked(m_db->getSetting("auto_start", "false") == "true");
    m_darkMode->setChecked(ThemeManager::instance()->isDark());
    m_initialAccent = ThemeManager::instance()->accentColor();
    updateAccentSwatches();
    m_trendFormat->setCurrentIndex(
        m_db->getSetting("trend_display_format", "normal") == "heatmap" ? 1 : 0);

    m_linewebEnabled->setChecked(
        m_db->getSetting("lineweb_enabled", "false") == "true");
    m_linewebEndpoint->setText(m_db->getSetting("lineweb_endpoint", ""));
    m_linewebToken->setText(m_db->getSetting("lineweb_token", ""));
    m_linewebInterval->setValue(
        m_db->getSetting("lineweb_interval", "10").toInt());

    m_aiEnabled->setChecked(m_db->getSetting("ai_enabled", "false") == "true");
    m_aiEndpoint->setText(m_db->getSetting("ai_api_endpoint",
        QStringLiteral("https://api.deepseek.com")));
    m_aiApiKey->setText(m_db->getSetting("ai_api_key", ""));
    m_aiModel->setText(m_db->getSetting("ai_model", "deepseek-chat"));

    m_reminderEnabled->setChecked(
        m_db->getSetting("reminder_enabled", "false") == "true");
    m_reminderTimesList->clear();
    const QStringList times =
        m_db->getSetting("reminder_times", "").split(QLatin1Char(','));
    for (const QString &time : times) {
        const QString t = time.trimmed();
        if (!t.isEmpty())
            m_reminderTimesList->addItem(t);
    }
    m_reminderTimesList->sortItems();

    m_intervalReminderEnabled->setChecked(
        m_db->getSetting("reminder_interval_enabled", "false") == "true");
    m_intervalReminderMinutes->setValue(
        qBound(5, m_db->getSetting("reminder_interval_minutes", "45").toInt(), 240));

    m_weeklyReportEnabled->setChecked(
        m_db->getSetting("weekly_report_enabled", "false") == "true");
    m_weeklyReportDay->setCurrentIndex(
        qBound(1, m_db->getSetting("weekly_report_day", "1").toInt(), 7) - 1);
    const QTime weeklyTime =
        QTime::fromString(m_db->getSetting("weekly_report_time", "09:00"),
                          QStringLiteral("HH:mm"));
    if (weeklyTime.isValid())
        m_weeklyReportTime->setTime(weeklyTime);

    updateCloudStatus();

    m_dailyGoal->setValue(m_db->getSetting("daily_goal", "28800").toInt() / 3600);

    refreshKnownAppsList();
    refreshIgnoredList();
    refreshAliasTable();
}

void SettingsDialog::updateCloudStatus()
{
    const QString lastPush = m_db->getSetting("lineweb_last_push", "");
    const QString lastFetch = m_db->getSetting("lineweb_last_fetch", "");
    if (!lastPush.isEmpty() && !lastFetch.isEmpty())
        m_linewebStatus->setText(lastPush + "  ·  " + lastFetch);
    else if (!lastPush.isEmpty())
        m_linewebStatus->setText(lastPush);
    else if (!lastFetch.isEmpty())
        m_linewebStatus->setText(lastFetch);
    else
        m_linewebStatus->setText(QString::fromUtf8("尚未同步"));
}

void SettingsDialog::updateReminderStatus()
{
    const QString lastFired = m_db->getSetting("reminder_last_fired", "");
    if (!lastFired.isEmpty())
        m_reminderStatus->setText(
            QString::fromUtf8("⏳ 最近触发：") + lastFired);
    else
        m_reminderStatus->setText(
            QString::fromUtf8("⚙ 尚未触发过提醒，请确认已启用并添加时间点"));
}

void SettingsDialog::updateAccentSwatches()
{
    const ThemeManager *tm = ThemeManager::instance();
    const bool custom = tm->hasCustomAccent();
    const QString current = custom ? tm->accentColor().name() : QString();
    const QString defaultHex = ThemeManager::defaultAccent().name();
    for (int i = 0; i < kAccentPresets.size() && i < m_accentSwatches.size(); ++i) {
        const AccentPreset &preset = kAccentPresets[i];
        const QString hex = accentPresetHex(preset);
        const bool checked = *preset.hex
            ? (custom && current.compare(hex, Qt::CaseInsensitive) == 0)
            : (!custom || current.compare(defaultHex, Qt::CaseInsensitive) == 0);
        m_accentSwatches[i]->setChecked(checked);
    }
}

void SettingsDialog::reject()
{
    // 未保存的主题色只是预览态，关闭对话框时还原为打开前的颜色。
    if (ThemeManager::instance()->accentColor() != m_initialAccent)
        ThemeManager::instance()->setAccentColor(m_initialAccent, false);
    QDialog::reject();
}

void SettingsDialog::fetchGoalFromCloud(const QString &endpoint, const QString &token)
{
    QUrl url(normalizeLineWebEndpoint(endpoint) + "/api/health/daily-goal/data");
    QNetworkRequest req(url);
    req.setRawHeader("X-Screen-Time-Token", token.toUtf8());

    auto *nam = new QNetworkAccessManager(this);
    QNetworkReply *reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        reply->deleteLater();
        nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (!obj.contains("dailyGoalSeconds") || !obj["dailyGoalSeconds"].isDouble())
            return;
        const int goal = qBound(0, obj["dailyGoalSeconds"].toInt(), kMaxTotalSeconds);
        if (goal <= 0)
            return; // 云端未设置目标，保留本地默认值。
        m_db->setSetting("daily_goal", QString::number(goal));
        m_dailyGoal->setValue(goal / 3600);
        QMessageBox::information(this,
            QString::fromUtf8("云端目标已同步"),
            QString::fromUtf8("已同步云端目标：%1h")
                .arg(goal / 3600));
    });
}

void SettingsDialog::saveSettings()
{
    m_db->setSetting("tracking_enabled",
                     m_trackingEnabled->isChecked() ? "true" : "false");
    m_db->setSetting("poll_interval", QString::number(m_pollInterval->value()));
    m_db->setSetting("idle_threshold", QString::number(m_idleThreshold->value()));
    m_db->setSetting("min_tracking_seconds",
                     QString::number(m_minTrackingSeconds->value()));
    m_db->setSetting("min_record_threshold",
                     QString::number(m_minRecordThreshold->value()));
    m_db->setSetting("auto_start", m_autoStart->isChecked() ? "true" : "false");
    AutoStartHelper::setAutoStart(m_autoStart->isChecked());

    if (m_darkMode->isChecked() != ThemeManager::instance()->isDark())
        ThemeManager::instance()->setTheme(
            m_darkMode->isChecked() ? ThemeManager::Dark : ThemeManager::Light);

    // 主题色已即时预览，保存时仅在变化后落库。
    if (ThemeManager::instance()->accentColor() != m_initialAccent)
        ThemeManager::instance()->commitAccent();

    m_db->setSetting("trend_display_format",
                     m_trendFormat->currentIndex() == 1 ? "heatmap" : "normal");

    m_db->setSetting("lineweb_enabled",
                     m_linewebEnabled->isChecked() ? "true" : "false");
    m_db->setSetting("lineweb_endpoint", m_linewebEndpoint->text().trimmed());
    m_db->setSetting("lineweb_token", m_linewebToken->text().trimmed());
    m_db->setSetting("lineweb_interval",
                     QString::number(m_linewebInterval->value()));

    m_db->setSetting("ai_enabled", m_aiEnabled->isChecked() ? "true" : "false");
    m_db->setSetting("ai_api_endpoint", m_aiEndpoint->text().trimmed());
    m_db->setSetting("ai_api_key", m_aiApiKey->text().trimmed());
    m_db->setSetting("ai_model", m_aiModel->text().trimmed());

    QStringList times;
    for (int i = 0; i < m_reminderTimesList->count(); ++i)
        times << m_reminderTimesList->item(i)->text();
    m_db->setSetting("reminder_enabled",
                     m_reminderEnabled->isChecked() ? "true" : "false");
    m_db->setSetting("reminder_times", times.join(QLatin1Char(',')));
    m_db->setSetting("reminder_interval_enabled",
                     m_intervalReminderEnabled->isChecked() ? "true" : "false");
    m_db->setSetting("reminder_interval_minutes",
                     QString::number(m_intervalReminderMinutes->value()));

    m_db->setSetting("weekly_report_enabled",
                     m_weeklyReportEnabled->isChecked() ? "true" : "false");
    m_db->setSetting("weekly_report_day",
                     QString::number(m_weeklyReportDay->currentIndex() + 1));
    m_db->setSetting("weekly_report_time",
                     m_weeklyReportTime->time().toString(QStringLiteral("HH:mm")));

    m_db->setSetting("daily_goal", QString::number(m_dailyGoal->value() * 3600));
}

void SettingsDialog::refreshKnownAppsList()
{
    m_knownAppsList->clear();
    m_knownSearch->clear();

    const QStringList processNames = m_db->getAllKnownProcessNames();
    QMap<int, QString> ignored = m_db->getIgnoredApps();
    QSet<QString> ignoredNames;
    for (auto it = ignored.begin(); it != ignored.end(); ++it)
        ignoredNames.insert(it.value());
    const QMap<QString, QString> aliases = m_db->getAppAliases();

    for (const QString &path : processNames) {
        const QString key = ProcessIdentity::normalizeKey(path);
        QString name = aliases.contains(key)
            ? aliases[key] : UiUtils::friendlyAppName(path);
        QIcon icon = AppIconProvider::instance()->icon(path, 20);
        auto *item = new QListWidgetItem(icon, name);
        item->setData(Qt::UserRole, path);
        if (ignoredNames.contains(key))
            item->setForeground(DesignTokens::kTextFaint());
        m_knownAppsList->addItem(item);
    }
}

void SettingsDialog::refreshIgnoredList()
{
    m_ignoredAppsList->clear();
    m_ignoredSearch->clear();

    QMap<int, QString> ignored = m_db->getIgnoredApps();
    const QMap<QString, QString> aliases = m_db->getAppAliases();

    for (auto it = ignored.begin(); it != ignored.end(); ++it) {
        const QString path = it.value();
        const QString key = ProcessIdentity::normalizeKey(path);
        const QString name = aliases.contains(key)
            ? aliases[key] : UiUtils::friendlyAppName(path);
        QIcon icon = AppIconProvider::instance()->icon(path, 20);
        auto *item = new QListWidgetItem(icon, name);
        item->setData(Qt::UserRole, it.key());
        m_ignoredAppsList->addItem(item);
    }
}

void SettingsDialog::filterKnownApps(const QString &text)
{
    for (int i = 0; i < m_knownAppsList->count(); ++i) {
        QListWidgetItem *item = m_knownAppsList->item(i);
        item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
    }
}

void SettingsDialog::filterIgnoredApps(const QString &text)
{
    for (int i = 0; i < m_ignoredAppsList->count(); ++i) {
        QListWidgetItem *item = m_ignoredAppsList->item(i);
        item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
    }
}

void SettingsDialog::refreshAliasTable()
{
    m_aliasTable->setRowCount(0);
    const QMap<QString, QString> aliases = m_db->getAppAliases();
    for (auto it = aliases.begin(); it != aliases.end(); ++it) {
        int row = m_aliasTable->rowCount();
        m_aliasTable->insertRow(row);
        m_aliasTable->setItem(row, 0, new QTableWidgetItem(it.key()));
        m_aliasTable->setItem(row, 1, new QTableWidgetItem(it.value()));
    }
}

void SettingsDialog::onAddIgnored()
{
    QList<QListWidgetItem *> selected = m_knownAppsList->selectedItems();
    for (QListWidgetItem *item : selected) {
        m_db->addIgnoredApp(ProcessIdentity::normalizeKey(
            item->data(Qt::UserRole).toString()));
    }
    refreshKnownAppsList();
    refreshIgnoredList();
}

void SettingsDialog::onRemoveIgnored()
{
    QListWidgetItem *item = m_ignoredAppsList->currentItem();
    if (!item) {
        QMessageBox::warning(this,
            QString::fromUtf8("没有选择"),
            QString::fromUtf8("请先选择要移除的应用"));
        return;
    }
    m_db->removeIgnoredApp(item->data(Qt::UserRole).toInt());
    refreshIgnoredList();
    refreshKnownAppsList();
}

bool SettingsDialog::promptAlias(const QString &title, QString *processName,
                                 QString *displayName, bool processReadOnly)
{
    QDialog dlg(this);
    dlg.setWindowTitle(title);
    dlg.setMinimumWidth(360);
    dlg.setStyleSheet(settingsStyle());
    auto *layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(12);

    auto *processEdit = new QLineEdit(*processName, &dlg);
    processEdit->setPlaceholderText(QStringLiteral("例如 code.exe"));
    processEdit->setReadOnly(processReadOnly);
    auto *displayEdit = new QLineEdit(*displayName, &dlg);
    displayEdit->setPlaceholderText(QStringLiteral("显示名"));

    auto *processLabel = new QLabel(QStringLiteral("进程名"), &dlg);
    auto *displayLabel = new QLabel(QStringLiteral("显示名"), &dlg);
    layout->addWidget(processLabel);
    layout->addWidget(processEdit);
    layout->addWidget(displayLabel);
    layout->addWidget(displayEdit);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    buttons->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("accentBtn"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    buttons->button(QDialogButtonBox::Cancel)->setObjectName(QStringLiteral("secondaryBtn"));
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return false;
    *processName = processEdit->text().trimmed();
    *displayName = displayEdit->text().trimmed();
    return !processName->isEmpty() && !displayName->isEmpty();
}

void SettingsDialog::onAddAlias()
{
    QString processName;
    QString displayName;
    if (!promptAlias(QStringLiteral("添加别名"), &processName, &displayName, false))
        return;
    m_db->setAppAlias(ProcessIdentity::normalizeKey(processName), displayName);
    refreshAliasTable();
}

void SettingsDialog::onEditAlias()
{
    const int row = m_aliasTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this,
            QString::fromUtf8("没有选择"),
            QString::fromUtf8("请先选择要编辑的别名"));
        return;
    }
    QString processName = m_aliasTable->item(row, 0)->text();
    QString displayName = m_aliasTable->item(row, 1)->text();
    if (!promptAlias(QStringLiteral("编辑别名"), &processName, &displayName, true))
        return;
    m_db->setAppAlias(processName, displayName);
    refreshAliasTable();
}

void SettingsDialog::onDeleteAlias()
{
    const int row = m_aliasTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this,
            QString::fromUtf8("没有选择"),
            QString::fromUtf8("请先选择要删除的别名"));
        return;
    }
    m_db->removeAppAliasByProcessName(m_aliasTable->item(row, 0)->text());
    refreshAliasTable();
}
