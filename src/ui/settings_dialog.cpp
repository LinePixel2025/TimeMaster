#include "settings_dialog.h"
#include "database/database_manager.h"
#include "utility/autostart_helper.h"
#include "icon/app_icon_provider.h"
#include "ui/theme_manager.h"
#include "ui/design_tokens.h"
#include "ui/ui_utils.h"
#include "utility/process_identity.h"
#include "push/lineweb_pusher.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QShortcut>
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

namespace {

QString accentButtonStyle()
{
    return QString(
        "QPushButton { background-color: %1; color: white; border: none;"
        " border-radius: 6px; padding: 6px 16px; font-size: 13px; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }")
        .arg(DesignTokens::kAccent().name(),
             DesignTokens::kAccentHover().name(),
             DesignTokens::kAccentPressed().name());
}

QString secondaryButtonStyle()
{
    return QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 6px; padding: 6px 14px; font-size: 13px; }"
        "QPushButton:hover { background: %4; }")
        .arg(DesignTokens::kSurface().name(QColor::HexArgb),
             DesignTokens::kText().name(QColor::HexArgb),
             DesignTokens::kBorder().name(QColor::HexArgb),
             DesignTokens::kSeparator().name(QColor::HexArgb));
}

QString settingsStyle()
{
    return QString(
        "QDialog { background: %1; }"
        "QTabWidget::pane { background: %2; border: 1px solid %3; border-radius: 8px; }"
        "QTabBar { background: transparent; }"
        "QTabBar::tab { color: %4; background: transparent; border: none;"
        " padding: 9px 16px; margin: 0 3px; min-width: 96px; }"
        "QTabBar::tab:hover { color: %5; background: %6; border-radius: 6px; }"
        "QTabBar::tab:selected { color: %7; background: %8; border-radius: 6px; font-weight: 600; }"
        "QLineEdit, QSpinBox { color: %9; background: %2; border: 1px solid %3;"
        " border-radius: 6px; padding: 7px 9px; min-height: 18px; }"
        "QLineEdit:focus, QSpinBox:focus { border-color: %7; }"
        "QListWidget, QTableWidget { color: %9; background: %2; border: 1px solid %3;"
        " border-radius: 6px; alternate-background-color: %10; }"
        "QListWidget::item { padding: 7px 8px; border-radius: 4px; }"
        "QListWidget::item:selected, QTableWidget::item:selected { color: %9; background: %8; }"
        "QHeaderView::section { color: %4; background: %10; border: none;"
        " border-bottom: 1px solid %3; padding: 8px; font-weight: 600; }"
        "QCheckBox { color: %9; spacing: 8px; padding: 5px 0; }"
        "QCheckBox::indicator { width: 17px; height: 17px; }"
        "QGroupBox { color: %9; border: 1px solid %3; border-radius: 8px;"
        " margin-top: 12px; padding: 18px 14px 12px; font-weight: 600; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 5px; background: %1; }")
        .arg(DesignTokens::kBg().name(), DesignTokens::kSurface().name(),
             DesignTokens::kBorder().name(), DesignTokens::kTextMute().name(),
             DesignTokens::kText().name(), DesignTokens::kButtonHoverBg().name(),
             DesignTokens::kAccent().name(), DesignTokens::kAccentLight().name(),
             DesignTokens::kTextStrong().name(), DesignTokens::kSeparator().name());
}

} // namespace

SettingsDialog::SettingsDialog(DatabaseManager *db, QWidget *parent)
    : QDialog(parent), m_db(db)
{
    setWindowTitle(QString::fromUtf8("\xe8\xae\xbe\xe7\xbd\xae"));
    resize(940, 620);
    setMinimumSize(820, 540);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, DesignTokens::kBg());
    pal.setColor(QPalette::Base, DesignTokens::kSurface());
    pal.setColor(QPalette::Text, DesignTokens::kTextStrong());
    pal.setColor(QPalette::WindowText, DesignTokens::kTextStrong());
    setPalette(pal);

    setStyleSheet(settingsStyle());

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 16);
    mainLayout->setSpacing(16);

    m_tabWidget = new QTabWidget(this);
    // North keeps Chinese labels horizontal and leaves the content area wide.
    m_tabWidget->setTabPosition(QTabWidget::North);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setUsesScrollButtons(false);
    mainLayout->addWidget(m_tabWidget);

    // ================= Tab 1: App Management =================
    auto *appTab = new QWidget(this);
    auto *appLayout = new QVBoxLayout(appTab);
    appLayout->setContentsMargins(20, 16, 20, 16);
    appLayout->setSpacing(12);

    auto *splitLayout = new QHBoxLayout();
    splitLayout->setSpacing(8);

    auto buildPanel = [this](const QString &labelText,
                             QLineEdit **searchOut,
                             QListWidget **listOut,
                             bool multiSelect) {
        auto *panel = new QVBoxLayout();
        panel->setSpacing(6);
        auto *label = new QLabel(labelText, this);
        label->setFont(DesignTokens::appFont(12, QFont::Medium));
        panel->addWidget(label);

        auto *search = new QLineEdit(this);
        search->setPlaceholderText(
            QString::fromUtf8("\xe6\x90\x9c\xe7\xb4\xa2..."));
        panel->addWidget(search);

        auto *list = new QListWidget(this);
        list->setSelectionMode(multiSelect
            ? QAbstractItemView::MultiSelection
            : QAbstractItemView::SingleSelection);
        panel->addWidget(list, 1);

        if (searchOut) *searchOut = search;
        if (listOut) *listOut = list;
        return panel;
    };

    splitLayout->addLayout(buildPanel(
        QString::fromUtf8("\xe5\xb7\xb2\xe7\x9f\xa5\xe5\xba\x94\xe7\x94\xa8"),
        &m_knownSearch, &m_knownAppsList, true), 1);
    connect(m_knownSearch, &QLineEdit::textChanged,
            this, &SettingsDialog::filterKnownApps);

    auto *centerPanel = new QVBoxLayout();
    centerPanel->setSpacing(8);
    centerPanel->addStretch();

    auto *addIgnoredBtn = new QPushButton(
        QString::fromUtf8("\xe2\x86\x92 \xe5\x8a\xa0\xe5\x85\xa5\xe5\xb1\x8f\xe8\x94\xbd"), this);
    addIgnoredBtn->setStyleSheet(secondaryButtonStyle());
    connect(addIgnoredBtn, &QPushButton::clicked,
            this, &SettingsDialog::onAddIgnored);
    centerPanel->addWidget(addIgnoredBtn);

    auto *removeIgnoredBtn = new QPushButton(
        QString::fromUtf8("\xe2\x86\x90 \xe7\xa7\xbb\xe9\x99\xa4\xe5\xb1\x8f\xe8\x94\xbd"), this);
    removeIgnoredBtn->setStyleSheet(secondaryButtonStyle());
    connect(removeIgnoredBtn, &QPushButton::clicked,
            this, &SettingsDialog::onRemoveIgnored);
    centerPanel->addWidget(removeIgnoredBtn);

    centerPanel->addStretch();
    splitLayout->addLayout(centerPanel);

    splitLayout->addLayout(buildPanel(
        QString::fromUtf8("\xe5\xb7\xb2\xe5\xb1\x8f\xe8\x94\xbd\xe5\xba\x94\xe7\x94\xa8"),
        &m_ignoredSearch, &m_ignoredAppsList, false), 1);
    connect(m_ignoredSearch, &QLineEdit::textChanged,
            this, &SettingsDialog::filterIgnoredApps);

    auto *delShortcut = new QShortcut(QKeySequence::Delete, m_ignoredAppsList);
    connect(delShortcut, &QShortcut::activated, this, [this]() {
        if (m_ignoredAppsList->currentItem())
            onRemoveIgnored();
    });

    appLayout->addLayout(splitLayout, 1);

    auto *aliasLabel = new QLabel(
        QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe5\x90\x8d\xe7\xa7\xb0\xe5\x88\xab\xe5\x90\x8d"), this);
    aliasLabel->setFont(DesignTokens::appFont(12, QFont::Medium));
    appLayout->addWidget(aliasLabel);

    m_aliasTable = new QTableWidget(0, 2, this);
    m_aliasTable->setHorizontalHeaderLabels({
        QString::fromUtf8("\xe8\xbf\x9b\xe7\xa8\x8b\xe5\x90\x8d"),
        QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba\xe5\x90\x8d")
    });
    m_aliasTable->horizontalHeader()->setStretchLastSection(true);
    m_aliasTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_aliasTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_aliasTable->verticalHeader()->setVisible(false);
    appLayout->addWidget(m_aliasTable);

    auto *aliasBtnRow = new QHBoxLayout();
    auto *addAliasBtn = new QPushButton(
        QString::fromUtf8("\xe6\xb7\xbb\xe5\x8a\xa0\xe5\x88\xab\xe5\x90\x8d"), this);
    addAliasBtn->setStyleSheet(secondaryButtonStyle());
    connect(addAliasBtn, &QPushButton::clicked,
            this, &SettingsDialog::onAddAlias);
    aliasBtnRow->addWidget(addAliasBtn);

    auto *editAliasBtn = new QPushButton(
        QString::fromUtf8("\xe7\xbc\x96\xe8\xbe\x91"), this);
    editAliasBtn->setStyleSheet(secondaryButtonStyle());
    connect(editAliasBtn, &QPushButton::clicked,
            this, &SettingsDialog::onEditAlias);
    aliasBtnRow->addWidget(editAliasBtn);

    auto *deleteAliasBtn = new QPushButton(
        QString::fromUtf8("\xe5\x88\xa0\xe9\x99\xa4"), this);
    deleteAliasBtn->setStyleSheet(secondaryButtonStyle());
    connect(deleteAliasBtn, &QPushButton::clicked,
            this, &SettingsDialog::onDeleteAlias);
    aliasBtnRow->addWidget(deleteAliasBtn);
    aliasBtnRow->addStretch();
    appLayout->addLayout(aliasBtnRow);

    m_tabWidget->addTab(appTab, QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe7\xae\xa1\xe7\x90\x86"));

    // ================= Tab 2: Tracking =================
    auto *trackTab = new QWidget(this);
    auto *trackLayout = new QVBoxLayout(trackTab);
    trackLayout->setContentsMargins(24, 22, 24, 22);
    trackLayout->setSpacing(10);

    m_trackingEnabled = new QCheckBox(
        QString::fromUtf8("\xe5\x90\xaf\xe7\x94\xa8\xe8\xbf\xbd\xe8\xb8\xaa"), this);
    m_trackingEnabled->setFont(DesignTokens::appFont(13));
    trackLayout->addWidget(m_trackingEnabled);

    auto addSpinRow = [this, trackLayout](
            const QString &text, const QString &tooltip,
            int min, int max, int def, int step,
            QSpinBox **out) {
        auto *row = new QHBoxLayout();
        auto *label = new QLabel(text, this);
        label->setFont(DesignTokens::appFont(13));
        row->addWidget(label);
        auto *spin = new QSpinBox(this);
        spin->setRange(min, max);
        spin->setValue(def);
        spin->setSingleStep(step);
        if (!tooltip.isEmpty())
            spin->setToolTip(tooltip);
        row->addWidget(spin);
        row->addStretch();
        trackLayout->addLayout(row);
        if (out) *out = spin;
    };

    addSpinRow(
        QString::fromUtf8("\xe8\xbd\xae\xe8\xaf\xa2\xe9\x97\xb4\xe9\x9a\x94\xef\xbc\x88\xe7\xa7\x92\xef\xbc\x89:"),
        QString(), 1, 10, 1, 1, &m_pollInterval);
    addSpinRow(
        QString::fromUtf8("\xe7\xa9\xba\xe9\x97\xb2\xe5\x88\xa4\xe5\xae\x9a\xe6\x97\xb6\xe9\x97\xb4\xef\xbc\x88\xe7\xa7\x92\xef\xbc\x89:"),
        QString(), 10, 600, 60, 10, &m_idleThreshold);
    addSpinRow(
        QString::fromUtf8("\xe6\x9c\x80\xe4\xbd\x8e\xe8\xae\xa1\xe6\x97\xb6\xe9\x98\x88\xe5\x80\xbc\xef\xbc\x88\xe7\xa7\x92\xef\xbc\x89:"),
        QString::fromUtf8("\xe7\xaa\x97\xe5\x8f\xa3\xe5\x88\x87\xe6\x8d\xa2\xe5\x90\x8e\xe6\xb4\xbb\xe8\xb7\x83\xe8\xb6\x85\xe8\xbf\x87\xe8\xbf\x99\xe4\xb8\xaa\xe6\x97\xb6\xe9\x97\xb4\xe6\x89\x8d\xe5\xbc\x80\xe5\xa7\x8b\xe8\xae\xa1\xe6\x97\xb6\xef\xbc\x8c") + QString::fromUtf8("0\xe4\xb8\xba\xe4\xb8\x8d\xe9\x99\x90\xe5\x88\xb6"),
        0, 30, 0, 1, &m_minTrackingSeconds);
    addSpinRow(
        QString::fromUtf8("\xe6\x9c\x80\xe4\xbd\x8e\xe8\xae\xb0\xe5\xbd\x95\xe9\x98\x88\xe5\x80\xbc\xef\xbc\x88\xe7\xa7\x92\xef\xbc\x89:"),
        QString::fromUtf8("\xe5\x8d\x95\xe6\xac\xa1\xe4\xbd\xbf\xe7\x94\xa8\xe6\x97\xb6\xe9\x95\xbf\xe4\xbd\x8e\xe4\xba\x8e\xe6\xad\xa4\xe5\x80\xbc\xe7\x9a\x84\xe8\xae\xb0\xe5\xbd\x95\xe5\xb0\x86\xe4\xb8\x8d\xe8\xae\xa1\xe5\x85\xa5\xe7\xbb\x9f\xe8\xae\xa1\xe5\x92\x8c\xe5\xaf\xbc\xe5\x87\xba\xef\xbc\x8c") + QString::fromUtf8("0\xe4\xb8\xba\xe4\xb8\x8d\xe9\x99\x90\xe5\x88\xb6"),
        0, 300, 40, 5, &m_minRecordThreshold);
    m_minRecordThreshold->setSuffix(QString::fromUtf8(" \xe7\xa7\x92"));

    trackLayout->addStretch();
    m_tabWidget->addTab(trackTab, QString::fromUtf8("\xe8\xbf\xbd\xe8\xb8\xaa\xe8\xae\xbe\xe7\xbd\xae"));

    // ================= Tab 3: Personalization =================
    auto *personalTab = new QWidget(this);
    auto *personalLayout = new QVBoxLayout(personalTab);
    personalLayout->setContentsMargins(24, 22, 24, 22);
    personalLayout->setSpacing(10);

    m_darkMode = new QCheckBox(
        QString::fromUtf8("\xe6\x9a\x97\xe8\x89\xb2\xe6\xa8\xa1\xe5\xbc\x8f"), this);
    m_darkMode->setFont(DesignTokens::appFont(13));
    personalLayout->addWidget(m_darkMode);

    m_autoStart = new QCheckBox(
        QString::fromUtf8("\xe5\xbc\x80\xe6\x9c\xba\xe8\x87\xaa\xe5\x90\xaf"), this);
    m_autoStart->setFont(DesignTokens::appFont(13));
    personalLayout->addWidget(m_autoStart);

    auto *dailyGoalRow = new QHBoxLayout();
    auto *dailyGoalLabel = new QLabel(
        QString::fromUtf8("\xe6\x9c\xac\xe5\x9c\xb0\xe9\xbb\x98\xe8\xae\xa4\xe7\x9b\xae\xe6\xa0\x87\xef\xbc\x88\xe4\xba\x91\xe7\xab\xaf\xe6\x9c\xaa\xe8\xae\xbe\xe7\xbd\xae\xe6\x97\xb6\xe7\x94\x9f\xe6\x95\x88\xef\xbc\x8c\xe5\xb0\x8f\xe6\x97\xb6\xef\xbc\x89:"), this);
    dailyGoalLabel->setFont(DesignTokens::appFont(13));
    dailyGoalRow->addWidget(dailyGoalLabel);
    m_dailyGoal = new QSpinBox(this);
    m_dailyGoal->setRange(1, 24);
    m_dailyGoal->setValue(8);
    dailyGoalRow->addWidget(m_dailyGoal);
    dailyGoalRow->addStretch();
    personalLayout->addLayout(dailyGoalRow);

    personalLayout->addStretch();
    m_tabWidget->addTab(personalTab, QString::fromUtf8("\xe4\xb8\xaa\xe6\x80\xa7\xe5\x8c\x96"));

    // ================= Tab 4: 提醒 =================
    auto *remindTab = new QWidget(this);
    auto *remindLayout = new QVBoxLayout(remindTab);
    remindLayout->setContentsMargins(24, 22, 24, 22);
    remindLayout->setSpacing(10);

    m_reminderEnabled = new QCheckBox(
        QString::fromUtf8("\xe5\x90\xaf\xe7\x94\xa8\xe5\xae\x9a\xe6\x97\xb6\xe6\x8f\x90\xe9\x86\x92"), this);
    m_reminderEnabled->setFont(DesignTokens::appFont(13));
    m_reminderEnabled->setToolTip(
        QString::fromUtf8("\xe5\x9c\xa8\xe4\xb8\x8b\xe9\x9d\xa2\xe9\x85\x8d\xe7\xbd\xae\xe7\x9a\x84\xe6\x97\xb6\xe9\x97\xb4\xe7\x82\xb9\xe6\x8f\x90\xe9\x86\x92\xe4\xbd\xbf\xe7\x94\xa8\xe6\x83\x85\xe5\x86\xb5"));
    remindLayout->addWidget(m_reminderEnabled);

    auto *addRow = new QHBoxLayout();
    addRow->addWidget(new QLabel(
        QString::fromUtf8("\xe6\x97\xb6\xe9\x97\xb4\xe7\x82\xb9:"), this));
    m_reminderTimeEdit = new QTimeEdit(this);
    m_reminderTimeEdit->setDisplayFormat(QStringLiteral("HH:mm"));
    m_reminderTimeEdit->setTime(QTime::currentTime());
    addRow->addWidget(m_reminderTimeEdit);
    m_reminderAddBtn = new QPushButton(
        QString::fromUtf8("\xe6\xb7\xbb\xe5\x8a\xa0"), this);
    m_reminderAddBtn->setStyleSheet(secondaryButtonStyle());
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
    m_reminderRemoveBtn = new QPushButton(
        QString::fromUtf8("\xe5\x88\xa0\xe9\x99\xa4"), this);
    m_reminderRemoveBtn->setStyleSheet(secondaryButtonStyle());
    connect(m_reminderRemoveBtn, &QPushButton::clicked, this, [this]() {
        delete m_reminderTimesList->takeItem(m_reminderTimesList->currentRow());
    });
    addRow->addWidget(m_reminderRemoveBtn);
    addRow->addStretch();
    remindLayout->addLayout(addRow);

    m_reminderTimesList = new QListWidget(this);
    m_reminderTimesList->setMinimumHeight(120);
    remindLayout->addWidget(m_reminderTimesList);

    // ---- 每周周报分组 ----
    auto *weeklyLine = new QFrame(this);
    weeklyLine->setFrameShape(QFrame::HLine);
    weeklyLine->setStyleSheet(QString("color: %1;").arg(DesignTokens::kBorder().name()));
    remindLayout->addWidget(weeklyLine);

    m_weeklyReportEnabled = new QCheckBox(
        QString::fromUtf8("\xe6\xaf\x8f\xe5\x91\xa8\xe8\x87\xaa\xe5\x8a\xa8\xe7\x94\x9f\xe6\x88\x90\xe5\x91\xa8\xe4\xbd\xbf\xe7\x94\xa8\xe6\x97\xa5\xe6\x8a\xa5"), this);
    m_weeklyReportEnabled->setFont(DesignTokens::appFont(13));
    m_weeklyReportEnabled->setToolTip(
        QString::fromUtf8("\xe6\xaf\x8f\xe5\x91\xa8\xe5\x9c\xa8\xe4\xb8\x8b\xe9\x9d\xa2\xe7\x9a\x84\xe6\x97\xb6\xe5\x88\xbb\xe8\x87\xaa\xe5\x8a\xa8\xe7\x94\x9f\xe6\x88\x90\xe4\xb8\x8a\xe4\xb8\x80\xe5\x91\xa8\xe7\x9a\x84\xe7\x94\xa8\xe4\xbe\x8b\xe6\x97\xa5\xe6\x8a\xa5\xef\xbc\x88HTML\xef\xbc\x8c\xe5\xa1\x98\xe7\x9b\x98\xe9\x80\x9a\xe7\x9f\xa5\xe5\x90\x8e\xe5\x8f\xaf\xe4\xbb\x8e\xe4\xb8\xbb\xe9\xa1\xb5\xe6\x89\x93\xe5\xbc\x80\xe3\x80\x82\xe5\x90\xaf\xe7\x94\xa8 AI \xe6\x97\xb6\xe5\x90\xab AI \xe5\x88\x86\xe6\x9e\x90\xef\xbc\x8c\xe5\x90\xa6\xe5\x88\x99\xe4\xb8\xba\xe6\x9c\xac\xe5\x9c\xb0\xe6\x8a\xa5\xe5\x91\x8a\xe3\x80\x82"));
    remindLayout->addWidget(m_weeklyReportEnabled);

    auto *weeklyDayRow = new QHBoxLayout();
    weeklyDayRow->addWidget(new QLabel(
        QString::fromUtf8("\xe7\x94\x9f\xe6\x88\x90\xe6\x97\xa5:"), this));
    m_weeklyReportDay = new QComboBox(this);
    m_weeklyReportDay->addItems({
        QString::fromUtf8("\xe5\x91\xa8\xe4\xb8\x80"),
        QString::fromUtf8("\xe5\x91\xa8\xe4\xba\x8c"),
        QString::fromUtf8("\xe5\x91\xa8\xe4\xb8\x89"),
        QString::fromUtf8("\xe5\x91\xa8\xe5\x9b\x9b"),
        QString::fromUtf8("\xe5\x91\xa8\xe4\xba\x94"),
        QString::fromUtf8("\xe5\x91\xa8\xe5\x85\xad"),
        QString::fromUtf8("\xe5\x91\xa8\xe6\x97\xa5"),
    });
    weeklyDayRow->addWidget(m_weeklyReportDay);
    weeklyDayRow->addWidget(new QLabel(
        QString::fromUtf8("\xe6\x97\xb6\xe5\x88\xbb:"), this));
    m_weeklyReportTime = new QTimeEdit(this);
    m_weeklyReportTime->setDisplayFormat(QStringLiteral("HH:mm"));
    m_weeklyReportTime->setTime(QTime(9, 0));
    weeklyDayRow->addWidget(m_weeklyReportTime);
    weeklyDayRow->addStretch();
    remindLayout->addLayout(weeklyDayRow);

    m_reminderStatus = new QLabel(this);
    m_reminderStatus->setStyleSheet(
        QString("color: %1; font-size: 12px; background: transparent;")
            .arg(DesignTokens::kTextMute().name()));
    remindLayout->addWidget(m_reminderStatus);

    remindLayout->addStretch();
    m_tabWidget->addTab(remindTab, QString::fromUtf8("\xe6\x8f\x90\xe9\x86\x92"));

    // ================= Tab 5: Cloud Sync =================
    auto *cloudTab = new QWidget(this);
    auto *cloudLayout = new QVBoxLayout(cloudTab);
    cloudLayout->setContentsMargins(24, 22, 24, 22);
    cloudLayout->setSpacing(10);

    m_linewebEnabled = new QCheckBox(
        QString::fromUtf8("\xe5\x90\xaf\xe7\x94\xa8\xe6\x8e\xa8\xe9\x80\x81"), this);
    m_linewebEnabled->setFont(DesignTokens::appFont(13));
    cloudLayout->addWidget(m_linewebEnabled);

    auto *endpointRow = new QHBoxLayout();
    endpointRow->addWidget(new QLabel(
        QString::fromUtf8("API \xe5\x9c\xb0\xe5\x9d\x80:"), this));
    m_linewebEndpoint = new QLineEdit(this);
    m_linewebEndpoint->setPlaceholderText(
        QString::fromUtf8("https://your-server.com"));
    endpointRow->addWidget(m_linewebEndpoint, 1);
    cloudLayout->addLayout(endpointRow);

    auto *tokenRow = new QHBoxLayout();
    tokenRow->addWidget(new QLabel(QString::fromUtf8("Token:"), this));
    m_linewebToken = new QLineEdit(this);
    m_linewebToken->setEchoMode(QLineEdit::Password);
    m_linewebToken->setPlaceholderText("st_...");
    tokenRow->addWidget(m_linewebToken, 1);
    m_linewebTokenToggle = new QPushButton(
        QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba"), this);
    m_linewebTokenToggle->setStyleSheet(secondaryButtonStyle());
    connect(m_linewebTokenToggle, &QPushButton::clicked, this, [this]() {
        const bool show = (m_linewebToken->echoMode() == QLineEdit::Password);
        m_linewebToken->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
        m_linewebTokenToggle->setText(
            show ? QString::fromUtf8("\xe9\x9a\x90\xe8\x97\x8f")
                 : QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba"));
    });
    tokenRow->addWidget(m_linewebTokenToggle);
    cloudLayout->addLayout(tokenRow);

    auto *intervalRow = new QHBoxLayout();
    intervalRow->addWidget(new QLabel(
        QString::fromUtf8("\xe6\x8e\xa8\xe9\x80\x81\xe9\x97\xb4\xe9\x9a\x94\xef\xbc\x88\xe5\x88\x86\xe9\x92\x9f\xef\xbc\x89:"), this));
    m_linewebInterval = new QSpinBox(this);
    m_linewebInterval->setRange(5, 30);
    m_linewebInterval->setValue(10);
    intervalRow->addWidget(m_linewebInterval);
    intervalRow->addStretch();
    cloudLayout->addLayout(intervalRow);

    auto *testRow = new QHBoxLayout();
    m_linewebTestBtn = new QPushButton(
        QString::fromUtf8("\xe8\xbf\x9e\xe6\x8e\xa5\xe6\xb5\x8b\xe8\xaf\x95"), this);
    m_linewebTestBtn->setStyleSheet(secondaryButtonStyle());
    connect(m_linewebTestBtn, &QPushButton::clicked, this, [this]() {
        const QString token = m_linewebToken->text().trimmed();
        const QString endpoint = m_linewebEndpoint->text().trimmed();
        if (token.isEmpty() || endpoint.isEmpty()) {
            QMessageBox::warning(this,
                QString::fromUtf8("\xe9\x85\x8d\xe7\xbd\xae\xe4\xb8\x8d\xe5\xae\x8c\xe6\x95\xb4"),
                QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe5\xa1\xab\xe5\x86\x99 API \xe5\x9c\xb0\xe5\x9d\x80\xe5\x92\x8c Token"));
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
                    QString::fromUtf8("\xe6\xb5\x8b\xe8\xaf\x95\xe6\x88\x90\xe5\x8a\x9f"),
                    QString::fromUtf8("\xe8\xbf\x9e\xe6\x8e\xa5\xe6\xb5\x8b\xe8\xaf\x95\xe6\x88\x90\xe5\x8a\x9f\xef\xbc\x81"));
                // 推送验证成功后顺带拉取云端目标写回 daily_goal（云端优先、本地兜底）。
                fetchGoalFromCloud(endpoint, token);
            } else {
                QMessageBox::warning(this,
                    QString::fromUtf8("\xe6\xb5\x8b\xe8\xaf\x95\xe5\xa4\xb1\xe8\xb4\xa5"),
                    QString::fromUtf8("\xe8\xbf\x9e\xe6\x8e\xa5\xe5\xa4\xb1\xe8\xb4\xa5\xef\xbc\x9a")
                        + reply->errorString());
            }
        });
    });
    testRow->addWidget(m_linewebTestBtn);

    m_linewebStatus = new QLabel(this);
    m_linewebStatus->setStyleSheet(
        QString("color: %1; font-size: 12px; background: transparent;")
            .arg(DesignTokens::kTextMute().name()));
    testRow->addWidget(m_linewebStatus);
    testRow->addStretch();
    cloudLayout->addLayout(testRow);
    cloudLayout->addStretch();
    m_tabWidget->addTab(cloudTab, QString::fromUtf8("\xe4\xba\x91\xe7\xab\xaf\xe5\x90\x8c\xe6\xad\xa5"));

    // ================= Tab 6: AI 智能 =================
    auto *aiTab = new QWidget(this);
    auto *aiLayout = new QVBoxLayout(aiTab);
    aiLayout->setContentsMargins(24, 22, 24, 22);
    aiLayout->setSpacing(10);

    m_aiEnabled = new QCheckBox(
        QString::fromUtf8("\xe5\x90\xaf\xe7\x94\xa8 AI \xe6\x8a\xa5\xe5\x91\x8a"), this);
    m_aiEnabled->setFont(DesignTokens::appFont(13));
    aiLayout->addWidget(m_aiEnabled);

    auto *aiEndpointRow = new QHBoxLayout();
    aiEndpointRow->addWidget(new QLabel(
        QString::fromUtf8("API \xe5\x9c\xb0\xe5\x9d\x80:"), this));
    m_aiEndpoint = new QLineEdit(this);
    m_aiEndpoint->setPlaceholderText(
        QString::fromUtf8("https://api.deepseek.com"));
    aiEndpointRow->addWidget(m_aiEndpoint, 1);
    aiLayout->addLayout(aiEndpointRow);

    auto *aiKeyRow = new QHBoxLayout();
    aiKeyRow->addWidget(new QLabel(QString::fromUtf8("API Key:"), this));
    m_aiApiKey = new QLineEdit(this);
    m_aiApiKey->setEchoMode(QLineEdit::Password);
    m_aiApiKey->setPlaceholderText(QString::fromUtf8("\xe5\x8f\xaf\xe9\x80\x9a\xe8\xbf\x87\xe7\xae\xa1\xe7\x90\x86\xe5\xb9\xb3\xe5\x8f\xb0\xe8\x8e\xb7\xe5\x8f\x96\xef\xbc\x8c\xe4\xbb\xa5 Bearer \xe6\x96\xb9\xe5\xbc\x8f\xe9\xaa\x8c\xe8\xaf\x81"));
    aiKeyRow->addWidget(m_aiApiKey, 1);
    m_aiApiKeyToggle = new QPushButton(
        QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba"), this);
    m_aiApiKeyToggle->setStyleSheet(secondaryButtonStyle());
    connect(m_aiApiKeyToggle, &QPushButton::clicked, this, [this]() {
        const bool show = (m_aiApiKey->echoMode() == QLineEdit::Password);
        m_aiApiKey->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
        m_aiApiKeyToggle->setText(
            show ? QString::fromUtf8("\xe9\x9a\x90\xe8\x97\x8f")
                 : QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba"));
    });
    aiKeyRow->addWidget(m_aiApiKeyToggle);
    aiLayout->addLayout(aiKeyRow);

    auto *aiModelRow = new QHBoxLayout();
    aiModelRow->addWidget(new QLabel(
        QString::fromUtf8("\xe6\xa8\xa1\xe5\x9e\x8b\xe5\x90\x8d:"), this));
    m_aiModel = new QLineEdit(this);
    m_aiModel->setPlaceholderText("deepseek-chat");
    aiModelRow->addWidget(m_aiModel, 1);
    aiLayout->addLayout(aiModelRow);

    auto *aiTestRow = new QHBoxLayout();
    m_aiTestBtn = new QPushButton(
        QString::fromUtf8("\xe8\xbf\x9e\xe6\x8e\xa5\xe6\xb5\x8b\xe8\xaf\x95"), this);
    m_aiTestBtn->setStyleSheet(secondaryButtonStyle());
    connect(m_aiTestBtn, &QPushButton::clicked, this, [this]() {
        const QString key = m_aiApiKey->text().trimmed();
        const QString endpoint = m_aiEndpoint->text().trimmed();
        if (key.isEmpty() || endpoint.isEmpty()) {
            QMessageBox::warning(this,
                QString::fromUtf8("\xe9\x85\x8d\xe7\xbd\xae\xe4\xb8\x8d\xe5\xae\x8c\xe6\x95\xb4"),
                QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe5\xa1\xab\xe5\x86\x99 API \xe5\x9c\xb0\xe5\x9d\x80\xe5\x92\x8c API Key"));
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
                    QString::fromUtf8("\xe6\xb5\x8b\xe8\xaf\x95\xe6\x88\x90\xe5\x8a\x9f"),
                    obj.contains(QStringLiteral("data"))
                        ? QString::fromUtf8("\xe8\xbf\x9e\xe6\x8e\xa5\xe6\x88\x90\xe5\x8a\x9f\xef\xbc\x81"
                                             "\xe5\xbd\x93\xe5\x89\x8d\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe5\x8f\xaf\xe7\x94\xa8\xe6\xa8\xa1\xe5\x9e\x8b\xe6\x95\xb0\xef\xbc\x9a%1")
                                                 .arg(obj[QStringLiteral("data")].toArray().size())
                        : QString::fromUtf8("\xe8\xbf\x9e\xe6\x8e\xa5\xe6\x88\x90\xe5\x8a\x9f\xef\xbc\x81"));
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
                    QString::fromUtf8("\xe6\xb5\x8b\xe8\xaf\x95\xe5\xa4\xb1\xe8\xb4\xa5"),
                    QString::fromUtf8("\xe8\xbf\x9e\xe6\x8e\xa5\xe5\xa4\xb1\xe8\xb4\xa5\xef\xbc\x9a") + err);
            }
        });
    });
    aiTestRow->addWidget(m_aiTestBtn);
    aiTestRow->addStretch();
    aiLayout->addLayout(aiTestRow);

    aiLayout->addStretch();
    m_tabWidget->addTab(aiTab, QString::fromUtf8("AI \xe6\x99\xba\xe8\x83\xbd"));

    // ================= Tab 6: About =================
    auto *aboutTab = new QWidget(this);
    auto *aboutLayout = new QVBoxLayout(aboutTab);
    aboutLayout->setContentsMargins(24, 22, 24, 22);
    aboutLayout->setAlignment(Qt::AlignCenter);

    auto *appNameLabel = new QLabel(QString::fromUtf8("Time Master"), aboutTab);
    appNameLabel->setFont(DesignTokens::appFont(26, QFont::Bold));
    appNameLabel->setAlignment(Qt::AlignCenter);
    appNameLabel->setStyleSheet(
        QString("color: %1; background: transparent;").arg(DesignTokens::kTextStrong().name()));
    aboutLayout->addWidget(appNameLabel);

    auto *versionLabel = new QLabel(
        QString::fromUtf8("v") + QApplication::applicationVersion(), aboutTab);
    versionLabel->setFont(DesignTokens::appFont(14));
    versionLabel->setAlignment(Qt::AlignCenter);
    versionLabel->setStyleSheet(
        QString("color: %1; background: transparent;").arg(DesignTokens::kTextMute().name()));
    aboutLayout->addWidget(versionLabel);
    aboutLayout->addSpacing(24);

    auto *descLabel = new QLabel(
        QString::fromUtf8("Windows \xe6\xa1\x8c\xe9\x9d\xa2\xe6\x97\xb6\xe9\x97\xb4\xe8\xbf\xbd\xe8\xb8\xaa\xe5\xb7\xa5\xe5\x85\xb7"),
        aboutTab);
    descLabel->setFont(DesignTokens::appFont(12));
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setStyleSheet(
        QString("color: %1; background: transparent;").arg(DesignTokens::kTextFaint().name()));
    aboutLayout->addWidget(descLabel);
    aboutLayout->addStretch();
    m_tabWidget->addTab(aboutTab, QString::fromUtf8("\xe5\x85\xb3\xe4\xba\x8e"));

    // ================= Bottom buttons =================
    auto *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);
    btnLayout->addStretch();

    auto *cancelBtn = new QPushButton(
        QString::fromUtf8("\xe5\x8f\x96\xe6\xb6\x88"), this);
    cancelBtn->setStyleSheet(secondaryButtonStyle());
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    auto *saveBtn = new QPushButton(
        QString::fromUtf8("\xe4\xbf\x9d\xe5\xad\x98"), this);
    saveBtn->setStyleSheet(accentButtonStyle());
    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        saveSettings();
        emit settingsChanged();
        accept();
    });
    btnLayout->addWidget(saveBtn);
    mainLayout->addLayout(btnLayout);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, DesignTokens::kBg());
        pal.setColor(QPalette::Base, DesignTokens::kSurface());
        pal.setColor(QPalette::Text, DesignTokens::kTextStrong());
        pal.setColor(QPalette::WindowText, DesignTokens::kTextStrong());
        setPalette(pal);
        m_linewebStatus->setStyleSheet(
            QString("color: %1; font-size: 12px; background: transparent;")
                .arg(DesignTokens::kTextMute().name()));
        m_reminderStatus->setStyleSheet(
            QString("color: %1; font-size: 12px; background: transparent;")
                .arg(DesignTokens::kTextMute().name()));
    });

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
        m_linewebStatus->setText(QString::fromUtf8("\xe5\xb0\x9a\xe6\x9c\xaa\xe5\x90\x8c\xe6\xad\xa5"));
}

void SettingsDialog::updateReminderStatus()
{
    const QString lastFired = m_db->getSetting("reminder_last_fired", "");
    if (lastFired.isEmpty())
        m_reminderStatus->setText(
            QString::fromUtf8("\xe2\x9a\x99 \xe5\xb0\x9a\xe6\x9c\xaa\xe8\xa7\xa6\xe5\x8f\x91\xe8\xbf\x87\xe6\x8f\x90\xe9\x86\x92\xef\xbc\x8c"
                              "\xe8\xaf\xb7\xe7\xa1\xae\xe8\xae\xa4\xe5\xb7\xb2\xe5\x90\xaf\xe7\x94\xa8\xe5\xb9\xb6\xe6\xb7\xbb\xe5\x8a\xa0\xe6\x97\xb6\xe9\x97\xb4\xe7\x82\xb9"));
    else
        m_reminderStatus->setText(
            QString::fromUtf8("\xe2\x8f\xb3 \xe6\x9c\x80\xe8\xbf\x91\xe8\xa7\xa6\xe5\x8f\x91\xef\xbc\x9a") + lastFired);
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
            QString::fromUtf8("\xe4\xba\x91\xe7\xab\xaf\xe7\x9b\xae\xe6\xa0\x87\xe5\xb7\xb2\xe5\x90\x8c\xe6\xad\xa5"),
            QString::fromUtf8("\xe5\xb7\xb2\xe5\x90\x8c\xe6\xad\xa5\xe4\xba\x91\xe7\xab\xaf\xe7\x9b\xae\xe6\xa0\x87\xef\xbc\x9a%1h")
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
            QString::fromUtf8("\xe6\xb2\xa1\xe6\x9c\x89\xe9\x80\x89\xe6\x8b\xa9"),
            QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe9\x80\x89\xe6\x8b\xa9\xe8\xa6\x81\xe7\xa7\xbb\xe9\x99\xa4\xe7\x9a\x84\xe5\xba\x94\xe7\x94\xa8"));
        return;
    }
    m_db->removeIgnoredApp(item->data(Qt::UserRole).toInt());
    refreshIgnoredList();
    refreshKnownAppsList();
}

void SettingsDialog::onAddAlias()
{
    bool ok = false;
    QString processName = QInputDialog::getText(this,
        QString::fromUtf8("\xe6\xb7\xbb\xe5\x8a\xa0\xe5\x88\xab\xe5\x90\x8d"),
        QString::fromUtf8("\xe8\xbf\x9b\xe7\xa8\x8b\xe5\x90\x8d (e.g. code.exe):"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok || processName.isEmpty()) return;

    QString displayName = QInputDialog::getText(this,
        QString::fromUtf8("\xe6\xb7\xbb\xe5\x8a\xa0\xe5\x88\xab\xe5\x90\x8d"),
        QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba\xe5\x90\x8d:"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok || displayName.isEmpty()) return;

    m_db->setAppAlias(ProcessIdentity::normalizeKey(processName), displayName);
    refreshAliasTable();
}

void SettingsDialog::onEditAlias()
{
    const int row = m_aliasTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this,
            QString::fromUtf8("\xe6\xb2\xa1\xe6\x9c\x89\xe9\x80\x89\xe6\x8b\xa9"),
            QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe9\x80\x89\xe6\x8b\xa9\xe8\xa6\x81\xe7\xbc\x96\xe8\xbe\x91\xe7\x9a\x84\xe5\x88\xab\xe5\x90\x8d"));
        return;
    }
    const QString processName = m_aliasTable->item(row, 0)->text();
    const QString currentDisplay = m_aliasTable->item(row, 1)->text();

    bool ok = false;
    QString newDisplay = QInputDialog::getText(this,
        QString::fromUtf8("\xe7\xbc\x96\xe8\xbe\x91\xe5\x88\xab\xe5\x90\x8d"),
        QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba\xe5\x90\x8d:"),
        QLineEdit::Normal, currentDisplay, &ok);
    if (!ok || newDisplay.isEmpty()) return;

    m_db->setAppAlias(processName, newDisplay);
    refreshAliasTable();
}

void SettingsDialog::onDeleteAlias()
{
    const int row = m_aliasTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this,
            QString::fromUtf8("\xe6\xb2\xa1\xe6\x9c\x89\xe9\x80\x89\xe6\x8b\xa9"),
            QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe9\x80\x89\xe6\x8b\xa9\xe8\xa6\x81\xe5\x88\xa0\xe9\x99\xa4\xe7\x9a\x84\xe5\x88\xab\xe5\x90\x8d"));
        return;
    }
    m_db->removeAppAliasByProcessName(m_aliasTable->item(row, 0)->text());
    refreshAliasTable();
}
