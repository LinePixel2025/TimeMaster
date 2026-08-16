#include "ui/main_window.h"
#include "database/database_manager.h"
#include "ai/ai_client.h"
#include "ui/settings_dialog.h"
#include "ui/settings_icons.h"
#include "ui/theme_manager.h"
#include "ui/design_tokens.h"
#include "ui/hero_card.h"
#include "ui/trend_card.h"
#include "ui/rank_card.h"
#include "ui/compare_card.h"
#include "ui/ai_report_card.h"
#include "export/exporter.h"

#include <QCloseEvent>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QWidget>
#include <QApplication>
#include <QLabel>
#include <QScreen>
#include <QGuiApplication>
#include <QDate>

#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

static const DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
static const DWORD DWMWA_CAPTION_COLOR = 35;

static void applyDwmTitleBar(HWND hwnd, bool dark)
{
    BOOL dwmDark = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dwmDark, sizeof(dwmDark));
    QColor bg = DesignTokens::kBg();
    COLORREF color = RGB(bg.red(), bg.green(), bg.blue());
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &color, sizeof(color));
}

MainWindow::MainWindow(DatabaseManager *db, AiClient *ai, QWidget *parent)
    : QMainWindow(parent), m_db(db)
{
    setWindowTitle("Time Master");
    // Four dashboard cards plus the AI report card need this stable floor to
    // keep text and charts readable while the user resizes the window.
    setMinimumSize(900, 700);

    // Qt 返回逻辑像素，直接按可用工作区计算默认尺寸。
    const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
    const QSize defSize(qMin(960, qRound(avail.width() * 0.80)),
                        qMin(740, qRound(avail.height() * 0.82)));
    resize(defSize);

    QColor bg = DesignTokens::kBg();
    QColor textStrong = DesignTokens::kTextStrong();

    QPalette pal = palette();
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::Base, DesignTokens::kSurface());
    pal.setColor(QPalette::Text, textStrong);
    pal.setColor(QPalette::WindowText, textStrong);
    setPalette(pal);

    auto *central = new QWidget(this);
    central->setObjectName("centralWidget");
    central->setStyleSheet(QString("#centralWidget { background-color: %1; }").arg(bg.name()));
    setCentralWidget(central);

    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 16, 24, 22);
    layout->setSpacing(18);

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(8);

    auto *titleColumn = new QVBoxLayout();
    titleColumn->setSpacing(1);
    m_titleLabel = new QLabel("Time Master", central);
    m_titleLabel->setFont(DesignTokens::appFont(20, QFont::DemiBold));
    titleColumn->addWidget(m_titleLabel);
    m_dateLabel = new QLabel(QDate::currentDate().toString(QString::fromUtf8("yyyy年M月d日  dddd")), central);
    m_dateLabel->setFont(DesignTokens::appFont(10));
    titleColumn->addWidget(m_dateLabel);
    headerLayout->addLayout(titleColumn);
    headerLayout->addStretch();

    m_themeBtn = new QPushButton(
        ThemeManager::instance()->isDark()
            ? QString::fromUtf8("\xe2\x98\x80")
            : QString::fromUtf8("\xf0\x9f\x8c\x99"),
        central);
    m_themeBtn->setFixedSize(36, 36);
    m_themeBtn->setToolTip(
        ThemeManager::instance()->isDark()
            ? QString::fromUtf8("\xe5\x88\x87\xe6\x8d\xa2\xe5\x88\xb0\xe4\xba\xae\xe8\x89\xb2\xe6\xa8\xa1\xe5\xbc\x8f")
            : QString::fromUtf8("\xe5\x88\x87\xe6\x8d\xa2\xe5\x88\xb0\xe6\x9a\x97\xe8\x89\xb2\xe6\xa8\xa1\xe5\xbc\x8f"));
    connect(m_themeBtn, &QPushButton::clicked, this, []() {
        ThemeManager::instance()->toggle();
    });
    headerLayout->addWidget(m_themeBtn);

    m_settingsBtn = new QPushButton(central);
    m_settingsBtn->setFixedSize(36, 36);
    m_settingsBtn->setIcon(SettingsIcons::icon(SettingsIcons::Gear, 18));
    m_settingsBtn->setIconSize(QSize(18, 18));
    m_settingsBtn->setToolTip(QString::fromUtf8("设置"));
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettings);
    headerLayout->addWidget(m_settingsBtn);

    m_exportBtn = new QPushButton(QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe8\xae\xb0\xe5\xbd\x95"), central);
    m_exportBtn->setMinimumHeight(36);
    connect(m_exportBtn, &QPushButton::clicked, this, &MainWindow::onExport);
    headerLayout->addWidget(m_exportBtn);

    m_cloudSyncBtn = new QPushButton(QString::fromUtf8("\xe4\xba\x91\xe7\xab\xaf\xe5\x90\x8c\xe6\xad\xa5"), central);
    m_cloudSyncBtn->setMinimumHeight(36);
    m_cloudSyncBtn->setToolTip(QString::fromUtf8("\xe7\xab\x8b\xe5\x8d\xb3\xe6\x8e\xa8\xe9\x80\x81\xe4\xbb\x8a\xe6\x97\xa5\xe6\x97\xb6\xe9\x95\xbf\xe5\xb9\xb6\xe5\x90\x8c\xe6\xad\xa5\xe4\xba\x91\xe7\xab\xaf\xe7\x9b\xae\xe6\xa0\x87"));
    connect(m_cloudSyncBtn, &QPushButton::clicked, this, &MainWindow::cloudSyncRequested);
    headerLayout->addWidget(m_cloudSyncBtn);

    m_refreshBtn = new QPushButton(QString::fromUtf8("\xe5\x88\xb7\xe6\x96\xb0"), central);
    m_refreshBtn->setMinimumHeight(36);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshData);
    headerLayout->addWidget(m_refreshBtn);

    layout->addLayout(headerLayout);

    auto *grid = new QGridLayout();
    grid->setSpacing(12);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    m_heroCard = new HeroCard(central);
    m_trendCard = new TrendCard(central);
    m_compareCard = new CompareCard(central);
    m_rankCard = new RankCard(central);

    grid->addWidget(m_heroCard, 0, 0);
    grid->addWidget(m_trendCard, 0, 1);
    grid->addWidget(m_compareCard, 1, 0);
    grid->addWidget(m_rankCard, 1, 1);
    grid->setRowStretch(0, 5);
    grid->setRowStretch(1, 5);

    m_aiCard = new AiReportCard(ai, central);
    grid->addWidget(m_aiCard, 2, 0, 1, 2); // 第 3 行跨两列
    grid->setRowStretch(2, 4);

    layout->addLayout(grid, 1);

    connect(m_aiCard, &AiReportCard::generateRequested, this, [this]() {
        emit aiReportRequested(AiPeriod::daily());
    });
    connect(m_aiCard, &AiReportCard::dailyReportOpenRequested,
            this, &MainWindow::dailyReportOpenRequested);
    connect(m_aiCard, &AiReportCard::weeklyReportOpenRequested,
            this, &MainWindow::weeklyReportOpenRequested);
    connect(m_aiCard, &AiReportCard::weeklyReportGenerateRequested,
            this, &MainWindow::weeklyReportGenerateRequested);
    connect(m_aiCard, &AiReportCard::weeklyReportRegenerateRequested,
            this, &MainWindow::weeklyReportRegenerateRequested);
    // 设置保存后重读 AI 配置与缓存（与 settingsChanged 的 main 端接线配合）。
    connect(this, &MainWindow::settingsChanged, this, [this]() {
        m_aiCard->reloadState();
    });

    connect(m_trendCard, &TrendCard::chartTypeChanged, this,
            [this](const QString &type) {
        m_db->setSetting("chart_type", type);
    });

    connect(m_trendCard, &TrendCard::heatmapPeriodChanged, this,
            [this](const QString &period) {
        m_db->setSetting("trend_heatmap_period", period);
        refreshData();
    });

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(10000);
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::refreshData);
    m_refreshTimer->start();

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme theme) {
        const bool dark = (theme == ThemeManager::Dark);
        m_themeBtn->setText(dark ? QString::fromUtf8("\xe2\x98\x80")
                                 : QString::fromUtf8("\xf0\x9f\x8c\x99"));
        m_themeBtn->setToolTip(
            dark
                ? QString::fromUtf8("\xe5\x88\x87\xe6\x8d\xa2\xe5\x88\xb0\xe4\xba\xae\xe8\x89\xb2\xe6\xa8\xa1\xe5\xbc\x8f")
                : QString::fromUtf8("\xe5\x88\x87\xe6\x8d\xa2\xe5\x88\xb0\xe6\x9a\x97\xe8\x89\xb2\xe6\xa8\xa1\xe5\xbc\x8f"));

        applyTheme();

        if (isVisible()) {
            applyDwmTitleBar(reinterpret_cast<HWND>(winId()), dark);
        }
    });

    applyTheme();

    // 读回柱状/折线视图偏好（此前只写不读，重启会回落到柱状）。
    m_trendCard->setChartType(m_db->getSetting("chart_type", "bar"));

    refreshData();
}

void MainWindow::applyTheme()
{
    const QColor bg = DesignTokens::kBg();
    QPalette pal = palette();
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::Base, DesignTokens::kSurface());
    pal.setColor(QPalette::Text, DesignTokens::kTextStrong());
    pal.setColor(QPalette::WindowText, DesignTokens::kTextStrong());
    setPalette(pal);
    centralWidget()->setStyleSheet(
        QString("#centralWidget { background-color: %1; }").arg(bg.name()));

    m_titleLabel->setStyleSheet(QString("color: %1; background: transparent;")
        .arg(DesignTokens::kTextStrong().name()));
    m_dateLabel->setStyleSheet(QString("color: %1; background: transparent;")
        .arg(DesignTokens::kTextMute().name()));

    const QString iconStyle = QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid transparent;"
        " border-radius: 6px; font-size: 16px; padding: 0; }"
        "QPushButton:hover { background: %2; border-color: %3; }"
        "QPushButton:focus { border-color: %4; }")
        .arg(DesignTokens::kTextMute().name(),
             DesignTokens::kButtonHoverBg().name(),
             DesignTokens::kBorder().name(),
             DesignTokens::kAccent().name());
    m_themeBtn->setStyleSheet(iconStyle);
    m_settingsBtn->setStyleSheet(iconStyle);
    m_settingsBtn->setIcon(SettingsIcons::icon(
        SettingsIcons::Gear, 18, DesignTokens::kTextMute()));

    const QString secondaryStyle = QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 6px; padding: 0 16px; font-size: 12px; }"
        "QPushButton:hover { background: %4; }"
        "QPushButton:focus { border-color: %5; }")
        .arg(DesignTokens::kSurface().name(), DesignTokens::kText().name(),
             DesignTokens::kBorder().name(), DesignTokens::kButtonHoverBg().name(),
             DesignTokens::kAccent().name());
    m_exportBtn->setStyleSheet(secondaryStyle);

    const QString primaryStyle = QString(
        "QPushButton { background: %1; color: white; border: 1px solid %1;"
        " border-radius: 6px; padding: 0 18px; font-size: 12px; font-weight: 600; }"
        "QPushButton:hover { background: %2; border-color: %2; }"
        "QPushButton:pressed { background: %3; border-color: %3; }")
        .arg(DesignTokens::kAccent().name(), DesignTokens::kAccentHover().name(),
             DesignTokens::kAccentPressed().name());
    m_refreshBtn->setStyleSheet(primaryStyle);
    m_cloudSyncBtn->setStyleSheet(primaryStyle);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    applyDwmTitleBar(reinterpret_cast<HWND>(winId()),
                     ThemeManager::instance()->isDark());
}

void MainWindow::refreshData()
{
    const int todayTotal = m_db->getTodayTotal();
    const int yesterdayTotal = m_db->getYesterdayTotal();
    const int dailyGoal = m_db->getSetting("daily_goal", "28800").toInt();

    m_heroCard->setData(todayTotal, yesterdayTotal, dailyGoal);

    const QString format = m_db->getSetting("trend_display_format", "normal");
    m_trendCard->setDisplayFormat(format);
    m_trendCard->setData(m_db->getWeekSummary());
    if (format == QStringLiteral("heatmap")) {
        m_trendCard->setMonthData(m_db->getMonthSummary());
        m_trendCard->setHeatmapPeriod(
            m_db->getSetting("trend_heatmap_period", "week"));
    }

    m_rankCard->refresh(m_db->getAppRank());
    m_compareCard->setData(todayTotal, yesterdayTotal);
}

void MainWindow::onAiReportReady(const QString &, const QString &text)
{
    m_aiCard->setReport(text);
}

void MainWindow::onAiReportFailed(const QString &, const QString &error)
{
    m_aiCard->showError(error);
}

void MainWindow::onWeeklyReportReady(const QString &path)
{
    m_aiCard->setWeeklyReportPath(path);
}

void MainWindow::onExport()
{
    QStringList formats;
    formats << QString::fromUtf8("CSV (.csv)")
            << QString::fromUtf8("Excel (.xlsx)");
    bool ok = false;
    QString fmt = QInputDialog::getItem(this,
        QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe6\xa0\xbc\xe5\xbc\x8f"),
        QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe5\xaf\xbc\xe5\x87\xba\xe6\xa0\xbc\xe5\xbc\x8f:"),
        formats, 0, false, &ok);
    if (!ok) return;

    QString filter = fmt.contains("CSV")
        ? QString::fromUtf8("CSV \xe6\x96\x87\xe4\xbb\xb6 (*.csv)")
        : QString::fromUtf8("Excel \xe6\x96\x87\xe4\xbb\xb6 (*.xlsx)");
    QString path = QFileDialog::getSaveFileName(this,
        QString::fromUtf8("\xe4\xbf\x9d\xe5\xad\x98\xe6\x96\x87\xe4\xbb\xb6"),
        QString(), filter);
    if (path.isEmpty()) return;

    try {
        Exporter exporter(m_db);
        bool success = fmt.contains("CSV")
            ? exporter.exportCsv(path)
            : exporter.exportExcel(path);

        if (success) {
            QMessageBox::information(this,
                QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe6\x88\x90\xe5\x8a\x9f"),
                QString::fromUtf8("\xe8\xae\xb0\xe5\xbd\x95\xe5\xb7\xb2\xe5\xaf\xbc\xe5\x87\xba\xe5\x88\xb0:\n") + path);
        } else {
            QMessageBox::critical(this,
                QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe5\xa4\xb1\xe8\xb4\xa5"),
                QString::fromUtf8("\xe6\x97\xa0\xe6\xb3\x95\xe5\x86\x99\xe5\x85\xa5\xe6\x96\x87\xe4\xbb\xb6"));
        }
    } catch (const std::exception &e) {
        QMessageBox::critical(this,
            QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe5\xa4\xb1\xe8\xb4\xa5"),
            e.what());
    }
}

void MainWindow::onSettings()
{
    SettingsDialog dialog(m_db, this);
    if (dialog.exec() == QDialog::Accepted) {
        refreshData();
        emit settingsChanged();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
}
