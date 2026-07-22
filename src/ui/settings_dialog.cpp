#include "settings_dialog.h"
#include "database/database_manager.h"
#include "utility/autostart_helper.h"
#include "icon/app_icon_provider.h"
#include "ui/theme_manager.h"
#include "ui/design_tokens.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QColor>
#include <QList>
#include <QShortcut>
#include <QLineEdit>
#include <QDate>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QApplication>

static QString normalizeEndpoint(const QString &endpoint)
{
    QString result = endpoint.trimmed();
    while (result.endsWith('/'))
        result.chop(1);
    if (result.endsWith("/api/health/push"))
        result = result.left(result.length() - 16);
    return result;
}

static QFont appFont(int size, QFont::Weight weight = QFont::Normal)
{
    QFont font("Microsoft YaHei", size, weight);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

static QString friendlyName(const QString &processPath,
                            const QMap<QString, QString> &aliases)
{
    if (aliases.contains(processPath))
        return aliases[processPath];
    int pos = processPath.lastIndexOf('\\');
    QString name = (pos >= 0) ? processPath.mid(pos + 1) : processPath;
    if (name.endsWith(".exe", Qt::CaseInsensitive))
        name.chop(4);
    if (!name.isEmpty())
        name[0] = name[0].toUpper();
    return name;
}

SettingsDialog::SettingsDialog(DatabaseManager *db, QWidget *parent)
    : QDialog(parent), m_db(db)
{
    setWindowTitle(QString::fromUtf8("\xe8\xae\xbe\xe7\xbd\xae"));
    resize(900, 600);
    setMinimumSize(780, 520);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    m_tabWidget = new QTabWidget(this);
    mainLayout->addWidget(m_tabWidget);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, DesignTokens::kBg());
    pal.setColor(QPalette::Base, DesignTokens::kSurface());
    pal.setColor(QPalette::Text, DesignTokens::kTextStrong());
    pal.setColor(QPalette::WindowText, DesignTokens::kTextStrong());
    setPalette(pal);

    setStyleSheet(
        QString("SettingsDialog { background-color: %1; }"
                "QTabWidget::pane { background: %1; border: none; }"
                "QTabBar::tab { padding: 8px 20px; font-size: 13px; }")
            .arg(DesignTokens::kBg().name()));

    // ---- Tab 1: App Management ----
    QWidget *appTab = new QWidget();
    QVBoxLayout *appLayout = new QVBoxLayout(appTab);
    appLayout->setContentsMargins(8, 8, 8, 8);
    appLayout->setSpacing(8);

    // ---- Split: left | center | right ----
    QHBoxLayout *splitLayout = new QHBoxLayout();
    splitLayout->setSpacing(8);

    // Left panel: known apps
    QVBoxLayout *leftPanel = new QVBoxLayout();
    leftPanel->setSpacing(6);

    QLabel *knownLabel = new QLabel(QString::fromUtf8("\xe5\xb7\xb2\xe7\x9f\xa5\xe5\xba\x94\xe7\x94\xa8"), this);
    knownLabel->setFont(appFont(12, QFont::Medium));
    leftPanel->addWidget(knownLabel);

    m_knownSearch = new QLineEdit(this);
    m_knownSearch->setPlaceholderText(QString::fromUtf8("\xe6\x90\x9c\xe7\xb4\xa2\xe5\xb7\xb2\xe7\x9f\xa5\xe5\xba\x94\xe7\x94\xa8..."));
    connect(m_knownSearch, &QLineEdit::textChanged, this, &SettingsDialog::filterKnownApps);
    leftPanel->addWidget(m_knownSearch);

    m_knownAppsList = new QListWidget();
    m_knownAppsList->setSelectionMode(QAbstractItemView::MultiSelection);
    leftPanel->addWidget(m_knownAppsList, 1);

    splitLayout->addLayout(leftPanel, 1);

    // Center buttons
    QVBoxLayout *centerPanel = new QVBoxLayout();
    centerPanel->setSpacing(8);
    centerPanel->addStretch();

    QPushButton *addIgnoredBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x92 \xe5\x8a\xa0\xe5\x85\xa5\xe5\xb1\x8f\xe8\x94\xbd"), this);
    connect(addIgnoredBtn, &QPushButton::clicked, this, &SettingsDialog::onAddIgnored);
    centerPanel->addWidget(addIgnoredBtn);

    QPushButton *removeIgnoredBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90 \xe7\xa7\xbb\xe9\x99\xa4\xe5\xb1\x8f\xe8\x94\xbd"), this);
    connect(removeIgnoredBtn, &QPushButton::clicked, this, &SettingsDialog::onRemoveIgnored);
    centerPanel->addWidget(removeIgnoredBtn);

    centerPanel->addStretch();
    splitLayout->addLayout(centerPanel);

    // Right panel: blocked apps
    QVBoxLayout *rightPanel = new QVBoxLayout();
    rightPanel->setSpacing(6);

    QLabel *ignoredLabel = new QLabel(QString::fromUtf8("\xe5\xb7\xb2\xe5\xb1\x8f\xe8\x94\xbd\xe5\xba\x94\xe7\x94\xa8"), this);
    ignoredLabel->setFont(appFont(12, QFont::Medium));
    rightPanel->addWidget(ignoredLabel);

    m_ignoredSearch = new QLineEdit(this);
    m_ignoredSearch->setPlaceholderText(QString::fromUtf8("\xe6\x90\x9c\xe7\xb4\xa2\xe5\xb7\xb2\xe5\xb1\x8f\xe8\x94\xbd\xe5\xba\x94\xe7\x94\xa8..."));
    connect(m_ignoredSearch, &QLineEdit::textChanged, this, &SettingsDialog::filterIgnoredApps);
    rightPanel->addWidget(m_ignoredSearch);

    m_ignoredAppsList = new QListWidget();
    rightPanel->addWidget(m_ignoredAppsList, 1);

    // Delete key shortcut for blocked apps list
    QShortcut *delShortcut = new QShortcut(QKeySequence::Delete, m_ignoredAppsList);
    connect(delShortcut, &QShortcut::activated, this, [this]() {
        if (m_ignoredAppsList->currentItem())
            onRemoveIgnored();
    });

    splitLayout->addLayout(rightPanel, 1);

    appLayout->addLayout(splitLayout, 1);

    QLabel *aliasLabel = new QLabel(QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe5\x90\x8d\xe7\xa7\xb0\xe5\x88\xab\xe5\x90\x8d"), this);
    aliasLabel->setFont(appFont(12, QFont::Medium));
    appLayout->addWidget(aliasLabel);

    m_aliasTable = new QTableWidget(0, 3, this);
    m_aliasTable->setHorizontalHeaderLabels({
        QString::fromUtf8("\xe8\xbf\x9b\xe7\xa8\x8b\xe5\x90\x8d"),
        QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba\xe5\x90\x8d"),
        QString::fromUtf8("\xe6\x93\x8d\xe4\xbd\x9c")
    });
    m_aliasTable->horizontalHeader()->setStretchLastSection(true);
    m_aliasTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_aliasTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    appLayout->addWidget(m_aliasTable);

    QHBoxLayout *aliasBtnRow = new QHBoxLayout();
    QPushButton *addAliasBtn = new QPushButton(QString::fromUtf8("\xe6\xb7\xbb\xe5\x8a\xa0\xe5\x88\xab\xe5\x90\x8d"), this);
    connect(addAliasBtn, &QPushButton::clicked, this, &SettingsDialog::onAddAlias);
    aliasBtnRow->addWidget(addAliasBtn);

    QPushButton *editAliasBtn = new QPushButton(QString::fromUtf8("\xe7\xbc\x96\xe8\xbe\x91"), this);
    connect(editAliasBtn, &QPushButton::clicked, this, &SettingsDialog::onEditAlias);
    aliasBtnRow->addWidget(editAliasBtn);

    QPushButton *deleteAliasBtn = new QPushButton(QString::fromUtf8("\xe5\x88\xa0\xe9\x99\xa4"), this);
    connect(deleteAliasBtn, &QPushButton::clicked, this, &SettingsDialog::onDeleteAlias);
    aliasBtnRow->addWidget(deleteAliasBtn);
    aliasBtnRow->addStretch();
    appLayout->addLayout(aliasBtnRow);

    m_tabWidget->addTab(appTab, QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe7\xae\xa1\xe7\x90\x86"));

    // ---- Tab 2: Tracking Settings ----
    QWidget *trackTab = new QWidget();
    QVBoxLayout *trackLayout = new QVBoxLayout(trackTab);
    trackLayout->setContentsMargins(8, 8, 8, 8);
    trackLayout->setSpacing(16);

    m_trackingEnabled = new QCheckBox(QString::fromUtf8("\xe5\x90\xaf\xe7\x94\xa8\xe8\xbf\xbd\xe8\xb8\xaa"), this);
    m_trackingEnabled->setFont(appFont(13));
    trackLayout->addWidget(m_trackingEnabled);

    QHBoxLayout *pollRow = new QHBoxLayout();
    pollRow->addWidget(new QLabel(QString::fromUtf8("\xe8\xbd\xae\xe8\xaf\xa2\xe9\x97\xb4\xe9\x9a\x94\xef\xbc\x88\xe7\xa7\x92\xef\xbc\x89:"), this));
    m_pollInterval = new QSpinBox(this);
    m_pollInterval->setRange(1, 10);
    m_pollInterval->setValue(1);
    pollRow->addWidget(m_pollInterval);
    pollRow->addStretch();
    trackLayout->addLayout(pollRow);

    QHBoxLayout *idleRow = new QHBoxLayout();
    idleRow->addWidget(new QLabel(QString::fromUtf8("\xe7\xa9\xba\xe9\x97\xb2\xe5\x88\xa4\xe5\xae\x9a\xe6\x97\xb6\xe9\x97\xb4\xef\xbc\x88\xe7\xa7\x92\xef\xbc\x89:"), this));
    m_idleThreshold = new QSpinBox(this);
    m_idleThreshold->setRange(10, 600);
    m_idleThreshold->setValue(60);
    idleRow->addWidget(m_idleThreshold);
    idleRow->addStretch();
    trackLayout->addLayout(idleRow);

    QHBoxLayout *minTrackRow = new QHBoxLayout();
    minTrackRow->addWidget(new QLabel(QString::fromUtf8("\xe6\x9c\x80\xe4\xbd\x8e\xe8\xae\xa1\xe6\x97\xb6\xe9\x98\x88\xe5\x80\xbc\xef\xbc\x88\xe7\xa7\x92\xef\xbc\x89:"), this));
    m_minTrackingSeconds = new QSpinBox(this);
    m_minTrackingSeconds->setRange(0, 30);
    m_minTrackingSeconds->setValue(0);
    m_minTrackingSeconds->setToolTip(QString::fromUtf8("\xe7\xaa\x97\xe5\x8f\xa3\xe5\x88\x87\xe6\x8d\xa2\xe5\x90\x8e\xe6\xb4\xbb\xe8\xb7\x83\xe8\xb6\x85\xe8\xbf\x87\xe8\xbf\x99\xe4\xb8\xaa\xe6\x97\xb6\xe9\x97\xb4\xe6\x89\x8d\xe5\xbc\x80\xe5\xa7\x8b\xe8\xae\xa1\xe6\x97\xb6\xef\xbc\x8c" "0\xe4\xb8\xba\xe4\xb8\x8d\xe9\x99\x90\xe5\x88\xb6"));
    minTrackRow->addWidget(m_minTrackingSeconds);
    minTrackRow->addStretch();
    trackLayout->addLayout(minTrackRow);

    QHBoxLayout *minRecordRow = new QHBoxLayout();
    minRecordRow->addWidget(new QLabel(QString::fromUtf8("\xe6\x9c\x80\xe4\xbd\x8e\xe8\xae\xb0\xe5\xbd\x95\xe9\x98\x88\xe5\x80\xbc\xef\xbc\x88\xe7\xa7\x92\xef\xbc\x89:"), this));
    m_minRecordThreshold = new QSpinBox(this);
    m_minRecordThreshold->setRange(0, 300);
    m_minRecordThreshold->setValue(40);
    m_minRecordThreshold->setSingleStep(5);
    m_minRecordThreshold->setSuffix(QString::fromUtf8(" \xe7\xa7\x92"));
    m_minRecordThreshold->setToolTip(QString::fromUtf8("\xe5\x8d\x95\xe6\xac\xa1\xe4\xbd\xbf\xe7\x94\xa8\xe6\x97\xb6\xe9\x95\xbf\xe4\xbd\x8e\xe4\xba\x8e\xe6\xad\xa4\xe5\x80\xbc\xe7\x9a\x84\xe8\xae\xb0\xe5\xbd\x95\xe5\xb0\x86\xe4\xb8\x8d\xe8\xae\xa1\xe5\x85\xa5\xe7\xbb\x9f\xe8\xae\xa1\xe5\x92\x8c\xe5\xaf\xbc\xe5\x87\xba\xef\xbc\x8c" "0\xe4\xb8\xba\xe4\xb8\x8d\xe9\x99\x90\xe5\x88\xb6"));
    minRecordRow->addWidget(m_minRecordThreshold);
    minRecordRow->addStretch();
    trackLayout->addLayout(minRecordRow);

    trackLayout->addStretch();
    m_tabWidget->addTab(trackTab, QString::fromUtf8("\xe8\xbf\xbd\xe8\xb8\xaa\xe8\xae\xbe\xe7\xbd\xae"));

    // ---- Tab 3: Personalization ----
    QWidget *personalTab = new QWidget();
    QVBoxLayout *personalLayout = new QVBoxLayout(personalTab);
    personalLayout->setContentsMargins(8, 8, 8, 8);
    personalLayout->setSpacing(16);

    QCheckBox *darkMode = new QCheckBox(QString::fromUtf8("\xe6\x9a\x97\xe8\x89\xb2\xe6\xa8\xa1\xe5\xbc\x8f"), this);
    darkMode->setFont(appFont(13));
    darkMode->setChecked(ThemeManager::instance()->isDark());
    connect(darkMode, &QCheckBox::toggled, this, [](bool checked) {
        ThemeManager::instance()->setTheme(checked ? ThemeManager::Dark : ThemeManager::Light);
    });
    personalLayout->addWidget(darkMode);

    m_autoStart = new QCheckBox(QString::fromUtf8("\xe5\xbc\x80\xe6\x9c\xba\xe8\x87\xaa\xe5\x90\xaf"), this);
    m_autoStart->setFont(appFont(13));
    personalLayout->addWidget(m_autoStart);
    personalLayout->addStretch();
    m_tabWidget->addTab(personalTab, QString::fromUtf8("\xe4\xb8\xaa\xe6\x80\xa7\xe5\x8c\x96"));

    // ---- Tab 4: Cloud Sync ----
    QWidget *cloudTab = new QWidget();
    QVBoxLayout *cloudLayout = new QVBoxLayout(cloudTab);
    cloudLayout->setContentsMargins(8, 8, 8, 8);
    cloudLayout->setSpacing(16);

    m_linewebEnabled = new QCheckBox(QString::fromUtf8("\xe5\x90\xaf\xe7\x94\xa8\xe6\x8e\xa8\xe9\x80\x81"), this);
    m_linewebEnabled->setFont(appFont(13));
    cloudLayout->addWidget(m_linewebEnabled);

    QHBoxLayout *endpointRow = new QHBoxLayout();
    endpointRow->addWidget(new QLabel(QString::fromUtf8("API \xe5\x9c\xb0\xe5\x9d\x80:"), this));
    m_linewebEndpoint = new QLineEdit(this);
    m_linewebEndpoint->setPlaceholderText("https://your-server.com（\xe4\xbb\x85\xe5\x9f\xba\xe7\xa1\x80\xe5\x9c\xb0\xe5\x9d\x80\xef\xbc\x8c\xe6\x97\xa0\xe9\x9c\x80\xe5\x90\x8e\xe7\xbc\x80\xe8\xb7\xaf\xe5\xbe\x84\xef\xbc\x89");
    endpointRow->addWidget(m_linewebEndpoint, 1);
    cloudLayout->addLayout(endpointRow);

    QHBoxLayout *tokenRow = new QHBoxLayout();
    tokenRow->addWidget(new QLabel("Token:", this));
    m_linewebToken = new QLineEdit(this);
    m_linewebToken->setEchoMode(QLineEdit::Password);
    m_linewebToken->setPlaceholderText("st_...");
    tokenRow->addWidget(m_linewebToken, 1);
    m_linewebTokenToggle = new QPushButton(QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba"), this);
    connect(m_linewebTokenToggle, &QPushButton::clicked, this, [this]() {
        if (m_linewebToken->echoMode() == QLineEdit::Password) {
            m_linewebToken->setEchoMode(QLineEdit::Normal);
            m_linewebTokenToggle->setText(QString::fromUtf8("\xe9\x9a\x90\xe8\x97\x8f"));
        } else {
            m_linewebToken->setEchoMode(QLineEdit::Password);
            m_linewebTokenToggle->setText(QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba"));
        }
    });
    tokenRow->addWidget(m_linewebTokenToggle);
    cloudLayout->addLayout(tokenRow);

    QHBoxLayout *intervalRow = new QHBoxLayout();
    intervalRow->addWidget(new QLabel(QString::fromUtf8("\xe6\x8e\xa8\xe9\x80\x81\xe9\x97\xb4\xe9\x9a\x94\xef\xbc\x88\xe5\x88\x86\xe9\x92\x9f\xef\xbc\x89:"), this));
    m_linewebInterval = new QSpinBox(this);
    m_linewebInterval->setRange(5, 30);
    m_linewebInterval->setValue(10);
    intervalRow->addWidget(m_linewebInterval);
    intervalRow->addStretch();
    cloudLayout->addLayout(intervalRow);

    QHBoxLayout *testRow = new QHBoxLayout();
    m_linewebTestBtn = new QPushButton(QString::fromUtf8("\xe8\xbf\x9e\xe6\x8e\xa5\xe6\xb5\x8b\xe8\xaf\x95"), this);
    connect(m_linewebTestBtn, &QPushButton::clicked, this, [this]() {
        QString token = m_linewebToken->text().trimmed();
        QString endpoint = m_linewebEndpoint->text().trimmed();
        if (token.isEmpty() || endpoint.isEmpty()) {
            QMessageBox::warning(this,
                QString::fromUtf8("\xe9\x85\x8d\xe7\xbd\xae\xe4\xb8\x8d\xe5\xae\x8c\xe6\x95\xb4"),
                QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe5\xa1\xab\xe5\x86\x99 API \xe5\x9c\xb0\xe5\x9d\x80\xe5\x92\x8c Token"));
            return;
        }

        QJsonObject body;
        body["totalSeconds"] = m_db->getTodayTotal();
        body["date"] = QDate::currentDate().toString("yyyy-MM-dd");

        QUrl url(normalizeEndpoint(endpoint) + "/api/health/push");
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req.setRawHeader("X-Screen-Time-Token", token.toUtf8());

        QNetworkAccessManager *nam = new QNetworkAccessManager(this);
        QNetworkReply *reply = nam->post(req, QJsonDocument(body).toJson());
        connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
            reply->deleteLater();
            nam->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                QMessageBox::information(this,
                    QString::fromUtf8("\xe6\xb5\x8b\xe8\xaf\x95\xe6\x88\x90\xe5\x8a\x9f"),
                    QString::fromUtf8("\xe8\xbf\x9e\xe6\x8e\xa5\xe6\xb5\x8b\xe8\xaf\x95\xe6\x88\x90\xe5\x8a\x9f\xef\xbc\x81"));
            } else {
                QMessageBox::warning(this,
                    QString::fromUtf8("\xe6\xb5\x8b\xe8\xaf\x95\xe5\xa4\xb1\xe8\xb4\xa5"),
                    QString::fromUtf8("\xe8\xbf\x9e\xe6\x8e\xa5\xe5\xa4\xb1\xe8\xb4\xa5\xef\xbc\x9a") + reply->errorString());
            }
        });
    });
    testRow->addWidget(m_linewebTestBtn);
    m_linewebStatus = new QLabel(this);
    m_linewebStatus->setStyleSheet(QString("color: %1; font-size: 12px;").arg(DesignTokens::kTextMute().name()));
    testRow->addWidget(m_linewebStatus);
    testRow->addStretch();
    cloudLayout->addLayout(testRow);

    cloudLayout->addStretch();
    m_tabWidget->addTab(cloudTab, QString::fromUtf8("\xe4\xba\x91\xe7\xab\xaf\xe5\x90\x8c\xe6\xad\xa5"));

    // ---- Tab 5: About ----
    QWidget *aboutTab = new QWidget();
    QVBoxLayout *aboutLayout = new QVBoxLayout(aboutTab);
    aboutLayout->setContentsMargins(8, 8, 8, 8);
    aboutLayout->setAlignment(Qt::AlignCenter);

    // 应用名称
    QLabel *appNameLabel = new QLabel(QString::fromUtf8("Time Master"), aboutTab);
    appNameLabel->setFont(appFont(24, QFont::Bold));
    appNameLabel->setAlignment(Qt::AlignCenter);
    appNameLabel->setStyleSheet(QString("color: %1;").arg(DesignTokens::kTextStrong().name()));
    aboutLayout->addWidget(appNameLabel);

    // 版本号
    QLabel *versionLabel = new QLabel(
        QString::fromUtf8("v") + QApplication::applicationVersion(), aboutTab);
    versionLabel->setFont(appFont(14));
    versionLabel->setAlignment(Qt::AlignCenter);
    versionLabel->setStyleSheet(QString("color: %1;").arg(DesignTokens::kTextMute().name()));
    aboutLayout->addWidget(versionLabel);

    // 间隔
    aboutLayout->addSpacing(24);

    // 描述
    QLabel *descLabel = new QLabel(
        QString::fromUtf8("Windows \xe6\xa1\x8c\xe9\x9d\xa2\xe6\x97\xb6\xe9\x97\xb4\xe8\xbf\xbd\xe8\xb8\xaa\xe5\xb7\xa5\xe5\x85\xb7"), aboutTab);
    descLabel->setFont(appFont(12));
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setStyleSheet(QString("color: %1;").arg(DesignTokens::kTextFaint().name()));
    aboutLayout->addWidget(descLabel);

    aboutLayout->addStretch();

    m_tabWidget->addTab(aboutTab, QString::fromUtf8("\xe5\x85\xb3\xe4\xba\x8e"));

    // ---- Bottom Buttons ----
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    QPushButton *cancelBtn = new QPushButton(QString::fromUtf8("\xe5\x8f\x96\xe6\xb6\x88"), this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    QPushButton *saveBtn = new QPushButton(QString::fromUtf8("\xe4\xbf\x9d\xe5\xad\x98"), this);
    saveBtn->setStyleSheet(
        QString("QPushButton { background-color: %1; color: white; border: none; border-radius: 6px; "
                "padding: 6px 16px; font-size: 13px; }"
                "QPushButton:hover { background-color: %2; }"
                "QPushButton:pressed { background-color: %3; }")
            .arg(DesignTokens::kAccent().name(), DesignTokens::kAccentHover().name(), DesignTokens::kAccentPressed().name()));
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
        setStyleSheet(
            QString("SettingsDialog { background-color: %1; }"
                    "QTabWidget::pane { background: %1; border: none; }"
                    "QTabBar::tab { padding: 8px 20px; font-size: 13px; }")
                .arg(DesignTokens::kBg().name()));
    });

    loadSettings();
}

void SettingsDialog::loadSettings()
{
    m_trackingEnabled->setChecked(m_db->getSetting("tracking_enabled", "true") == "true");
    m_pollInterval->setValue(m_db->getSetting("poll_interval", "1").toInt());
    m_idleThreshold->setValue(m_db->getSetting("idle_threshold", "60").toInt());
    m_minTrackingSeconds->setValue(m_db->getSetting("min_tracking_seconds", "0").toInt());
    m_minRecordThreshold->setValue(m_db->getSetting("min_record_threshold", "40").toInt());
    m_autoStart->setChecked(m_db->getSetting("auto_start", "false") == "true");
    refreshKnownAppsList();
    refreshIgnoredList();
    refreshAliasTable();

    m_linewebEnabled->setChecked(m_db->getSetting("lineweb_enabled", "false") == "true");
    m_linewebEndpoint->setText(m_db->getSetting("lineweb_endpoint", ""));
    m_linewebToken->setText(m_db->getSetting("lineweb_token", ""));
    m_linewebInterval->setValue(m_db->getSetting("lineweb_interval", "10").toInt());
    QString lastPush = m_db->getSetting("lineweb_last_push", "");
    m_linewebStatus->setText(lastPush.isEmpty()
        ? QString::fromUtf8("\xe5\xb0\x9a\xe6\x9c\xaa\xe6\x8e\xa8\xe9\x80\x81")
        : lastPush);
}

void SettingsDialog::saveSettings()
{
    m_db->setSetting("tracking_enabled", m_trackingEnabled->isChecked() ? "true" : "false");
    m_db->setSetting("poll_interval", QString::number(m_pollInterval->value()));
    m_db->setSetting("idle_threshold", QString::number(m_idleThreshold->value()));
    m_db->setSetting("min_tracking_seconds", QString::number(m_minTrackingSeconds->value()));
    m_db->setSetting("min_record_threshold", QString::number(m_minRecordThreshold->value()));
    m_db->setSetting("auto_start", m_autoStart->isChecked() ? "true" : "false");
    AutoStartHelper::setAutoStart(m_autoStart->isChecked());

    m_db->setSetting("lineweb_enabled", m_linewebEnabled->isChecked() ? "true" : "false");
    m_db->setSetting("lineweb_endpoint", m_linewebEndpoint->text().trimmed());
    m_db->setSetting("lineweb_token", m_linewebToken->text().trimmed());
    m_db->setSetting("lineweb_interval", QString::number(m_linewebInterval->value()));
}

void SettingsDialog::refreshKnownAppsList()
{
    m_knownAppsList->clear();
    m_knownSearch->clear();

    QStringList processNames = m_db->getAllKnownProcessNames();
    QMap<int, QString> ignored = m_db->getIgnoredApps();
    QSet<QString> ignoredNames;
    for (auto it = ignored.begin(); it != ignored.end(); ++it)
        ignoredNames.insert(it.value());

    QMap<QString, QString> aliases = m_db->getAppAliases();

    for (const QString &path : processNames) {
        QString name = friendlyName(path, aliases);
        QIcon icon = AppIconProvider::instance()->icon(path, 20);
        QListWidgetItem *item = new QListWidgetItem(icon, name);
        item->setData(Qt::UserRole, path);
        if (ignoredNames.contains(path))
            item->setForeground(DesignTokens::kTextFaint());
        m_knownAppsList->addItem(item);
    }
}

void SettingsDialog::refreshIgnoredList()
{
    m_ignoredAppsList->clear();
    m_ignoredSearch->clear();

    QMap<int, QString> ignored = m_db->getIgnoredApps();
    QMap<QString, QString> aliases = m_db->getAppAliases();

    for (auto it = ignored.begin(); it != ignored.end(); ++it) {
        QString path = it.value();
        QString name = friendlyName(path, aliases);
        QIcon icon = AppIconProvider::instance()->icon(path, 20);
        QListWidgetItem *item = new QListWidgetItem(icon, name);
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
    QMap<QString, QString> aliases = m_db->getAppAliases();
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
        QString processPath = item->data(Qt::UserRole).toString();
        m_db->addIgnoredApp(processPath);
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
    int id = item->data(Qt::UserRole).toInt();
    m_db->removeIgnoredApp(id);
    refreshIgnoredList();
    refreshKnownAppsList();
}

void SettingsDialog::onAddAlias()
{
    bool ok;
    QString processName = QInputDialog::getText(this,
        QString::fromUtf8("\xe6\xb7\xbb\xe5\x8a\xa0\xe5\x88\xab\xe5\x90\x8d"),
        QString::fromUtf8("\xe8\xbf\x9b\xe7\xa8\x8b\xe5\x90\x8d (e.g. code.exe):"),
        QLineEdit::Normal, "", &ok);
    if (!ok || processName.isEmpty()) return;

    QString displayName = QInputDialog::getText(this,
        QString::fromUtf8("\xe6\xb7\xbb\xe5\x8a\xa0\xe5\x88\xab\xe5\x90\x8d"),
        QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba\xe5\x90\x8d:"),
        QLineEdit::Normal, "", &ok);
    if (!ok || displayName.isEmpty()) return;

    m_db->setAppAlias(processName, displayName);
    refreshAliasTable();
}

void SettingsDialog::onEditAlias()
{
    int row = m_aliasTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this,
            QString::fromUtf8("\xe6\xb2\xa1\xe6\x9c\x89\xe9\x80\x89\xe6\x8b\xa9"),
            QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe9\x80\x89\xe6\x8b\xa9\xe8\xa6\x81\xe7\xbc\x96\xe8\xbe\x91\xe7\x9a\x84\xe5\x88\xab\xe5\x90\x8d"));
        return;
    }
    QString processName = m_aliasTable->item(row, 0)->text();
    QString currentDisplay = m_aliasTable->item(row, 1)->text();
    bool ok;
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
    int row = m_aliasTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this,
            QString::fromUtf8("\xe6\xb2\xa1\xe6\x9c\x89\xe9\x80\x89\xe6\x8b\xa9"),
            QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe9\x80\x89\xe6\x8b\xa9\xe8\xa6\x81\xe5\x88\xa0\xe9\x99\xa4\xe7\x9a\x84\xe5\x88\xab\xe5\x90\x8d"));
        return;
    }
    QString processName = m_aliasTable->item(row, 0)->text();
    m_db->removeAppAliasByProcessName(processName);
    refreshAliasTable();
}
